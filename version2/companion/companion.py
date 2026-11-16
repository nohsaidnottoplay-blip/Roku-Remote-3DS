"""Local theme catalog and offline dictation for Roku Remote 3DS. Python 3.10+."""
from argparse import ArgumentParser
from array import array
from configparser import ConfigParser, Error as IniError
from hashlib import sha256
from hmac import compare_digest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path, PurePosixPath
from threading import RLock
from urllib.parse import urlsplit
import io, json, os, re, secrets, socket, sys, wave, zipfile

MIB=1024*1024
class Invalid(ValueError): pass

def inspect_theme(data):
    if not 1<=len(data)<=8*MIB: raise Invalid("ZIP limit: 8 MiB.")
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            members=z.infolist(); names={}; manifests=[]; total=0
            if not 1<=len(members)<=128: raise Invalid("ZIP limit: 128 entries.")
            for f in members:
                n=f.orig_filename
                if (not n or len(n.encode())>127 or n.startswith("/") or "\\" in n or ":" in n
                    or any(ord(c)<32 or ord(c)==127 for c in n)
                    or any(p in ("",".","..") for p in n.rstrip("/").split("/"))):
                    raise Invalid("Invalid member path.")
                k=n.casefold()
                if k in names: raise Invalid("Duplicate member.")
                names[k]=f
                if f.is_dir() or n.startswith("__MACOSX/"): continue
                if f.file_size<=4*MIB: total+=f.file_size
                if PurePosixPath(n).name.lower() in ("theme.json","theme.ini"): manifests.append(f)
            if total>32*MIB or len(manifests)!=1: raise Invalid("Provide a single theme.json or theme.ini.")
            mf=manifests[0]
            if not 1<=mf.file_size<=16384 or mf.flag_bits&1: raise Invalid("Manifest too large or encrypted.")
            raw=z.read(mf).decode("utf-8-sig")
            if mf.filename.lower().endswith(".ini"):
                ini=ConfigParser(interpolation=None); ini.optionxform=str
                ini.read_string(raw if raw.lstrip().startswith("[") else "[theme]\n"+raw)
                m=dict(ini["theme"]) if "theme" in ini else {}
                for k in ("colors","images","sounds"):
                    if k in ini:m[k]=dict(ini[k])
            else:m=json.loads(raw)
            if not isinstance(m,dict) or str(m.get("format",2))!="2" or any(k in m and not isinstance(m[k],dict) for k in ("colors","images","sounds")):
                raise Invalid("Unsupported manifest.")
            def field(k,default,n):
                val=m.get(k);return " ".join(val.split())[:n] if isinstance(val,str) and val.strip() else default
            meta=dict(id=sha256(data).hexdigest(),name=field("name","Untitled theme",60),
                      creator=field("creator","Unlisted creator",60),category=field("category","Other",32),
                      description=field("description","No description supplied.",240),screenshot=True)
            from PIL import Image,ImageDraw
            im=None
            prefix=mf.filename[:-len(PurePosixPath(mf.filename).name)]
            path=m.get("images",{}).get("screenshot",m.get("screenshot","screenshot.png"))
            f=names.get((prefix+path).casefold()) if isinstance(path,str) else None
            if f and f.file_size<=4*MIB and not f.flag_bits&1:
                try:
                    with Image.open(io.BytesIO(z.read(f))) as a:
                        if a.format in ("PNG","JPEG","WEBP","GIF") and a.width<=512 and a.height<=256:
                            im=a.convert("RGB");im.thumbnail((320,200))
                except (OSError,ValueError,RuntimeError,zipfile.BadZipFile):pass
            if im is None:
                def col(k,d):
                    v=m.get("colors",{}).get(k)
                    return "#"+v.lstrip("#") if isinstance(v,str) and re.fullmatch(r"#?[0-9a-fA-F]{6}",v) else d
                im=Image.new("RGB",(320,176),col("bottom","#211A30"));draw=ImageDraw.Draw(im)
                draw.text((15,14),"PALETTE PREVIEW",fill=col("highlight","#CEB2FF"))
                draw.text((15,42),meta["name"][:38],fill=col("text","#FFFFFF"))
                for x,txt in ((12,"Back"),(114,"Home"),(216,"OK")):
                    draw.rounded_rectangle((x,82,x+90,126),radius=7,fill=col("button","#484057"))
                    draw.text((x+14,98),txt,fill=col("button_text","#FFFFFF"))
            buf=io.BytesIO();im.save(buf,"PNG");return meta,buf.getvalue()
    except Invalid:raise
    except (ValueError,OSError,RuntimeError,KeyError,IniError,zipfile.BadZipFile,UnicodeError,NotImplementedError) as exc:
        raise Invalid("Unreadable theme ZIP or manifest.") from exc

class Catalog:
    def __init__(self,root):
        self.root=Path(root);self.root.mkdir(parents=True,exist_ok=True)
        self.lock=RLock();self.themes={}
        for p in sorted(self.root.glob("*.zip"))[:100]:
            if p.stat().st_size<=8*MIB:
                try:self.add(p.read_bytes())
                except Invalid:pass
    def add(self,data):
        meta,shot=inspect_theme(data);ident=meta["id"]
        with self.lock:
            if ident not in self.themes and len(self.themes)>=100:raise Invalid("Catalog full (100 themes).")
            p=self.root/(ident+".zip")
            if not p.exists():
                tmp=p.with_suffix(".part");tmp.write_bytes(data);os.replace(tmp,p)
            self.themes[ident]=(meta,shot)
        return meta
    def list(self):
        with self.lock:return sorted((v[0] for v in self.themes.values()),key=lambda m:(m["category"].casefold(),m["name"].casefold()))
    def get(self,ident,kind):
        with self.lock:
            item=self.themes.get(ident)
            return None if item is None else item[1] if kind=="screenshot" else (self.root/(ident+".zip")).read_bytes()

class Speech:
    def __init__(self):
        self.lock=RLock();self.decoder=None
        try:
            from pocketsphinx import Decoder
            self.factory=Decoder
        except ImportError:self.factory=None
    def transcribe(self,b):
        try:
            with wave.open(io.BytesIO(b),"rb") as w:
                rate=w.getframerate();n=w.getnframes()
                if (w.getnchannels()!=1 or w.getsampwidth()!=2 or w.getcomptype()!="NONE" or not 8000<=rate<=48000 or not rate//10<=n<=rate*24):
                    raise Invalid("Expected 0.1 to 24 seconds of mono 16-bit PCM WAV.")
                raw=w.readframes(n)
                if len(raw)!=2*n:raise Invalid("Truncated WAV.")
        except (wave.Error,EOFError) as exc:raise Invalid("Invalid WAV.") from exc
        if not self.factory:raise Invalid("Install PocketSphinx first.")
        if rate!=16000:
            src=array("h");src.frombytes(raw)
            if sys.byteorder=="big":src.byteswap()
            dst=array("h")
            for i in range(n*16000//rate):
                j,f=divmod(i*rate,16000)
                dst.append((int(src[j])*(16000-f)+int(src[min(j+1,n-1)])*f)//16000)
            if sys.byteorder=="big":dst.byteswap()
            raw=dst.tobytes()
        with self.lock:
            if not self.decoder:self.decoder=self.factory(samprate=16000,loglevel="ERROR")
            self.decoder.start_utt()
            try:self.decoder.process_raw(raw,False,True)
            finally:self.decoder.end_utt()
            hyp=self.decoder.hyp();return hyp.hypstr.strip()[:256] if hyp else ""

def make_server(addr,store,token,speech=None):
    engine=speech if speech is not None else Speech()
    class Handler(BaseHTTPRequestHandler):
        def setup(self):super().setup();self.connection.settimeout(30)
        def send(self,code,obj,kind="application/json"):
            b=obj if isinstance(obj,bytes) else json.dumps(obj,ensure_ascii=False).encode()
            self.send_response(code);self.send_header("Content-Type",kind);self.send_header("Content-Length",str(len(b)));self.send_header("Cache-Control","no-store");self.end_headers();self.wfile.write(b)
        def auth(self):
            ok=compare_digest(self.headers.get("Authorization","").encode(),("Bearer "+token).encode())
            if not ok:self.send(401,{"error":"Pairing code required."})
            return ok
        def do_GET(self):
            if not self.auth():return
            path=urlsplit(self.path).path
            if path=="/api/health":return self.send(200,{"speech":bool(engine.factory),"market":True})
            if path=="/api/themes":return self.send(200,{"themes":store.list()})
            match=re.fullmatch(r"/api/themes/([0-9a-f]{64})/(download|screenshot)",path)
            if match:
                b=store.get(*match.groups())
                if b is not None:return self.send(200,b,"application/zip" if match[2]=="download" else "image/png")
            self.send(404,{"error":"Not found."})
        def do_POST(self):
            if not self.auth():return
            path=urlsplit(self.path).path
            if path not in ("/api/themes","/api/transcribe"):return self.send(404,{"error":"Not found."})
            try:
                n=int(self.headers.get("Content-Length","0"))
                if not 1<=n<=(8*MIB if path=="/api/themes" else MIB):return self.send(413,{"error":"Incorrect body size."})
                b=self.rfile.read(n)
                if len(b)!=n:raise Invalid("Incomplete upload.")
                if path=="/api/themes":return self.send(201,store.add(b))
                trans=engine.transcribe(b)
                self.send(200,{"text":trans,"error":"" if trans else "No speech recognized."})
            except (ValueError,OSError) as exc:self.send(400,{"error":str(exc)})
            except Exception:self.send(500,{"error":"Companion could not process this request."})
        def log_message(self,*args):pass  # Do not log recordings, text, or pairing codes.
    return ThreadingHTTPServer(addr,Handler)

def main():
    a=ArgumentParser(description=__doc__)
    a.add_argument("--lan",action="store_true",help="Allow the 3DS to connect on your local network")
    a.add_argument("--port",type=int,default=8765)
    a.add_argument("--store",type=Path,default=Path.home()/".roku-remote-3ds"/"market")
    a.add_argument("--token");ns=a.parse_args();store=Catalog(ns.store)
    pair=ns.store/"pairing-code.txt"
    if ns.token:token=ns.token
    elif pair.exists():token=pair.read_text().strip()
    else:
        token=secrets.token_hex(5).upper();pair.write_text(token+"\n")
        try:pair.chmod(0o600)
        except OSError:pass
    if not token or len(token)>96 or not token.isascii() or not token.isprintable():a.error("Use a printable ASCII pairing code (1–96 characters).")
    seed=Path(__file__).resolve().parent/"themes"/"Tidepool-Version2.zip"
    if seed.exists():store.add(seed.read_bytes())
    host="127.0.0.1"
    if ns.lan:
        try:
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:sock.connect(("192.0.2.1",9));host=sock.getsockname()[0]
        except OSError:host=socket.gethostname()
    srv=make_server(("0.0.0.0" if ns.lan else host,ns.port),store,token)
    print(f"Companion address: http://{host}:{ns.port}\nPairing code: {token}\nPress Ctrl+C to stop.",flush=True)
    try:srv.serve_forever()
    except KeyboardInterrupt:pass
    finally:srv.server_close()
if __name__=="__main__":main()

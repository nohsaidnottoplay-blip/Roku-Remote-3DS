"""Exercise the real HTTP server, image codecs, WAV input and ZIP repository."""
import io,json,os,sys,tempfile,threading,unittest,urllib.request,urllib.error,wave,zipfile
from hashlib import sha256
from pathlib import Path
from PIL import Image
BASE=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(BASE/"companion"),str(BASE/"examples")]
import companion as app
from build_tidepool import build

class RoundTrip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();root=Path(cls.temp.name)
        cls.zip=build(root/"example.zip").read_bytes();cls.encoded=sha256(cls.zip).hexdigest()
        cls.store=app.Catalog(root/"catalog");cls.srv=app.make_server(("127.0.0.1",0),cls.store,"LOCAL")
        cls.worker=threading.Thread(target=cls.srv.serve_forever,daemon=True);cls.worker.start()
        cls.base="http://127.0.0.1:"+str(cls.srv.server_port)
    @classmethod
    def tearDownClass(cls):cls.srv.shutdown();cls.srv.server_close();cls.worker.join();cls.temp.cleanup()
    def req(self,path,body=None,code="LOCAL"):
        r=urllib.request.Request(self.base+path,data=body,headers={"Authorization":"Bearer "+code})
        with urllib.request.urlopen(r,timeout=30) as resp:return resp.read()
    def test_01_authentication(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.req("/api/themes",code="WRONG")
        self.assertEqual(e.exception.code,401)
    def test_02_share_browse_preview_download_reload(self):
        meta=json.loads(self.req("/api/themes",self.zip))
        self.assertEqual((meta["id"],meta["category"],meta["creator"]),(self.encoded,"Nature","Roku Remote 3DS project"))
        self.assertIn(meta,json.loads(self.req("/api/themes"))["themes"])
        prefix="/api/themes/"+self.encoded
        with Image.open(io.BytesIO(self.req(prefix+"/screenshot"))) as im:self.assertEqual(im.format,"PNG")
        self.assertEqual(self.req(prefix+"/download"),self.zip)
        second=app.Catalog(self.store.root);self.assertEqual(second.get(self.encoded,"download"),self.zip)
    def test_03_missing_optional_preview(self):
        b=io.BytesIO()
        with zipfile.ZipFile(b,"w") as z:
            z.writestr("my/theme.json",json.dumps({"name":"Broken optional art","images":{"screenshot":"bad.png"}}));z.writestr("my/bad.png",b"broken")
        m,p=app.inspect_theme(b.getvalue());self.assertEqual(m["name"],"Broken optional art");self.assertTrue(p.startswith(b"\x89PNG"))
    def test_04_reject_invalid_required_manifest(self):
        for n,s in (("theme.json","{"),("../theme.json","{}"),("theme.json",'{"format":999}')):
            b=io.BytesIO()
            with zipfile.ZipFile(b,"w") as z:z.writestr(n,s)
            with self.assertRaises(app.Invalid):app.inspect_theme(b.getvalue())
    def test_05_example_media_really_decode(self):
        with zipfile.ZipFile(io.BytesIO(self.zip)) as z:
            for n,fmt,animated in (("art/top.png","PNG",False),("art/bottom.jpg","JPEG",False),("buttons/ok.gif","GIF",True),("buttons/home.webp","WEBP",True),("art/tide.webp","WEBP",True)):
                with Image.open(io.BytesIO(z.read(n))) as im:
                    self.assertEqual(im.format,fmt);first=im.convert("RGB").tobytes()
                    if animated:im.seek(1);self.assertNotEqual(first,im.convert("RGB").tobytes())
            with wave.open(io.BytesIO(z.read("sounds/home.wav")),"rb") as w:self.assertEqual((w.getsampwidth(),w.getnchannels()),(2,1))
    def test_06_reject_invalid_audio(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.req("/api/transcribe",b"not audio")
        self.assertEqual(e.exception.code,400)
    @unittest.skipUnless(os.environ.get("SPEECH_FIXTURE"),"Set SPEECH_FIXTURE to upstream test/data/goforward.raw")
    def test_07_real_dictation_and_3ds_rate(self):
        from array import array
        src=array("h");src.frombytes(Path(os.environ["SPEECH_FIXTURE"]).read_bytes())
        if sys.byteorder=="big":src.byteswap()
        rate=16364
        pcm=array("h",(src[min(i*16000//rate,len(src)-1)] for i in range(len(src)*rate//16000)))
        if sys.byteorder=="big":pcm.byteswap()
        b=io.BytesIO()
        with wave.open(b,"wb") as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm.tobytes())
        out=json.loads(self.req("/api/transcribe",b.getvalue()));self.assertIn("forward",out["text"]);self.assertIn("meters",out["text"])
if __name__=="__main__":unittest.main()

"""Build a small, original example theme. Usage: python build_tidepool.py output.zip"""
from pathlib import Path
from PIL import Image, ImageDraw
from array import array
import io, json, math, sys, wave, zipfile

def encode(im,kind,**kw):
    s=io.BytesIO();im.save(s,format=kind,**kw);return s.getvalue()

def water(w,h,phase=0):
    im=Image.new("RGB",(w,h));d=ImageDraw.Draw(im)
    for y in range(h):
        f=y/h;d.line((0,y,w,y),fill=(int(10+7*f),int(30+25*f),int(48+21*f)))
    for level in range(4):
        pts=[(x,int(h*(.61+level*.1)+5*math.sin(x/(w*.12)+phase+level*.8))) for x in range(-1,w+2)]
        c=[(17,63,77),(19,76,82),(23,91,94),(28,102,100)][level]
        d.polygon([(0,h)]+pts+[(w,h)],fill=c)
    return im

def glyph(home,phase):
    im=Image.new("RGB",(60,30),(29,71,80));d=ImageDraw.Draw(im);ink=[(174,239,219),(244,208,139),(97,211,192)][phase]
    if home:
        d.line([(18,14),(30,5),(42,14)],fill=ink,width=2);d.rectangle((22,14,38,25),outline=ink,width=2);d.line((30,20,30,25),fill=ink,width=2)
    else:
        d.ellipse((16-phase,2,44+phase,28),outline=ink,width=2);d.line([(22,15),(28,20),(38,9)],fill=ink,width=3)
    return im

def wavefile(hz):
    rate=22050;n=rate//7;pcm=array("h",(int(6200*math.sin(2*math.pi*hz*i/rate)*math.exp(-6*i/n)) for i in range(n)))
    if sys.byteorder=="big":pcm.byteswap()
    s=io.BytesIO()
    with wave.open(s,"wb") as waveout:
        waveout.setnchannels(1);waveout.setsampwidth(2);waveout.setframerate(rate);waveout.writeframes(pcm.tobytes())
    return s.getvalue()

def build(target):
    colors={"top":"0C2031","bottom":"102A37","accent":"55C2AB","text":"EFFAF4","muted":"A6C9C5","highlight":"88E7C7","panel":"193B49","border":"112C38","selected":"246C70","button":"234957","button_text":"F5FAEF"}
    m={"format":2,"name":"Tidepool","creator":"Roku Remote 3DS project","category":"Nature","description":"Deep teal waves, a pulsing OK button, a moving tide screensaver and gentle button tones.","base":"Dark","colors":colors,
       "images":{"top":"art/top.png","bottom":"art/bottom.jpg","Select":"buttons/ok.gif","Home":"buttons/home.webp","screensaver":"art/tide.webp","screenshot":"screenshot.png"},
       "sounds":{"Home":"sounds/home.wav","Back":"sounds/back.wav","Select":"sounds/ok.wav","Directional":"sounds/tap.wav","Volume":"sounds/tap.wav","Power":"sounds/back.wav"}}
    home=[glyph(True,i) for i in range(3)];ok=[glyph(False,i) for i in range(3)]
    tide=[water(160,96,x) for x in range(6)]
    preview=water(320,190);d=ImageDraw.Draw(preview)
    d.text((15,9),"T I D E P O O L",fill="#B4F4E2")
    for x,y,w,h,t in ((8,36,70,21,"BACK"),(83,36,70,21,"HOME"),(158,36,82,21,"OPTIONS"),(245,36,67,21,"SETUP"),(113,64,94,23,"UP"),(16,94,88,30,"LEFT"),(113,94,94,30,"OK"),(216,94,88,30,"RIGHT"),(113,132,94,23,"DOWN")):
        d.rounded_rectangle((x,y,x+w,y+h),5,fill="#246C70" if t=="OK" else "#234957");d.text((x+w//2,y+h//2),t,anchor="mm",fill="#F5FAEF")
    files={"theme.json":(json.dumps(m,indent=2)+"\n").encode(),"art/top.png":encode(water(200,120),"PNG"),
        "art/bottom.jpg":encode(water(160,120),"JPEG",quality=88),"buttons/ok.gif":encode(ok[0],"GIF",save_all=True,append_images=ok[1:],duration=[180,180,300],loop=0),
        "buttons/home.webp":encode(home[0],"WEBP",save_all=True,append_images=home[1:],duration=[220,220,300],loop=0,lossless=True),
        "art/tide.webp":encode(tide[0],"WEBP",save_all=True,append_images=tide[1:],duration=280,loop=0,lossless=True),"screenshot.png":encode(preview,"PNG")}
    for k,hz in {"home":420,"ok":540,"tap":320,"back":240}.items():files["sounds/"+k+".wav"]=wavefile(hz)
    target=Path(target);target.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(target,"w",zipfile.ZIP_DEFLATED) as z:
        for n,b in files.items():
            i=zipfile.ZipInfo(n,(2024,1,1,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,b)
    return target
if __name__=="__main__":build(sys.argv[1] if len(sys.argv)>1 else "Tidepool-Version2.zip")

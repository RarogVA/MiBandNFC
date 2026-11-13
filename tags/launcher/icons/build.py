#!/usr/bin/env python3
"""Rasterize vendored Google Material rounded chevrons/refresh into Vela I8.

Modified format only: SVG sources remain verbatim. Apache-2.0, see LICENSE.
The 12-byte magic=0x19/cf=0x0a I8 file header and BGRA palette match the
user-provided recorder.face image resources. Only M/L/H/V/C/Z are needed here.
"""
from pathlib import Path
import hashlib,json,re,struct,xml.etree.ElementTree as ET
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent
def points(path):
    t=re.findall(r"[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)",path)
    i=0;cmd="";pos=(0.,0.);start=pos;out=[]
    while i<len(t):
        if t[i].isalpha():cmd=t[i];i+=1
        c=cmd.upper();relative=cmd.islower()
        if c=="Z":out.append(start);pos=start;cmd="";continue
        count={"M":2,"L":2,"H":1,"V":1,"C":6}[c]
        nums=list(map(float,t[i:i+count]));assert len(nums)==count;i+=count
        if c in ("H","V"):
            value=nums[0]+(pos[0 if c=="H" else 1] if relative else 0)
            pos=(value,pos[1]) if c=="H" else (pos[0],value)
            out.append(pos);continue
        pairs=[tuple(nums[j:j+2]) for j in range(0,count,2)]
        if relative:pairs=[(pos[0]+x,pos[1]+y) for x,y in pairs]
        if c in ("M","L"):
            pos=pairs[0];out.append(pos)
            if c=="M":start=pos;cmd="l" if relative else "L"
        else:
            p0=pos;p1,p2,p3=pairs
            for k in range(1,33):
                u=k/32;v=1-u
                out.append(tuple(v*v*v*p0[j]+3*v*v*u*p1[j]+3*v*u*u*p2[j]+u*u*u*p3[j] for j in (0,1)))
            pos=p3
    return out
def main():
    result={}
    for name in ("left","right","refresh"):
        source=ROOT/(name+".svg");xml=ET.fromstring(source.read_text())
        shape=next(p for p in xml if p.get("fill")!="none")
        size=32;scale=8
        im=Image.new("RGBA",(size*scale,size*scale))
        ImageDraw.Draw(im).polygon([(x*size*scale/24,y*size*scale/24) for x,y in points(shape.get("d"))],fill=(240,243,248,255))
        im=im.resize((size,size),Image.Resampling.LANCZOS)
        palette={};indices=bytearray()
        for color in im.getdata():
            if color not in palette:palette[color]=len(palette)
            indices.append(palette[color])
        assert len(palette)<=256
        colors=bytearray()
        for r,g,b,a in palette:colors.extend((b,g,r,a))
        colors.extend(bytes(1024-len(colors)))
        data=struct.pack("<BBHHHHH",0x19,0x0a,0,size,size,size,0)+colors+indices
        assert len(data)==12+1024+size*size
        (ROOT/(name+".bin")).write_bytes(data);im.save(ROOT/(name+".png"))
        result[name]={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'image_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'size':[size,size]}
    (ROOT/'manifest.json').write_text(json.dumps({'source':'https://github.com/google/material-design-icons','license':'Apache-2.0','modified':'Rasterized and encoded as Vela I8','files':result},indent=2)+'\n')
if __name__=='__main__':main()

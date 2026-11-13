#!/usr/bin/env python3
"""Level the 80 bottom metric labels in the new 992 face; append-only."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

from PIL import Image
from localize import extract_original,typography,translate_image,DEFAULT_FONT
from pack_roundabout import (index_entries,manifest_resources,parse_image,decode_frame,
                            encode_frame,rebuild_image,require)

ROOT=Path(__file__).resolve().parents[2]
SOURCE_SHA="ef28f70549660bc5f8a126c6e4dbcc53e13511d222863e405eade28b03766738"

def sha(data):return hashlib.sha256(data).hexdigest()

def horizontal(before,spec,render,font):
    """Keep the exact compiled icon; replace only the existing label region."""
    out=before.copy();region=tuple(render["region"]);target=render["old_text_box"]
    glyph=typography(spec["text"],font,tuple(render["color"]),
                     target[2]-target[0],target[3]-target[1],angle=0)
    x=target[0]+(target[2]-target[0]-glyph.width)//2
    y=target[1]+(target[3]-target[1]-glyph.height)//2
    # User corrections: right Calories/heart-rate labels move right, never down.
    # The arc rises to the right; +12 gives clearance while staying before
    # the independent icon columns x>=96.
    shift_x=12 if spec["side"]=="right" and spec["text"] in ("Ккал","ЧСС") else 0
    x+=shift_x
    require(region[0]<=x and x+glyph.width<=region[2],"Label crossed its protected icon region")
    for levels in (256,32,16,8,4,2):
        if levels<256:
            glyph.putalpha(glyph.getchannel("A").point(lambda a:round(round(a*(levels-1)/255)*255/(levels-1))))
        out.paste((0,0,0,0),region);out.alpha_composite(glyph,(x,y))
        if len(set(out.getdata()))<=256:break
    require(len(set(out.getdata()))<=256,"Flat label cannot fit palette")
    protected=(0,0,40,out.height) if spec["side"]=="left" else (96,0,out.width,out.height)
    require(out.crop(protected).tobytes()==before.crop(protected).tobytes(),"Metric icon changed")
    return out,{"old_rotation":-32 if spec["side"]=="left" else 32,"new_rotation":0,"shift_x":shift_x,"shift_y":0,
                "new_text_box":[x,y,x+glyph.width,y+glyph.height],"protected_icon_unchanged":True}

def build(args):
    original=args.input.read_bytes();require(original[0x28:0x68].split(b"\0")[0]==b"999999992","Only new ID992 may be modified")
    require(sha(args.source_zip.read_bytes())==SOURCE_SHA,"Source archive hash mismatch")
    source=extract_original(args.source_zip)
    mapping=json.loads(args.map.read_text())
    specs={f:s for f,s in mapping["images"].items() if s["kind"]=="arc_label"}
    require(len(specs)==80,"Expected exactly 80 bottom labels")
    require(sha(args.font.read_bytes())==mapping["font_sha256"],"Localization font changed")
    _,resources=manifest_resources(source)
    before_entries=index_entries(original)
    image_entries={}
    for e in before_entries:
        if e[1] in (2,3):image_entries.setdefault(e[2],[]).append(e)
    result=bytearray(original);allowed=set();old_images={};new_images={};changed_records=[];seen=set()
    def append(payload):
        while len(result)%4:result.append(0)
        offset=len(result);result.extend(payload);return offset,len(payload)
    for rid,(kind,files) in resources.items():
        if not set(files)&set(specs):continue
        entries=image_entries[rid];off,size=entries[0][3:5]
        require(all(e[3:5]==(off,size) for e in entries),"UID has inconsistent payloads")
        dimensions,frames=parse_image(original[off:off+size],kind)
        require(len(frames)==len(files),"Image frame mapping mismatch")
        new_frames=[];record={"uid":hex(rid),"frames":[]}
        for filename,frame in zip(files,frames):
            if filename not in specs:new_frames.append(frame);continue
            spec=specs[filename]
            path=source/"resources"/filename
            require(sha(path.read_bytes())==spec["original_sha256"],"Original glyph source changed")
            with Image.open(path) as source_image:
                expected,render=translate_image(source_image,spec,args.font)
            pixels=decode_frame(frame,*dimensions)
            require(expected.size==dimensions and expected.tobytes()==pixels,"Input face is not the known tilted Russian label: "+filename)
            before=Image.frombytes("RGBA",dimensions,pixels)
            after,evidence=horizontal(before,spec,render,args.font)
            old_images[filename]=before;new_images[filename]=after;seen.add(filename)
            replacement=encode_frame(after.tobytes(),*dimensions)
            require(decode_frame(replacement,*dimensions)==after.tobytes(),"New label codec mismatch")
            new_frames.append(replacement)
            record["frames"].append({"file":filename,"text":spec["text"],**evidence})
        location=append(rebuild_image(kind,dimensions,new_frames))
        for e in entries:
            struct.pack_into("<II",result,e[5]+8,*location);allowed.update(range(e[5]+8,e[5]+16))
        changed_records.append(record)
    require(seen==set(specs),"Some metric labels were not rewritten")
    editor=json.loads((source/"editor.config.json").read_text())
    themes=[t for t in editor["themes"] if t["type"]=="normal"]
    grid=Image.new("RGB",(336*4,480));previews=[]
    args.preview.parent.mkdir(parents=True,exist_ok=True)
    for style,(theme,descriptor) in enumerate(zip(themes,(0,2,3,4)),1):
        pointer=168+descriptor*176+4
        off=struct.unpack_from("<I",original,pointer)[0]
        size=12+struct.unpack_from("<I",original,off+8)[0]
        dimensions,frames=parse_image(original[off:off+size],2)
        require(dimensions==(336,480) and len(frames)==1,"Unknown preview")
        before=Image.frombytes("RGBA",dimensions,decode_frame(frames[0],*dimensions));after=before.copy()
        background=before.getpixel((168,470));mask_points=set()
        for side,key in ((-4,"healthStepCount"),(169,"healthCalorieValue")):
            slot=next(s for s in theme["children"] if s["type"]=="Slot" and s["attrs"].get("x")==side)
            widget=next(w for w in slot["children"] if key in json.dumps(w))
            element=next(n for n in widget["children"] if n["type"]=="Image")
            filename=next(iter(element["attrs"]["resources"]["pointer0"].values()))[0]
            x=slot["attrs"]["x"]+element["attrs"]["x"];y=slot["attrs"]["y"]+element["attrs"]["y"]
            old=old_images[filename];new=new_images[filename].copy()
            mask=old.getchannel("A").point(lambda a:255 if a else 0)
            for iy in range(old.height):
                for ix in range(old.width):
                    if (side==-4 and ix<40) or (side==169 and ix>=96):
                        mask.putpixel((ix,iy),0);new.putpixel((ix,iy),(0,0,0,0));continue
                    if mask.getpixel((ix,iy)) or new.getpixel((ix,iy))[3]:mask_points.add((x+ix,y+iy))
            after.paste(background,(x,y,x+old.width,y+old.height),mask)
            after.alpha_composite(new,(x,y))
        palette=sorted(set(before.getdata()));nearest={};changed=0
        for y in range(480):
            for x in range(336):
                a,b=after.getpixel((x,y)),before.getpixel((x,y))
                if a==b:continue
                require((x,y) in mask_points,"Preview changed outside actual glyph support")
                if a not in nearest:nearest[a]=min(palette,key=lambda p:sum((v-w)**2 for v,w in zip(a,p)))
                after.putpixel((x,y),nearest[a]);changed+=1
        payload=rebuild_image(2,dimensions,[encode_frame(after.tobytes(),*dimensions)])
        new_offset,_=append(payload)
        struct.pack_into("<I",result,pointer,new_offset);allowed.update(range(pointer,pointer+4))
        if style==1:
            struct.pack_into("<I",result,0x20,new_offset);allowed.update(range(0x20,0x24))
            after.convert("RGB").save(args.preview)
        after.save(args.preview.with_name(args.preview.stem+f"-style{style}.png"))
        grid.paste(after.convert("RGB"),((style-1)*336,0))
        previews.append({"theme":descriptor,"changed_pixels":changed,"outside_label_support_unchanged":True})
    grid.save(args.preview.with_name(args.preview.stem+"-styles.png"))
    changed_uids={int(r["uid"],16) for r in changed_records}
    after_entries={(t,u):(off,size) for t,k,u,off,size,p in index_entries(result)}
    unchanged=0
    for t,k,u,off,size,p in before_entries:
        if u in changed_uids:continue
        require(after_entries[t,u]==(off,size) and result[off:off+size]==original[off:off+size],"Unrelated resource changed")
        unchanged+=1
    changed_prefix=[i for i,(a,b) in enumerate(zip(original,result)) if a!=b]
    require(set(changed_prefix)<=allowed,"Unexpected prefix edit")
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(result)
    report={"input_sha256":sha(original),"output_sha256":sha(result),"bytes":len(result),"source_sha256":SOURCE_SHA,
            "changed_images":len(seen),"changed_resource_payloads":len(changed_records),"unchanged_resource_entries":unchanged,
            "changed_prefix_bytes":len(changed_prefix),"records":changed_records,"previews":previews,
            "positions_and_numeric_sources_unchanged":True,"hardware_validated":False}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({k:report[k] for k in ("output_sha256","bytes","changed_images","changed_resource_payloads")},indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",type=Path,default=ROOT/"build/roundabout-ru-nfc2.face")
    p.add_argument("--output",type=Path,default=ROOT/"build/roundabout-ru-nfc2-level.face")
    p.add_argument("--source-zip",type=Path,default=ROOT/"references/roundabout-original.zip")
    p.add_argument("--map",type=Path,default=Path(__file__).with_name("localization-map.json"))
    p.add_argument("--font",type=Path,default=Path(DEFAULT_FONT))
    p.add_argument("--report",type=Path,default=Path(__file__).with_name("level_labels_validation.json"))
    p.add_argument("--preview",type=Path,default=ROOT/"build/roundabout-ru-nfc2-level.png")
    args=p.parse_args();require(args.input.resolve()!=args.output.resolve(),"Use a separate output")
    build(args)

if __name__=="__main__":main()

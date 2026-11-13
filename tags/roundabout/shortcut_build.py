#!/usr/bin/env python3
"""Build a separate Roundabout RU NFC with one native-app shortcut.

Exact p67glt / 3.201.026 only. No firmware patching and no device writes.
All additions are appended; original payload bytes remain in the file.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont
from pack_roundabout import (index_entries, parse_image, decode_frame,
                            encode_frame, rebuild_image, require)

ROOT = Path(__file__).resolve().parents[2]
BASE_SHA = "b7875710957e54f4feee042ff1bcbc766f2fb49afc8cde19d4e047aa0e3bd3b2"
SOURCE_SHA = "ef28f70549660bc5f8a126c6e4dbcc53e13511d222863e405eade28b03766738"
FW_SHA = "66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
FACE_ID, FACE_NAME = "999999994", "Roundabout RU NFC"
APP_ID = 0xD4
ASSETS = Path(__file__).with_name("shortcut-assets")
FONT = Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf")
NORMAL_THEMES = (0, 2, 3, 4)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def translations(text):
    encoded = text.encode("utf-8")
    return struct.pack("<Q", (1 << 33)-1) + struct.pack("<33I", *([len(encoded)]*33)) + encoded*33


def make_assets(font):
    """Small code-drawn card/contactless icon; frozen PNGs are build inputs."""
    ASSETS.mkdir(exist_ok=True)
    accents = [(120,180,255), (171,184,244), (221,196,148), (120,180,255)]
    for theme in range(8):
        normal = theme in NORMAL_THEMES
        accent = accents[NORMAL_THEMES.index(theme)] if normal else (190,190,190)
        scale = 4
        im = Image.new("RGBA", (84*scale,84*scale))
        d = ImageDraw.Draw(im)
        box = lambda b: tuple(round(v*scale) for v in b)
        color = (*accent,255)
        d.ellipse(box((2,2,81,81)), fill=(20,20,20,255) if normal else (0,0,0,255),
                  outline=(*accent,255) if normal else (85,85,85,255), width=2*scale)
        d.rounded_rectangle(box((20,18,52,42)), radius=4*scale, outline=color, width=2*scale)
        d.line(box((22,26,50,26)), fill=color, width=2*scale)
        d.line(box((26,35,34,35)), fill=color, width=2*scale)
        d.arc(box((48,20,62,42)), -62,62, fill=color,width=2*scale)
        d.arc(box((46,15,69,47)), -58,58, fill=color,width=2*scale)
        face = ImageFont.truetype(str(font), 13*scale)
        d.text((42*scale,53*scale), "Метки", font=face, anchor="mt", fill=(245,245,245,255))
        im = im.resize((84,84),Image.Resampling.LANCZOS)
        # Only new artwork is quantized. Existing face resources are untouched.
        im = im.quantize(colors=256,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE).convert("RGBA")
        im.save(ASSETS/f"theme_{theme}.png")
    (ASSETS/"provenance.json").write_text(json.dumps({
        "generator":"shortcut_build.py --generate-assets", "font":font.name,
        "font_sha256":sha(font.read_bytes()), "palette":"256 RGBA, new artwork only",
        "files":{p.name:sha(p.read_bytes()) for p in sorted(ASSETS.glob("*.png"))}},indent=2)+"\n")


def build(args):
    baseline = args.baseline.read_bytes()
    require(sha(baseline)==BASE_SHA,"Unexpected Roundabout RU baseline")
    source = args.source_zip.read_bytes()
    require(sha(source)==SOURCE_SHA,"Unexpected original source archive")
    firmware = args.firmware.read_bytes()
    require(sha(firmware)==FW_SHA,"Wrong firmware: addresses are version-specific")
    with zipfile.ZipFile(args.source_zip) as z:
        xml = ET.fromstring(z.read("resources/manifest.xml"))
        names = {k:int(v,16) for k,v in re.findall(r"^(\w+):\s*([0-9a-fA-F]+)",z.read("uidmap.map").decode(),re.M)}
    resources = {n.get("name"):n for n in xml.find("Resources")}
    old_entries = index_entries(baseline)
    lookup = {(theme,rid):(offset,size) for theme,kind,rid,offset,size,pos in old_entries}
    maxima = {kind:max(rid for _,k,rid,*_ in old_entries if k==kind) for kind in (2,6,9)}
    output = bytearray(baseline)
    allowed_prefix = set(range(0x28,0x68)) | set(range(0x74,0x7c))
    new_records = {}
    changed_slots = {}
    report_themes = []

    def append(payload):
        while len(output)%4: output.append(0)
        offset=len(output);output.extend(payload)
        return offset,len(payload)

    def next_uid(kind):
        maxima[kind]+=1
        require(maxima[kind]>>24==kind,"Resource UID overflow")
        return maxima[kind]

    def read_resource(theme,uid):
        offset,size=lookup[theme,uid]
        return baseline[offset:offset+size]

    for theme,node in enumerate(xml.findall("Theme")):
        layout=[n for n in node if n.get("x")=="126" and n.get("y")=="274"]
        require(len(layout)==1,"Expected one bottom circle per theme")
        slot_name=layout[0].get("ref")[1:]; slot_uid=names[slot_name]
        slot_xml=resources[slot_name]; slot=read_resource(theme,slot_uid)
        count,reserved,slot_flags=struct.unpack_from("<BBH",slot)
        require(count==44 and reserved==0 and slot_flags==2 and len(slot)==188,"Unexpected bottom Slot layout")
        require(slot[4+count*4:]==bytes.fromhex("0400000033000000"),"Expected SlotGroupName string '3', not a default index")
        template=next(resources[n.get("ref")[1:]] for n in slot_xml
                      if resources[n.get("ref")[1:]].get("jumpApp")=="findPhone")
        widget=bytearray(read_resource(theme,names[template.get("name")]))
        require(len(widget)==64 and widget[40]==1,"Expected one-image Widget template")
        flags=struct.unpack_from("<H",widget,42)[0]
        require((flags>>4)&255==0x27 and flags&2,"Template must launch Find Phone")
        image_uid,translation_uid,widget_uid=next_uid(2),next_uid(6),next_uid(9)
        with Image.open(ASSETS/f"theme_{theme}.png") as image:
            image=image.convert("RGBA")
        require(image.size==(84,84),"Shortcut icon must preserve original dimensions")
        payload=rebuild_image(2,image.size,[encode_frame(image.tobytes(),*image.size)])
        new_records[theme,2,image_uid]=append(payload)
        new_records[theme,6,translation_uid]=append(translations("NFC-метки"))
        struct.pack_into("<I",widget,4,translation_uid)
        struct.pack_into("<I",widget,36,image_uid)
        struct.pack_into("<H",widget,42,(flags&~0xff0)|(APP_ID<<4))
        struct.pack_into("<hhII",widget,44,0,0,image_uid,0)
        new_records[theme,9,widget_uid]=append(widget)
        # One option fixes the bottom circle. Existing other slots keep all choices.
        new_slot=struct.pack("<BBHI",1,0,slot_flags,widget_uid)+slot[4+count*4:]
        changed_slots[theme,slot_uid]=append(new_slot)
        report_themes.append(dict(theme=theme,mode=node.get("type"),slot=slot_name,
                                  slot_uid=hex(slot_uid),widget_uid=hex(widget_uid),
                                  image_uid=hex(image_uid),translation_uid=hex(translation_uid),
                                  app_id=APP_ID,packed_flags=hex(struct.unpack_from("<H",widget,42)[0])))

    # Append replacement tables for just image/translation/widget kinds.
    for theme in range(8):
        descriptor=168+theme*176
        for kind in (2,6,9):
            entries=[(rid,0,offset,size) for t,k,rid,offset,size,_ in old_entries if (t,k)==(theme,kind)]
            entries += [(rid,0,*location) for (t,k,rid),location in new_records.items() if (t,k)==(theme,kind)]
            entries.sort()
            require(len({e[0] for e in entries})==len(entries),"Duplicate UID")
            offset,size=append(b"".join(struct.pack("<IIII",*entry) for entry in entries))
            p=descriptor+8+kind*8
            struct.pack_into("<II",output,p,len(entries),offset)
            allowed_prefix.update(range(p,p+8))
    for theme,kind,rid,offset,size,pos in old_entries:
        if (theme,rid) in changed_slots:
            struct.pack_into("<II",output,pos+8,*changed_slots[theme,rid])
            allowed_prefix.update(range(pos+8,pos+16))
    output[0x28:0x68]=FACE_ID.encode().ljust(64,b"\0")
    struct.pack_into("<II",output,0x74,*append(translations(FACE_NAME)))

    args.output.parent.mkdir(parents=True,exist_ok=True)
    thumbnails=[]; grid=Image.new("RGB",(336*4,480))
    for style,theme in enumerate(NORMAL_THEMES):
        pointer=168+theme*176+4
        offset=struct.unpack_from("<I",baseline,pointer)[0]
        length=12+struct.unpack_from("<I",baseline,offset+8)[0]
        size,frames=parse_image(baseline[offset:offset+length],2)
        before=Image.frombytes("RGBA",size,decode_frame(frames[0],*size))
        after=before.copy();background=before.getpixel((168,360))
        require(background[0]==background[1]==background[2],"Expected plain background under bottom circle")
        after.paste(background,(126,274,210,358))
        with Image.open(ASSETS/f"theme_{theme}.png") as im: after.alpha_composite(im.convert("RGBA"),(126,274))
        # Reuse the original palette for gallery only, preserving every pixel
        # outside the bottom circle. Live shortcut art retains its own palette.
        palette=sorted(set(before.getdata())); nearest={}
        for y in range(274,358):
            for x in range(126,210):
                c=after.getpixel((x,y))
                if c not in nearest:nearest[c]=min(palette,key=lambda p:sum((a-b)**2 for a,b in zip(c,p)))
                after.putpixel((x,y),nearest[c])
        changed=[(x,y) for y in range(480) for x in range(336) if before.getpixel((x,y))!=after.getpixel((x,y))]
        require(all(126<=x<210 and 274<=y<358 for x,y in changed),"Thumbnail changed outside bottom circle")
        payload=rebuild_image(2,size,[encode_frame(after.tobytes(),*size)])
        new_offset,_=append(payload)
        struct.pack_into("<I",output,pointer,new_offset);allowed_prefix.update(range(pointer,pointer+4))
        if theme==0:
            struct.pack_into("<I",output,0x20,new_offset);allowed_prefix.update(range(0x20,0x24))
            after.convert("RGB").save(args.output.with_suffix(".png"))
        grid.paste(after.convert("RGB"),(style*336,0))
        thumbnails.append(dict(theme=theme,changed_pixels=len(changed),changed_outside_bottom_circle=0))
    grid.save(args.output.with_name(args.output.stem+"-styles.png"))

    # Independent readback of all index records. Every original resource except
    # eight Slot payloads must still reference the same exact offset and bytes.
    final_entries=index_entries(output)
    final={(t,rid):(off,size) for t,k,rid,off,size,_ in final_entries}
    unchanged=0
    for t,k,rid,off,size,_ in old_entries:
        if (t,rid) in changed_slots: continue
        require(final[t,rid]==(off,size),"Original resource was remapped unexpectedly")
        require(output[off:off+size]==baseline[off:off+size],"Original payload changed")
        unchanged+=1
    for item in report_themes:
        t=item["theme"];su=int(item["slot_uid"],16);wu=int(item["widget_uid"],16)
        off,size=final[t,su];require(size==16 and struct.unpack_from("<HHI",output,off)==(1,2,wu),"Slot roundtrip mismatch")
        off,size=final[t,wu];flag=struct.unpack_from("<H",output,off+42)[0]
        # Model the exact loader (input bits4..11 -> runtime bits7..14), then
        # image_click_cb (runtime bits7..14 -> native launch descriptor.appid).
        runtime_flags=((flag&0xf)|(((flag>>4)&255)<<7)|(((flag>>12)&7)<<4))
        require((runtime_flags>>7)&255==APP_ID and runtime_flags&2,"Native action did not roundtrip")
    changed=[i for i,(a,b) in enumerate(zip(baseline,output)) if a!=b]
    require(set(changed)<=allowed_prefix,"Unexpected original-prefix mutation")
    require(sha(args.baseline.read_bytes())==BASE_SHA,"Baseline changed during build")
    args.output.write_bytes(output)
    report=dict(name=FACE_NAME,id=FACE_ID,app_id=APP_ID,app_name="dev.mbapdu.tags",firmware_sha256=FW_SHA,
                baseline_sha256=BASE_SHA,source_zip_sha256=SOURCE_SHA,output_sha256=sha(output),bytes=len(output),
                unchanged_original_index_entries=unchanged,changed_original_prefix_bytes=len(changed),
                new_resources=len(new_records),fixed_bottom_slots=8,themes=report_themes,thumbnails=thumbnails,
                hardware_tested=False,requires="NFC-tags native app 0xD4 registered until reboot",
                preserved="Original payload bytes, all other Slot choices, clock/metric resources and layout descriptors",
                proof={"packed_widget_action":"u16@42 bits4..11", "binary_parser":"0x2c7d470a..0x2c7d4716",
                       "click_handler":"0x2c7d0efc", "availability_helper":"0x2c4512d4: always0",
                       "id_mapper":"0x2c45148c: identity except0x39->0x44", "launch":"[0x200b7360+8], descriptor+6=appid",
                       "slot_parser":"0x2c7d4a88: u8 count, u16 flags; 0x2c7d4cb4..4cd4 copies group-name string trailer"})
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({"output":str(args.output),"sha256":sha(output),"bytes":len(output),"validated":True},indent=2))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline",type=Path,default=ROOT/"releases/roundabout/roundabout-ru-p67glt.face")
    p.add_argument("--source-zip",type=Path,default=ROOT/"references/roundabout-original.zip")
    p.add_argument("--firmware",type=Path,default=ROOT/"references/vela_ap.bin")
    p.add_argument("--output",type=Path,default=ROOT/"build/roundabout-ru-nfc.face")
    p.add_argument("--report",type=Path,default=Path(__file__).with_name("shortcut_validation.json"))
    p.add_argument("--generate-assets",action="store_true")
    p.add_argument("--font",type=Path,default=FONT)
    args=p.parse_args()
    if args.generate_assets:make_assets(args.font)
    build(args)


if __name__=="__main__":main()

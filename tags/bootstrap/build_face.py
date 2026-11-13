#!/usr/bin/env python3
"""Append a manual Lua bootstrap to the live lower NFC circle; no device I/O."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"roundabout"))
from pack_roundabout import rgba,encode_frame,rebuild_image,parse_image,decode_frame

ROOT=Path(__file__).resolve().parents[2]
BASE_SHA="2a7e7ff2b89b0845bf1cf96a63a0f844082d5be8d47b5c38312a480870bfa475"
FW_SHA="66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
NORMAL=(0,2,3,4)
FACE_ID="999999991"
FACE_NAME="Roundabout RU NFC 3"

def require(ok,message):
    if not ok: raise ValueError(message)

def sha(data):return hashlib.sha256(data).hexdigest()

def entries(data):
    for theme in range(8):
        for kind in range(12):
            count,table=struct.unpack_from("<II",data,168+theme*176+8+kind*8)
            require(count<10000 and (not count or table+count*16<=len(data)),"Invalid table bounds")
            for i in range(count):
                pos=table+i*16
                uid,reserved,offset,size=struct.unpack_from("<IIII",data,pos)
                require(reserved==0 and offset+size<=len(data),"Invalid resource bounds")
                require(uid>>24==kind,"Resource kind mismatch")
                yield theme,kind,uid,offset,size,pos

def file_resource(name,data):
    name=name.encode("ascii")
    require(0<len(name)<256 and len(data)<1<<24,"FileResource encoded length overflow")
    return struct.pack("<I",len(data)|(len(name)<<24))+bytes(16)+name+data

def decode_file(payload):
    header=struct.unpack_from("<I",payload)[0]
    size,n=header&0xffffff,header>>24
    require(payload[4:20]==bytes(16) and len(payload)==20+n+size,"Malformed FileResource")
    return payload[20:20+n].decode("ascii"),payload[20+n:]

def translations(text):
    text=text.encode("utf8")
    return struct.pack("<Q33I",(1<<33)-1,*([len(text)]*33))+text*33

def build(args):
    original=args.baseline.read_bytes()
    require(sha(original)==BASE_SHA,"Unexpected frozen NFC baseline")
    firmware=args.firmware.read_bytes()
    require(sha(firmware)==FW_SHA,"Unsupported firmware")
    # Read actual working recorder resource format instead of only trusting the
    # newly written serializer. Recorder has one theme with duplicate UID0.
    recorder=args.recorder.read_bytes()
    count,table=struct.unpack_from("<II",recorder,168+8+5*8)
    require(count==12,"Unexpected recorder FileResource table")
    rid,zero,offset,size=struct.unpack_from("<IIII",recorder,table)
    reference_name,reference_data=decode_file(recorder[offset:offset+size])
    require(rid==0x05000000 and reference_name=="app/lua/main.lua","Recorder Lua reference changed")
    require(file_resource(reference_name,reference_data)==recorder[offset:offset+size],"FileResource reference roundtrip failed")
    resources=[("app/lua/main.lua",args.lua.read_bytes()),
               ("app/lua/catalog_core_v1.elf",args.core.read_bytes()),
               ("app/lua/catalog_ui_v3.elf",args.ui.read_bytes())]
    for name,payload in resources[1:]:require(payload[:4]==b"\x7fELF",f"Not an ELF: {name}")
    for spec in args.extra:
        name,path=spec.split("=",1)
        require(name in ("left.bin","right.bin","refresh.bin"),"Only the three audited icon filenames are accepted")
        resources.append(("app/lua/"+name,Path(path).read_bytes()))
    require(len({n for n,_ in resources})==len(resources),"Duplicate embedded filename")
    output=bytearray(original)
    before=list(entries(original));old={(t,u):(off,size,pos) for t,k,u,off,size,pos in before}
    require(not any(k==5 for _,k,*_ in before),"Baseline unexpectedly has FileResources")
    allowed=set(range(0x28,0x68))|set(range(0x74,0x7c))
    def append(payload):
        while len(output)%4:output.append(0)
        offset=len(output);output.extend(payload)
        return offset,len(payload)
    file_records=[]
    for i,(name,payload) in enumerate(resources):
        file_records.append((0x05000000+i,0,*append(file_resource(name,payload))))
    file_table,_=append(b"".join(struct.pack("<IIII",*r) for r in file_records))
    editbox_uid=max(u for t,k,u,*_ in before if k==2)+1
    editbox_size,editbox_pixels=rgba(Path(__file__).with_name("editbox-84.png"))
    require(editbox_size==(84,84),"Editor boundary must equal live image bounds")
    editbox=append(rebuild_image(2,editbox_size,[encode_frame(editbox_pixels,*editbox_size)]))
    modified=[]
    for theme in range(8):
        widget_uid=0x09000132+theme
        off,size,indexpos=old[theme,widget_uid]
        widget=original[off:off+size]
        require(size==64 and widget[40]==1 and (struct.unpack_from("<H",widget,42)[0]>>4)&255==0xd4,
                "Unexpected lower-circle NFC Widget")
        # Keep name, image, flags, edit rectangle, all geometry. Append a
        # transparent Lua child above the original live image at local (0,0).
        patched=bytearray(widget)
        if theme in NORMAL:
            patched=bytearray(widget[:56]+struct.pack("<hhII",0,0,0x05000000,0)+widget[56:])
            patched[40]=2
        struct.pack_into("<IHH",patched,len(patched)-8,editbox_uid,84,84)
        struct.pack_into("<II",output,indexpos+8,*append(patched))
        allowed.update(range(indexpos+8,indexpos+16));modified.append((theme,widget_uid))
        if theme in NORMAL:
            descriptor=168+theme*176+8+5*8
            struct.pack_into("<II",output,descriptor,len(file_records),file_table)
            allowed.update(range(descriptor,descriptor+8))
        image_entries=[(u,0,off,size) for t,k,u,off,size,pos in before if t==theme and k==2]
        image_entries.append((editbox_uid,0,*editbox));image_entries.sort()
        image_table,_=append(b"".join(struct.pack("<IIII",*r) for r in image_entries))
        descriptor=168+theme*176+8+2*8
        struct.pack_into("<II",output,descriptor,len(image_entries),image_table)
        allowed.update(range(descriptor,descriptor+8))
    output[0x28:0x68]=FACE_ID.encode().ljust(64,b"\0")
    struct.pack_into("<II",output,0x74,*append(translations(FACE_NAME)))
    after={(t,u):(off,size,pos) for t,k,u,off,size,pos in entries(output)}
    unchanged=0
    for t,k,u,off,size,pos in before:
        if (t,u) in modified:continue
        require(after[t,u][:2]==(off,size),"Existing resource remapped")
        require(output[off:off+size]==original[off:off+size],"Existing resource bytes changed")
        unchanged+=1
    for theme in NORMAL:
        for i,(name,payload) in enumerate(resources):
            off,size,_=after[theme,0x05000000+i]
            require(decode_file(output[off:off+size])==(name,payload),"Embedded file roundtrip mismatch")
        off,size,_=after[theme,0x09000132+theme]
        require(size==76 and output[off+40]==2,"Widget size/count mismatch")
        require(struct.unpack_from("<hhII",output,off+56)==(0,0,0x05000000,0),"Lua reference mismatch")
    for theme in (1,5,6,7):
        require(not any(t==theme and k==5 for t,k,*_ in entries(output)),"AOD must not instantiate Lua")
    for theme in range(8):
        off,size,_=after[theme,0x09000132+theme]
        require(struct.unpack_from("<IHH",output,off+size-8)==(editbox_uid,84,84),"Editor rectangle mismatch")
        off,size,_=after[theme,editbox_uid]
        size,frames=parse_image(output[off:off+size],2)
        require(size==(84,84) and decode_frame(frames[0],*size)==editbox_pixels,"Editor image roundtrip failed")
    changed=[i for i,(a,b) in enumerate(zip(original,output)) if a!=b]
    require(set(changed)<=allowed,"Unexpected original-prefix modification")
    require(sha(args.baseline.read_bytes())==BASE_SHA,"Baseline modified")
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(output)
    report={"face_id":FACE_ID,"name":FACE_NAME,"bytes":len(output),"sha256":sha(output),
            "ui_version":3,"ui_module":"NFC_Tags_Catalog_UI_V3","core_version":1,
            "baseline_sha256":BASE_SHA,"firmware_sha256":FW_SHA,"normal_themes":list(NORMAL),
            "unchanged_resource_entries":unchanged,"changed_prefix_bytes":len(changed),
            "editor_boundary":{"uid":hex(editbox_uid),"old_pixels":[69,69],"new_pixels":[84,84],"widget_position":[126,274],"themes":8},
            "resources":[{"name":n,"bytes":len(p),"sha256":sha(p)} for n,p in resources],
            "preserved":"All baseline clock/metric payloads, image previews, other buttons and AOD rendering; lower-circle Widget references change for Lua and corrected editBox",
            "manual_bootstrap_only":True,"automatic_nfc_start":False,"hardware_validated":False,
            "remaining_hardware_checks":["Lua inside ordinary Widget initializes and receives tap","Both files extract into face directory","First-tap UI deferred open","Later native shortcut pass-through","Reboot and repeat"]}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({"output":str(args.output),"bytes":len(output),"sha256":sha(output),"validated":True},indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline",type=Path,default=ROOT/"releases/roundabout-nfc/roundabout-ru-nfc-p67glt.face")
    p.add_argument("--firmware",type=Path,default=ROOT/"references/vela_ap.bin")
    p.add_argument("--recorder",type=Path,default=ROOT/"references/recorder.face")
    p.add_argument("--lua",type=Path,default=Path(__file__).with_name("main.lua"))
    p.add_argument("--core",type=Path,required=True)
    p.add_argument("--ui",type=Path,required=True)
    p.add_argument("--extra",action="append",default=[],metavar="NAME=PATH")
    p.add_argument("--output",type=Path,default=ROOT/"build/roundabout-ru-nfc3.face")
    p.add_argument("--report",type=Path,default=Path(__file__).with_name("validation.json"))
    build(p.parse_args())

if __name__=="__main__":main()

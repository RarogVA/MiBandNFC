#!/usr/bin/env python3
"""Independent readback for the local NDEF face; no device access."""
import argparse,json,struct
from pathlib import Path
from build_face import entries,decode_file,sha,require,ROOT,NORMAL,ASSET_NAMES

FACE_ID="999999969"
FACE_NAME="Roundabout RU NFC Local"

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--face",type=Path,default=ROOT/"build/roundabout-ru-nfc-local.face")
    args=p.parse_args();data=args.face.read_bytes()
    require(data[0x28:0x68].split(b"\0")[0]==FACE_ID.encode(),"Local face ID changed")
    # Compare the face against the committed release copies, not ignored build/
    # outputs: CI has no NDK build products.
    files={"app/lua/main_local.lua":ROOT/"tags/bootstrap/main_local.lua",
           "app/lua/catalog_core_v2_local.elf":ROOT/"releases/local-ndef/catalog-core-v2-local.elf",
           "app/lua/catalog_ui_v5.elf":ROOT/"releases/local-ndef/catalog-ui-v5.elf"}
    files.update({"app/lua/"+name+".bin":ROOT/"tags/launcher/icons"/(name+".bin") for name in ASSET_NAMES})
    table=list(entries(data));checked=[]
    for theme in NORMAL:
        found={decode_file(data[o:o+s])[0]:decode_file(data[o:o+s])[1] for t,k,u,o,s,pos in table if t==theme and k==5}
        require(set(found)==set(files),"Embedded file set changed")
        for name,path in files.items():require(found[name]==path.read_bytes(),"Embedded/source mismatch: "+name)
        for t,k,u,o,s,pos in table:
            if t==theme and u==0x09000132+theme:
                require(s==76 and data[o+40]==2,"Missing hybrid Widget")
                require((struct.unpack_from("<H",data,o+42)[0]>>4)&255==0xd4,"Wrong app ID")
                require(struct.unpack_from("<HH",data,o+s-4)==(84,84),"Editor bounds changed")
                require(struct.unpack_from("<hhII",data,o+56)==(0,0,0x05000000,0),"Wrong bootstrap origin/reference")
    require(not any(t not in NORMAL and k==5 for t,k,u,o,s,pos in table),"Lua present in AOD")
    baseline=(ROOT/"releases/roundabout-nfc/roundabout-ru-nfc-p67glt.face").read_bytes()
    old_data={(t,u):baseline[o:o+s] for t,k,u,o,s,pos in entries(baseline) if k==7}
    unchanged_numeric=0
    for t,k,u,o,s,pos in table:
        if k==7:
            require(data[o:o+s]==old_data[t,u],"Original data/numeric resource changed")
            unchanged_numeric+=1
    require(unchanged_numeric==len(old_data),"Original data resource missing")
    for path,digest in [("releases/roundabout/roundabout-ru-p67glt.face","b7875710957e54f4feee042ff1bcbc766f2fb49afc8cde19d4e047aa0e3bd3b2"),
                        ("releases/roundabout-nfc/roundabout-ru-nfc-p67glt.face","2a7e7ff2b89b0845bf1cf96a63a0f844082d5be8d47b5c38312a480870bfa475")]:
        require(sha((ROOT/path).read_bytes())==digest,"Frozen release changed")
    off,size=struct.unpack_from("<II",data,0x74);title=data[off:off+size]
    require(struct.unpack_from("<Q",title)[0]==(1<<33)-1,"Title locale mask changed")
    lengths=struct.unpack_from("<33I",title,8);cursor=140
    for length in lengths:
        require(title[cursor:cursor+length].decode()==FACE_NAME,"Unexpected localized title");cursor+=length
    require(cursor==len(title),"Title bounds mismatch")
    core=(ROOT/"releases/local-ndef/catalog-core-v2-local.elf").read_bytes()
    ui=(ROOT/"releases/local-ndef/catalog-ui-v5.elf").read_bytes()
    report={"face_id":FACE_ID,"name":FACE_NAME,"bytes":len(data),"sha256":sha(data),
            "ui_version":5,"ui_module":"NFC_Tags_Catalog_UI_V5","core_version":2,
            "embedded_files":[{"name":name,"source":str(path.relative_to(ROOT)),
                "bytes":path.stat().st_size,"sha256":sha(path.read_bytes())} for name,path in files.items()],
            "core_sha256":sha(core),"ui_sha256":sha(ui),
            "verified_normal_themes":list(NORMAL),"native_app_id":212,
            "original_data_resources_byteidentical":unchanged_numeric,
            "previous_releases_unchanged":True,"hardware_validated":False,
            "local_nfc_start_on_page_open":True}
    out=ROOT/"build/roundabout-ru-nfc-local-validation.json"
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))

if __name__=="__main__":main()

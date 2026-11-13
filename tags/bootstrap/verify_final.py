#!/usr/bin/env python3
"""Independent final-face readback; all original numeric bytes are preserved."""
import argparse,json,struct
from pathlib import Path
from build_face import entries,decode_file,sha,require,ROOT,NORMAL

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--face",type=Path,default=ROOT/"build/roundabout-ru-nfc3-final.face")
    args=p.parse_args();data=args.face.read_bytes()
    require(data[0x28:0x68].split(b"\0")[0]==b"999999991","Final ID changed")
    files={"app/lua/main.lua":ROOT/"tags/bootstrap/main.lua",
           "app/lua/catalog_core_v1.elf":ROOT/"tags/catalog/build/catalog_core_v1.elf",
           "app/lua/catalog_ui_v3.elf":ROOT/"tags/launcher/build/nfc_tags_catalog_ui_v3.elf",
           "app/lua/left.bin":ROOT/"tags/launcher/icons/left.bin",
           "app/lua/right.bin":ROOT/"tags/launcher/icons/right.bin",
           "app/lua/refresh.bin":ROOT/"tags/launcher/icons/refresh.bin"}
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
    labels=json.loads((ROOT/"tags/roundabout/level_labels_validation.json").read_text())
    require(labels["output_sha256"]==sha(data),"Label report refers to another face")
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
        require(title[cursor:cursor+length].decode()=="Roundabout RU NFC 3","Unexpected localized title");cursor+=length
    require(cursor==len(title),"Title bounds mismatch")
    report={"face_id":"999999991","name":"Roundabout RU NFC 3","bytes":len(data),"sha256":sha(data),"ui_version":3,"core_version":1,
            "embedded_files":[{"name":name,"source":str(path.relative_to(ROOT)),"bytes":path.stat().st_size,"sha256":sha(path.read_bytes())} for name,path in files.items()],
            "verified_normal_themes":list(NORMAL),"native_app_id":212,"original_data_resources_byteidentical":unchanged_numeric,"previous_releases_unchanged":True,
            "hardware_validated":False}
    Path(__file__).with_name("final_validation.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))

if __name__=="__main__":main()

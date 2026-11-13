#!/usr/bin/env python3
"""Check final Calories/heart-rate labels against real progress alpha masks."""
from pathlib import Path
import argparse,json,zipfile,xml.etree.ElementTree as ET
from pack_roundabout import index_entries,parse_image,decode_frame,sha,require

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--face",type=Path,default=ROOT/"build/roundabout-ru-nfc3-final.face")
    args=parser.parse_args();data=args.face.read_bytes()
    entries={(t,u):(o,s,k) for t,k,u,o,s,p in index_entries(data)}
    with zipfile.ZipFile(ROOT/"references/roundabout-original.zip") as z:
        xml=ET.fromstring(z.read("resources/manifest.xml"));resources={n.get("name"):n for n in xml.find("Resources")}
        uid={line.split(": ")[0]:int(line.split(": ")[1],16) for line in z.read("uidmap.map").decode().splitlines()}
    results=[]
    metrics={"healthCalorieProgress":"Ккал","healthHeartRateZone":"ЧСС"}
    for ti,theme in enumerate(xml.findall("Theme")):
        for si in theme:
            if si.get("y")!="364":continue
            side=int(si.get("x"))
            for wi in resources[si.get("ref")[1:]]:
                children=[(i,resources[i.get("ref")[1:]]) for i in resources[wi.get("ref")[1:]]]
                metric=next((n.get("source") for i,n in children if n.get("source") in metrics),None)
                if not metric:continue
                bars=set();letters=set()
                for item,node in children:
                    x,y=int(item.get("x")),int(item.get("y"))
                    progress=node.get("source")==metric
                    if not progress and node.tag!="Image":continue
                    rid=uid[node.get("ref")[1:]] if progress else uid[node.get("name")]
                    offset,length,kind=entries[ti,rid];size,frames=parse_image(data[offset:offset+length],kind)
                    for frame in frames:
                        raw=decode_frame(frame,*size)
                        for iy in range(size[1]):
                            for ix in range(size[0]):
                                if not progress and ((side==-4 and ix<40) or (side==169 and ix>=96)):continue
                                if raw[(iy*size[0]+ix)*4+3]:(bars if progress else letters).add((x+ix,y+iy))
                distance=min(max(abs(x-a),abs(y-b)) for x,y in letters for a,b in bars)
                required=4 if metric=="healthCalorieProgress" else 3
                require(distance>=required,f"{metrics[metric]} theme={ti} side={side} distance={distance}: insufficient empty pixels before the arc")
                results.append({"theme":ti,"label":metrics[metric],"side":"left" if side==-4 else "right","minimum_pixel_distance":distance,"empty_pixel_gap":distance-1})
    require(len(results)==32,"Expected eight themes, two sides and two metric labels")
    report={"face_sha256":sha(data),"method":"Chebyshev distance: all live progress-frame alpha pixels versus final Calories/heart-rate glyph alpha; icon columns excluded","results":results}
    Path(__file__).with_name("label_clearance_validation.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))

if __name__=="__main__":main()

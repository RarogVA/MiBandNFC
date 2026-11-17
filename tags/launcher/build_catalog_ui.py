#!/usr/bin/env python3
"""Build catalog UI separately from frozen single-tag UI profiles."""
from pathlib import Path
import hashlib,importlib.util,json,re
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("tag_core_builder",ROOT/"tags/build_band.py")
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
OUT=ROOT/"tags/launcher/build"
SOURCES=["tags/launcher/catalog_ui.c","tags/launcher/catalog_ui_bindings.c",
         "tags/launcher/firmware.c","tags/bootstrap/launch.c","band/runtime.c"]
def main():
    firmware=(ROOT/"references/vela_ap.bin").read_bytes()
    if hashlib.sha256(firmware).hexdigest()!="66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40":raise SystemExit("Wrong firmware")
    checked=[]
    for name in ("tags/launcher/fingerprints.h","tags/launcher/catalog_ui_bindings.c","tags/bootstrap/launch.c"):
        for a,values in re.findall(r"\{\s*(0x[0-9a-fA-F]+)u?,\s*\{([^}]+)\}\}",(ROOT/name).read_text()):
            addr=int(a,16);data=bytes(int(v.strip(),16) for v in values.split(",") if v.strip())
            if len(data)!=24 or firmware[addr-0x2c0c0000:addr-0x2c0c0000+24]!=data:raise SystemExit("Bad fingerprint "+a)
            checked.append(a)
    if len(checked)!=39:raise SystemExit("Expected all 39 UI firmware gates")
    OUT.mkdir(parents=True,exist_ok=True);objects=[]
    flags=[f for f in b.FLAGS if not f.startswith(("-DNFC_","-DTRANSPORT_"))]
    for source in SOURCES:
        obj=OUT/(Path(source).stem+"-catalog.o")
        b.run([b.BIN/"clang",*flags,"-I",ROOT/"tags/launcher","-c",ROOT/source,"-o",obj])
        b.run([b.BIN/"llvm-objcopy","--remove-section=.ARM.exidx*","--remove-section=.comment",obj]);objects.append(obj)
    elf=OUT/"nfc_tags_catalog_ui_v4.elf"
    b.run([b.BIN/"ld.lld","-r","-T",ROOT/"band/module.ld","-o",elf,*objects])
    before=elf.read_bytes();b.check_elf(before)
    b.run([b.BIN/"llvm-objcopy","--strip-unneeded","--keep-symbol=module_constructor",elf])
    data=elf.read_bytes()
    if b.elf_semantics(before)!=b.elf_semantics(data):raise SystemExit("Stripping changed loaded semantics")
    undefined=b.run([b.BIN/"llvm-nm","--undefined-only",elf]).strip()
    if undefined:raise SystemExit(undefined)
    entry,constructor=b.check_elf(data)
    inputs=SOURCES+["tags/launcher/catalog_ui_bindings.h","tags/launcher/catalog_layout.h","tags/launcher/catalog_assets.h","tags/launcher/bindings.h","tags/launcher/abi.h","tags/catalog/api.h","tags/bootstrap/launch.h","tags/launcher/fingerprints.h","tags/launcher/build_catalog_ui.py"]
    report=dict(model="miwear.watch.p67glt",firmware="3.201.026",profile="catalog-ui-v4",app_id=212,
                elf_bytes=len(data),elf_sha256=hashlib.sha256(data).hexdigest(),constructor=constructor,
                fingerprints=checked,hardware_tested=False,
                source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs})
    (OUT/"catalog-ui-build.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({k:report[k] for k in ("profile","elf_bytes","elf_sha256","hardware_tested")},indent=2))
if __name__=="__main__":main()

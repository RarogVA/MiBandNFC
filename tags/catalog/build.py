#!/usr/bin/env python3
"""Build isolated catalog core. Never replace frozen tag-v2 releases or install."""
from pathlib import Path
import hashlib,importlib.util,json,re
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('tag_builder',ROOT/'tags/build_band.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
OUT=ROOT/'tags/catalog/build'
SOURCES=['tags/band/module.c','tags/catalog/catalog.c','tags/catalog/storage.c','tags/catalog/sha256.c','tags/catalog/fs.c','tags/catalog/provider.c','tags/band/tag_record.c','band/transport.c','band/native_clock.c','band/nfc_probe.c','band/route_table.c','band/runtime.c','band/diagnostic.c']
def main():
 fw=(ROOT/'references/vela_ap.bin').read_bytes();expected='66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40'
 if hashlib.sha256(fw).hexdigest()!=expected:raise SystemExit('Wrong firmware')
 for path in (ROOT/'tags/catalog').glob('*fingerprints.h'):
  for address,values in re.findall(r'\{(0x[0-9a-f]+)u,\{([^}]+)\}\}',path.read_text()):
   at=int(address,16)-0x2c0c0000;data=bytes(int(x,16) for x in values.split(','))
   if len(data)!=24 or fw[at:at+24]!=data:raise SystemExit('Fingerprint mismatch '+address)
 OUT.mkdir(parents=True,exist_ok=True);objects=[]
 flags=b.FLAGS+['-DTAG_CATALOG_PROFILE=1','-DDIAGNOSTIC_STATE_PATH="/tmp/tag_catalog_v1_diag"']
 for source in SOURCES:
  obj=OUT/(source.replace('/','_')+'.o');b.run([b.BIN/'clang',*flags,'-I',ROOT/'band','-I',ROOT/'tags/band','-I',ROOT/'tags/catalog','-c',ROOT/source,'-o',obj]);b.run([b.BIN/'llvm-objcopy','--remove-section=.ARM.exidx*','--remove-section=.comment',obj]);objects.append(obj)
 elf=OUT/'catalog_core_v1.elf';b.run([b.BIN/'ld.lld','-r','-T',ROOT/'band/module.ld','-o',elf,*objects]);before=elf.read_bytes();b.check_elf(before)
 b.run([b.BIN/'llvm-objcopy','--strip-unneeded','--keep-symbol=module_constructor',elf]);data=elf.read_bytes()
 if b.elf_semantics(before)!=b.elf_semantics(data):raise SystemExit('Stripping changed semantics')
 if b.run([b.BIN/'llvm-nm','--undefined-only',elf]).strip():raise SystemExit('Unresolved symbols')
 _,ctor=b.check_elf(data);inputs=set(SOURCES)|{'tags/catalog/core.c','tags/catalog/build.py','band/module.ld','tags/launcher/abi.h'}
 for folder in ['band','tags/catalog','tags/band']:inputs.update(str(p.relative_to(ROOT)) for p in (ROOT/folder).glob('*.h'))
 report=dict(profile='catalog-v1',firmware_sha256=expected,elf_bytes=len(data),elf_sha256=hashlib.sha256(data).hexdigest(),constructor=ctor,hardware_validated=False,source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sorted(inputs)})
 (OUT/'build.json').write_text(json.dumps(report,indent=2)+'\n');(OUT/'inspection.txt').write_text(b.run([b.BIN/'llvm-readelf','-h','-S','-r',elf]));print(json.dumps({k:report[k] for k in ['elf_bytes','elf_sha256','hardware_validated']},indent=2))
if __name__=='__main__':main()

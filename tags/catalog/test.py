#!/usr/bin/env python3
from pathlib import Path
import subprocess,json
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'tags/catalog/build/tests';OUT.mkdir(parents=True,exist_ok=True)
FLAGS=['-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Itags/catalog','-Itags/catalog/tests','-Iband','-Itags/band','-Itests','-DTRANSPORT_TAG_PROFILE=1','-DTAG_CATALOG_PROFILE=1']
TESTS={'catalog':['tags/catalog/tests/catalog_test.c','tags/catalog/catalog.c','tags/catalog/storage.c','tags/catalog/sha256.c']}
TESTS['core']=['tags/catalog/tests/core_test.c','tags/catalog/catalog.c','tags/catalog/storage.c','tags/catalog/sha256.c','tags/band/tag_record.c']
TESTS['transport']=['tags/catalog/tests/transport_test.c']
def main():
 results={}
 for name,sources in TESTS.items():
  exe=OUT/name;subprocess.run(['clang',*FLAGS,*sources,'-o',str(exe)],cwd=ROOT,check=True)
  r=subprocess.run([str(exe)],cwd=ROOT,text=True,capture_output=True)
  if r.returncode:print(r.stdout+r.stderr);r.check_returncode()
  results[name]=r.stdout.strip();print(r.stdout,end='')
 (OUT/'results.json').write_text(json.dumps({'status':'PASS','sanitizers':['address','undefined'],'results':results},indent=2)+'\n')
if __name__=='__main__':main()

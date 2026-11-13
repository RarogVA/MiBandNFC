#include <stdio.h>
#include <stdlib.h>
#include "sha256.h"
#include "mock_fs.h"
typedef struct{const uint8_t *p;size_t n,pos;} memory;
static size_t memory_read(void *ctx,void *out,size_t n){memory *m=ctx;if(n>m->n-m->pos)n=m->n-m->pos;memcpy(out,m->p+m->pos,n);m->pos+=n;return n;}
static int decode(catalog_snapshot *s,const uint8_t *p,size_t n){memory m={p,n,0};return catalog_decode(s,memory_read,&m,(uint32_t)n);}
static void hash_test(void){
 uint8_t hash[32],want[32];catalog_sha256_digest("",0,hash);unhex(want,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");assert(!memcmp(hash,want,32));
 catalog_sha256_digest("abc",3,hash);unhex(want,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");assert(!memcmp(hash,want,32));
 catalog_sha256 h;catalog_sha256_init(&h);char a[1000];memset(a,'a',sizeof(a));for(unsigned i=0;i<1000;i++)catalog_sha256_update(&h,a,sizeof(a));catalog_sha256_final(&h,hash);unhex(want,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");assert(!memcmp(hash,want,32));
}
static void send_snapshot(uint32_t rev,uint8_t *blob,size_t n,uint8_t hash[32]){catalog_put_u32(blob+8,rev);catalog_sha256_digest(blob,n,hash);assert(!catalog_storage_begin(rev,1,(uint32_t)n,hash));for(size_t at=0;at<n;){size_t k=n-at;if(k>17)k=17;assert(!catalog_storage_data((uint32_t)at,blob+at,k));at+=k;}}
int main(void){
 hash_test();uint8_t blob[65536],hash[32],want[32];size_t n=unhex(blob,golden_hex);assert(n==75);catalog_snapshot parsed;
 assert(!decode(&parsed,blob,n));assert(parsed.revision==7&&parsed.count==1&&!strcmp(parsed.entries[0].name,"NFC-Test"));unhex(want,golden_sha);assert(!memcmp(parsed.sha256,want,32));
 unhex(want,"71c395e4cb360cb7932ebdd8d625713788f35ea5ce691c2c9f5b8cfe760a4a82");assert(!memcmp(parsed.entries[0].ndef_sha256,want,32));
 for(size_t i=0;i<n;i++)assert(decode(&parsed,blob,i));
 uint8_t saved=blob[35];blob[35]=0;assert(decode(&parsed,blob,n));blob[35]=saved;
 uint8_t bad[76];memcpy(bad,blob,n);bad[n]=0;assert(decode(&parsed,bad,n+1));
 memcpy(bad,blob,n);bad[40]=0xc0;assert(decode(&parsed,bad,n));
 memcpy(bad,blob,n);bad[52]=0xb1;assert(decode(&parsed,bad,n));
 /* 32 distinct maximum-size NDEF records, with full-size names/UID/tech. */
 memcpy(blob,"TCAT\0\1\0\x20",8);catalog_put_u32(blob+8,8);size_t at=12;
 for(unsigned i=0;i<32;i++){unsigned body=26+96+20+48+1024;blob[at++]=(uint8_t)(body>>8);blob[at++]=(uint8_t)body;uint8_t *r=blob+at;memset(r,0,body);r[15]=(uint8_t)(i+1);catalog_put_u32(r+16,1);r[20]=1;r[21]=96;r[22]=20;r[23]=48;r[24]=4;memset(r+26,'W',96);memset(r+122,'A',20);memset(r+142,'T',48);uint8_t *d=r+190;d[0]=0xc5;d[1]=0;catalog_put_u32(d+2,1018);memset(d+6,0x41,1018);at+=body;}
 assert(!decode(&parsed,blob,at)&&parsed.count==32&&parsed.entries[31].ndef_length==1024);
 blob[12+2+15]=2;assert(decode(&parsed,blob,at));
 /* Atomic staging leaves prior committed bytes and RAM unchanged on failure. */
 n=unhex(blob,golden_hex);send_snapshot(7,blob,n,hash);assert(!catalog_storage_commit());assert(catalog_current.revision==7);assert(!catalog_storage_select(0)&&catalog_storage_selected()==0);
 uint8_t original[75];memcpy(original,mock_files[0].bytes,75);
 send_snapshot(8,blob,n,hash);mock_rename_failure=1;assert(catalog_storage_commit());assert(catalog_current.revision==7&&!memcmp(mock_files[0].bytes,original,75));
 send_snapshot(8,blob,n,hash);mock_close_failure=1;assert(catalog_storage_commit());assert(catalog_current.revision==7);
 send_snapshot(8,blob,n,hash);mock_files[1].bytes[40]^=1;assert(catalog_storage_commit());assert(catalog_current.revision==7);
 send_snapshot(8,blob,n,hash);assert(!catalog_storage_commit());assert(catalog_current.revision==8&&catalog_storage_selected()==-1);
 assert(catalog_storage_begin(8,1,(uint32_t)n,hash)==1);hash[0]^=1;assert(catalog_storage_begin(8,1,(uint32_t)n,hash)<0);
 assert(!catalog_storage_begin(9,1,(uint32_t)n,hash));mock_short_write=1;assert(catalog_storage_data(0,blob,10)<0&&!catalog_storage_active());
 memset(&catalog_current,0,sizeof(catalog_current));assert(!catalog_storage_load()&&catalog_current.revision==8);
 /* Bounded malformed input fuzzing under ASAN/UBSAN. */
 unsigned random=1;for(unsigned i=0;i<20000;i++){size_t count=i%256;for(size_t j=0;j<count;j++){random=random*1664525u+1013904223u;blob[j]=(uint8_t)(random>>24);}decode(&parsed,blob,count);}
 puts("catalog SHA256, Android golden fixture, UTF8/NDEF bounds, 32x1024 capacity, atomic storage/faults/reload: PASS");return 0;
}

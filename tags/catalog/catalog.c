#include "catalog.h"
#include "sha256.h"
uint32_t catalog_u32(const uint8_t *p){return(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
void catalog_put_u32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
int catalog_equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;unsigned v=0;for(size_t i=0;i<n;i++)v|=x[i]^y[i];return v==0;}
static void copy(void *a,const void *b,size_t n){uint8_t *x=a;const uint8_t *y=b;while(n--)*x++=*y++;}
static void zero(void *a,size_t n){uint8_t *p=a;while(n--)*p++=0;}
static unsigned u16(const uint8_t *p){return(unsigned)p[0]<<8|p[1];}
static int utf8(const uint8_t *p,size_t n){
 size_t i=0;while(i<n){uint32_t c=p[i++],min=0;unsigned more=0;
  if(c<0x80){if(c<32||c==127)return 0;continue;}
  if(c>=0xc2&&c<=0xdf){more=1;min=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){more=2;min=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){more=3;min=0x10000;c&=7;}else return 0;
  if(more>n-i)return 0;while(more--){uint8_t v=p[i++];if((v&0xc0)!=0x80)return 0;c=(c<<6)|(v&63);}
  if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff)||(c>=0x80&&c<=0x9f)||c==0x2028||c==0x2029)return 0;
 }return 1;
}
int catalog_ndef_valid(const uint8_t *p,size_t n){
 size_t at=0;unsigned records=0;int ended=0;
 while(at<n){if(n-at<2||ended)return 0;unsigned h=p[at++],type=p[at++],tnf=h&7;uint32_t payload;unsigned id=0;
  if((h&0x20)||tnf>=6||((h&0x80)!=0)!=(records==0))return 0;
  if(h&0x10){if(at==n)return 0;payload=p[at++];}else{if(n-at<4)return 0;payload=catalog_u32(p+at);at+=4;}
  if(h&8){if(at==n)return 0;id=p[at++];}
  if(type>n-at)return 0;at+=type;if(id>n-at)return 0;at+=id;if(payload>n-at)return 0;at+=payload;
  if((tnf==0&&(type||id||payload))||(tnf==5&&type))return 0;ended=(h&0x40)!=0;records++;
 }return records&&ended;
}
int catalog_find(const catalog_snapshot *c,const uint8_t id[16]){if(!c||!id)return-1;for(unsigned i=0;i<c->count;i++)if(catalog_equal(c->entries[i].id,id,16))return(int)i;return-1;}
int catalog_decode(catalog_snapshot *out,catalog_reader read,void *ctx,uint32_t total){
 if(!out||!read||total<12||total>CATALOG_MAX_BYTES)return-1;
 uint8_t head[12],body[1216],len[2];uint32_t consumed=12;catalog_sha256 hash;
 if(read(ctx,head,12)!=12||!catalog_equal(head,"TCAT",4)||u16(head+4)!=1||u16(head+6)>32||!catalog_u32(head+8))return-1;
 zero(out,sizeof(*out));out->count=(uint16_t)u16(head+6);out->revision=catalog_u32(head+8);catalog_sha256_init(&hash);catalog_sha256_update(&hash,head,12);
 for(unsigned i=0;i<out->count;i++){
  if(total-consumed<2||read(ctx,len,2)!=2)return-1;unsigned n=u16(len);consumed+=2;
  if(n<26||n>sizeof(body)||n>total-consumed||read(ctx,body,n)!=n)return-1;consumed+=n;
  catalog_sha256_update(&hash,len,2);catalog_sha256_update(&hash,body,n);
  unsigned name=body[21],uid=body[22],tech=body[23],ndef=u16(body+24),kind=body[20];
  if(!name||name>96||uid>20||(uid&1)||tech>48||ndef>1024||kind>1||(!kind&&ndef)||(kind&&!ndef)||n!=26+name+uid+tech+ndef||!catalog_u32(body+16)||!utf8(body+26,name))return-1;
  uint8_t any=0;for(unsigned j=0;j<16;j++)any|=body[j];if(!any)return-1;
  for(unsigned j=0;j<i;j++)if(catalog_equal(body,out->entries[j].id,16))return-1;
  const uint8_t *u=body+26+name,*t=u+uid,*d=t+tech;
  for(unsigned j=0;j<uid;j++)if(!((u[j]>='0'&&u[j]<='9')||(u[j]>='A'&&u[j]<='F')))return-1;
  for(unsigned j=0;j<tech;j++)if(t[j]<32||t[j]>126)return-1;
  if(kind&&!catalog_ndef_valid(d,ndef))return-1;
  tag_catalog_entry *e=&out->entries[i];copy(e->id,body,16);e->revision=catalog_u32(body+16);e->kind=(uint8_t)kind;e->ndef_length=(uint16_t)ndef;
  copy(e->name,body+26,name);copy(e->uid,u,uid);copy(e->technology,t,tech);catalog_sha256_digest(d,ndef,e->ndef_sha256);
 }
 if(consumed!=total)return-1;uint8_t extra;if(read(ctx,&extra,1)!=0)return-1;catalog_sha256_final(&hash,out->sha256);return 0;
}

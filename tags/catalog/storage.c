#include "storage.h"
#include "sha256.h"
catalog_snapshot catalog_current;
static catalog_snapshot candidate;
static uint32_t transaction_revision,transaction_total,transaction_offset;
static uint8_t transaction_count,transaction_hash[32],transaction_active;
static void copy(void *a,const void *b,size_t n){uint8_t *d=a;const uint8_t *s=b;while(n--)*d++=*s++;}
static size_t read_cb(void *ctx,void *out,size_t n){return catalog_fs_read(out,1,n,ctx);}
static int write_file(const char *path,const char *mode,const void *bytes,size_t length){
 void *f=catalog_fs_open(path,mode);if(!f)return-1;size_t n=catalog_fs_write(bytes,1,length,f);int rc=catalog_fs_close(f);return n==length&&!rc?0:-1;
}
int catalog_storage_active(void){return transaction_active;}
uint32_t catalog_storage_offset(void){return transaction_offset;}
void catalog_storage_abort(void){transaction_active=0;transaction_offset=transaction_total=0;}
int catalog_storage_begin(uint32_t rev,uint8_t count,uint32_t total,const uint8_t hash[32]){
 if(!rev||count>32||total<12||total>CATALOG_MAX_BYTES||!hash)return-1;
 if(rev<catalog_current.revision)return-1;
 if(rev==catalog_current.revision){if(!catalog_equal(hash,catalog_current.sha256,32))return-1;catalog_storage_abort();return 1;}
 catalog_storage_abort();if(write_file(CATALOG_STAGE_PATH,"w","",0))return-2;
 transaction_revision=rev;transaction_count=count;transaction_total=total;copy(transaction_hash,hash,32);transaction_active=1;return 0;
}
int catalog_storage_data(uint32_t offset,const uint8_t *bytes,size_t n){
 if(!transaction_active||offset!=transaction_offset||!bytes||!n||n>240||n>transaction_total-offset)return-1;
 if(write_file(CATALOG_STAGE_PATH,"a",bytes,n)){catalog_storage_abort();return-2;}
 transaction_offset+=(uint32_t)n;return 0;
}
int catalog_storage_commit(void){
 if(!transaction_active||transaction_offset!=transaction_total)return-1;
 void *f=catalog_fs_open(CATALOG_STAGE_PATH,"r");if(!f)return-2;
 int rc=catalog_decode(&candidate,read_cb,f,transaction_total),closed=catalog_fs_close(f);
 if(rc||closed||candidate.revision!=transaction_revision||candidate.count!=transaction_count||!catalog_equal(candidate.sha256,transaction_hash,32)){catalog_storage_abort();return-3;}
 if(catalog_fs_rename(CATALOG_STAGE_PATH,CATALOG_DATA_PATH)){catalog_storage_abort();return-4;}
 catalog_current=candidate;catalog_storage_abort();return 0;
}
int catalog_storage_load(void){
 catalog_storage_abort();void *f=catalog_fs_open(CATALOG_DATA_PATH,"r");if(!f)return 1;
 uint8_t bytes[512];uint32_t total=0;size_t n;
 do{n=catalog_fs_read(bytes,1,sizeof(bytes),f);total+=(uint32_t)n;}while(n==sizeof(bytes)&&total<=CATALOG_MAX_BYTES);
 int closed=catalog_fs_close(f);if(closed||total>CATALOG_MAX_BYTES)return-1;
 f=catalog_fs_open(CATALOG_DATA_PATH,"r");if(!f)return-1;
 int rc=catalog_decode(&candidate,read_cb,f,total);closed=catalog_fs_close(f);
 if(rc||closed)return-1;catalog_current=candidate;return 0;
}
int catalog_storage_select(int index){
 if(index<0||index>=catalog_current.count)return-1;
 const tag_catalog_entry *e=&catalog_current.entries[index];uint8_t bytes[60];
 copy(bytes,"TSEL",4);catalog_put_u32(bytes+4,catalog_current.revision);copy(bytes+8,e->id,16);catalog_put_u32(bytes+24,e->revision);copy(bytes+28,e->ndef_sha256,32);
 if(write_file("/data/tag_catalog/selected.next","w",bytes,sizeof(bytes)))return-1;
 return catalog_fs_rename("/data/tag_catalog/selected.next","/data/tag_catalog/selected.bin")?-1:0;
}
int catalog_storage_selected(void){
 uint8_t bytes[61];void *f=catalog_fs_open("/data/tag_catalog/selected.bin","r");if(!f)return-1;
 size_t n=catalog_fs_read(bytes,1,sizeof(bytes),f);int rc=catalog_fs_close(f);
 if(n!=60||rc||!catalog_equal(bytes,"TSEL",4)||catalog_u32(bytes+4)!=catalog_current.revision)return-1;
 int at=catalog_find(&catalog_current,bytes+8);if(at<0)return-1;const tag_catalog_entry *e=&catalog_current.entries[at];
 return e->revision==catalog_u32(bytes+24)&&catalog_equal(e->ndef_sha256,bytes+28,32)?at:-1;
}
#ifdef TAG_CATALOG_LOCAL_PROFILE
int catalog_storage_ndef(int index,uint8_t *out,uint16_t capacity,uint16_t *length){
 if(index<0||index>=(int)catalog_current.count||!out||!length||!capacity)return-1;
 unsigned wanted_index=(unsigned)index;
 const tag_catalog_entry *wanted=&catalog_current.entries[index];
 if(!wanted->kind||!wanted->ndef_length||wanted->ndef_length>capacity)return-1;
 void *f=catalog_fs_open(CATALOG_DATA_PATH,"r");if(!f)return-2;
 uint8_t head[12],size_bytes[2],body[1216];
 size_t got=catalog_fs_read(head,1,sizeof(head),f);
 if(got!=sizeof(head)||!catalog_equal(head,"TCAT",4)||head[4]||head[5]!=1||
    head[6]||head[7]!=catalog_current.count){
  (void)catalog_fs_close(f);return-2;
 }
 for(unsigned i=0;i<catalog_current.count;i++){
  if(catalog_fs_read(size_bytes,1,sizeof(size_bytes),f)!=sizeof(size_bytes)){
   (void)catalog_fs_close(f);return-2;
  }
  unsigned n=(unsigned)size_bytes[0]<<8|size_bytes[1];
  if(n<26||n>sizeof(body)||catalog_fs_read(body,1,n,f)!=n){
   (void)catalog_fs_close(f);return-2;
  }
  if(i!=wanted_index)continue;
  unsigned name=body[21],uid=body[22],tech=body[23],ndef=(unsigned)body[24]<<8|body[25];
  int kind=body[20];
  if(n!=26+name+uid+tech+ndef||kind!=1||ndef!=wanted->ndef_length||
     ndef>capacity||ndef>sizeof(body)-26){
   (void)catalog_fs_close(f);return -3;
  }
  const uint8_t *data=body+26+name+uid+tech;
  uint8_t digest[32];catalog_sha256_digest(data,ndef,digest);
  if(!catalog_equal(digest,wanted->ndef_sha256,32)){
   (void)catalog_fs_close(f);return-3;
  }
  copy(out,data,ndef);*length=(uint16_t)ndef;
  if(catalog_fs_close(f))return-2;
  return 0;
 }
 (void)catalog_fs_close(f);return-3;
}
#endif

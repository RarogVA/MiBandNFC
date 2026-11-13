#include "storage.h"
#include "fs_fingerprints.h"
int catalog_storage_verified(void){
 for(unsigned i=0;i<sizeof(fs_checks)/sizeof(fs_checks[0]);i++){
  const volatile uint8_t *p=(const volatile uint8_t *)fs_checks[i].address;
  for(unsigned j=0;j<24;j++)if(p[j]!=fs_checks[i].bytes[j])return 0;
 }return 1;
}
void *catalog_fs_open(const char *p,const char *m){return((void *(*)(const char *,const char *))(uintptr_t)0x2c1b9365u)(p,m);}
size_t catalog_fs_read(void *p,size_t s,size_t n,void *f){return((size_t (*)(void *,size_t,size_t,void *))(uintptr_t)0x2c1b9569u)(p,s,n,f);}
size_t catalog_fs_write(const void *p,size_t s,size_t n,void *f){return((size_t (*)(const void *,size_t,size_t,void *))(uintptr_t)0x2c1b9ad5u)(p,s,n,f);}
int catalog_fs_close(void *f){return((int (*)(void *))(uintptr_t)0x2c1b9439u)(f);}
int catalog_fs_rename(const char *a,const char *b){return((int (*)(const char *,const char *))(uintptr_t)0x2c19fbe9u)(a,b);}

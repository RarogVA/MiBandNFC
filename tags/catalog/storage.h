#ifndef TAG_CATALOG_STORAGE_H
#define TAG_CATALOG_STORAGE_H
#include "catalog.h"
#define CATALOG_DATA_PATH "/data/tag_catalog/catalog.bin"
#define CATALOG_STAGE_PATH "/data/tag_catalog/catalog.next"
extern catalog_snapshot catalog_current;
int catalog_storage_verified(void);
int catalog_storage_load(void);
int catalog_storage_begin(uint32_t rev,uint8_t count,uint32_t total,const uint8_t hash[32]);
int catalog_storage_data(uint32_t offset,const uint8_t *bytes,size_t length);
int catalog_storage_commit(void);
void catalog_storage_abort(void);
int catalog_storage_active(void);
uint32_t catalog_storage_offset(void);
int catalog_storage_select(int index);
int catalog_storage_selected(void);
/* Backend primitives are exposed only for the core's fixed diagnostic/API
 * descriptors. All paths are compile-time constants, never user names. */
void *catalog_fs_open(const char *,const char *);
size_t catalog_fs_read(void *,size_t,size_t,void *);
size_t catalog_fs_write(const void *,size_t,size_t,void *);
int catalog_fs_close(void *);
int catalog_fs_rename(const char *,const char *);
#endif

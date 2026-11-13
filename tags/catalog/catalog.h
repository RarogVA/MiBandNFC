#ifndef TAG_CATALOG_H
#define TAG_CATALOG_H
#include "api.h"
#define CATALOG_MAX_BYTES 65536u
typedef struct {uint32_t revision;uint16_t count;uint16_t reserved;uint8_t sha256[32];tag_catalog_entry entries[32];} catalog_snapshot;
typedef size_t (*catalog_reader)(void *ctx,void *out,size_t length);
int catalog_decode(catalog_snapshot *out,catalog_reader read,void *ctx,uint32_t total);
int catalog_find(const catalog_snapshot *catalog,const uint8_t id[16]);
int catalog_equal(const void *a,const void *b,size_t n);
uint32_t catalog_u32(const uint8_t *p);
void catalog_put_u32(uint8_t *p,uint32_t n);
int catalog_ndef_valid(const uint8_t *p,size_t n);
#endif

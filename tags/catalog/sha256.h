#ifndef CATALOG_SHA256_H
#define CATALOG_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct {uint32_t h[8];uint64_t bytes;uint8_t block[64];unsigned used;} catalog_sha256;
void catalog_sha256_init(catalog_sha256 *s);
void catalog_sha256_update(catalog_sha256 *s,const void *data,size_t n);
void catalog_sha256_final(catalog_sha256 *s,uint8_t out[32]);
void catalog_sha256_digest(const void *data,size_t n,uint8_t out[32]);
#endif

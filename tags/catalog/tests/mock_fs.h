#ifndef CATALOG_MOCK_FS_H
#define CATALOG_MOCK_FS_H
#include <assert.h>
#include <string.h>
#include "storage.h"
typedef struct{uint8_t bytes[65537];size_t length,pos;int exists;} mock_file;
static mock_file mock_files[6];
static int mock_short_write,mock_close_failure,mock_rename_failure;
int catalog_storage_verified(void){return 1;}
static int file_index(const char *p){if(!strcmp(p,CATALOG_DATA_PATH))return 0;if(!strcmp(p,CATALOG_STAGE_PATH))return 1;if(!strcmp(p,"/data/tag_catalog/selected.next"))return 2;if(!strcmp(p,"/data/tag_catalog/selected.bin"))return 3;if(!strcmp(p,"/tmp/mihome_action"))return 4;if(!strcmp(p,"/tmp/mihome_status"))return 5;assert(!"unexpected filesystem path");return-1;}
void *catalog_fs_open(const char *p,const char *mode){mock_file *f=&mock_files[file_index(p)];if(mode[0]=='r'){if(!f->exists)return 0;f->pos=0;}else if(mode[0]=='w'){f->exists=1;f->length=f->pos=0;}else{assert(mode[0]=='a');f->exists=1;f->pos=f->length;}return f;}
size_t catalog_fs_read(void *p,size_t size,size_t n,void *file){assert(size==1);mock_file *f=file;if(n>f->length-f->pos)n=f->length-f->pos;memcpy(p,f->bytes+f->pos,n);f->pos+=n;return n;}
size_t catalog_fs_write(const void *p,size_t size,size_t n,void *file){assert(size==1);mock_file *f=file;if(mock_short_write&&n){mock_short_write=0;n--;}assert(n<=sizeof(f->bytes)-f->pos);memcpy(f->bytes+f->pos,p,n);f->pos+=n;if(f->pos>f->length)f->length=f->pos;return n;}
int catalog_fs_close(void *file){(void)file;if(mock_close_failure){mock_close_failure=0;return-1;}return 0;}
int catalog_fs_rename(const char *a,const char *b){if(mock_rename_failure){mock_rename_failure=0;return-1;}int from=file_index(a),to=file_index(b);assert(mock_files[from].exists);mock_files[to]=mock_files[from];mock_files[from].exists=0;return 0;}
static size_t unhex(uint8_t *out,const char *s){size_t n=0;while(*s){unsigned x=0;for(unsigned i=0;i<2;i++){unsigned c=(unsigned char)*s++;x=x*16+(c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c-'A'+10);}out[n++]=(uint8_t)x;}return n;}
static const char golden_hex[]="544341540001000100000007003D00112233445566778899AABBCCDDEEFF000000090108000400174E46432D546573744E444546D101135402727548656C6C6F2D66726F6D2D506978656C";
static const char golden_sha[]="599ab419f874e160b013998284ef81cb93f5685bb153091282c84fc7ee9d4140";
#endif

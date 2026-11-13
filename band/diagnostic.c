#include "diagnostic.h"
#include <stddef.h>
#include <stdint.h>

#define STAGE_MAX 48u
#define RECORD_MAX 96u
#ifndef DIAGNOSTIC_STATE_PATH
#define DIAGNOSTIC_STATE_PATH "/tmp/apdu_lab_state"
#endif
_Static_assert(sizeof(int)==4,"diagnostic code is signed 32-bit");
_Static_assert(RECORD_MAX>=6+STAGE_MAX+1+3+11+1+6+10+1,"record capacity");

#ifdef DIAGNOSTIC_HOST_TEST
extern int diagnostic_hw_verified(void);
extern void *diagnostic_hw_open(const char *,const char *);
extern size_t diagnostic_hw_write(const void *,size_t,size_t,void *);
extern int diagnostic_hw_close(void *);
#else
static int diagnostic_hw_verified(void) {
#if defined(__arm__) && defined(__thumb__)
    static const struct { uintptr_t address; uint8_t first[24]; } checks[]={
        {0x2c1b9364u,{0xb0,0xb5,0x05,0x46,0x00,0xaf,0x08,0x46,0x0c,0x46,0xff,0xf7,0x31,0xff,0x01,0x1e,0x0f,0xdb,0x28,0x46,0x4f,0xf4,0xdb,0x72}},
        {0x2c1b9ad4u,{0x2d,0xe9,0xf0,0x41,0x1d,0x46,0x0c,0x46,0x16,0x46,0x80,0x46,0x00,0xaf,0x18,0x46,0x00,0xf0,0x48,0xfb,0x06,0xfb,0x04,0xf1}},
        {0x2c1b9438u,{0x2d,0xe9,0xf0,0x41,0x00,0xaf,0x00,0x28,0x00,0xf0,0x88,0x80,0xb0,0xf8,0x7c,0x30,0x04,0x46,0x13,0xf0,0x02,0x05,0x5a,0xd1}}
    };
    for(size_t i=0;i<sizeof(checks)/sizeof(checks[0]);i++) {
        const volatile uint8_t *p=(const volatile uint8_t *)checks[i].address;
        for(size_t j=0;j<24;j++)if(p[j]!=checks[i].first[j])return -1;
    }
    return 1;
#else
    return -1;
#endif
}
static void *diagnostic_hw_open(const char *path,const char *mode) {
    return ((void *(*)(const char *,const char *))(uintptr_t)0x2c1b9365u)(path,mode);
}
static size_t diagnostic_hw_write(const void *p,size_t size,size_t count,void *file) {
    return ((size_t (*)(const void *,size_t,size_t,void *))(uintptr_t)0x2c1b9ad5u)
        (p,size,count,file);
}
static int diagnostic_hw_close(void *file) {
    return ((int (*)(void *))(uintptr_t)0x2c1b9439u)(file);
}
#endif

static int verified,have_last,last_code;
static uint32_t last_detail;
static size_t last_length;
static char last_stage[STAGE_MAX+1];

static size_t normalize(char *out,const char *stage) {
    size_t n=0;
    if(!stage)stage="unknown";
    while(n<STAGE_MAX && stage[n]) {
        unsigned char c=(unsigned char)stage[n];
        out[n]=(c>=32 && c<=126)?(char)c:'_';n++;
    }
    out[n]=0;return n;
}
static int same(const char *a,const char *b,size_t n) {
    for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static void literal(char *out,size_t *at,const char *value) {
    while(*value && *at<RECORD_MAX)out[(*at)++]=*value++;
}
static void decimal(char *out,size_t *at,uint32_t value) {
    char reverse[10];size_t n=0;
    do {reverse[n++]=(char)('0'+value%10u);value/=10u;}while(value);
    while(n && *at<RECORD_MAX)out[(*at)++]=reverse[--n];
}
void diagnostic_set(const char *stage,int code,uint32_t detail) {
    char normalized[STAGE_MAX+1];size_t n=normalize(normalized,stage);
    if(have_last && n==last_length && code==last_code && detail==last_detail &&
       same(normalized,last_stage,n))return;
    /* Cache the attempt, preventing an unavailable filesystem from becoming a
     * 10-ms retry loop. This file is diagnostic only, never a control channel. */
    have_last=1;last_length=n;last_code=code;last_detail=detail;
    for(size_t i=0;i<=n;i++)last_stage[i]=normalized[i];
    if(!verified)verified=diagnostic_hw_verified()>0?1:-1;
    if(verified<0)return;

    char record[RECORD_MAX];size_t at=0;
    literal(record,&at,"stage=");literal(record,&at,normalized);
    literal(record,&at,"\nrc=");
    uint32_t magnitude=(uint32_t)code;
    if(code<0) {literal(record,&at,"-");magnitude=0u-magnitude;}
    decimal(record,&at,magnitude);
    literal(record,&at,"\nstate=");decimal(record,&at,detail);
    literal(record,&at,"\n");
    void *file=diagnostic_hw_open(DIAGNOSTIC_STATE_PATH,"w");
    if(!file)return;
    /* One small write; close even after a short write. Failures are deliberately
     * silent and never recurse through the module's diagnostics or logger. */
    (void)diagnostic_hw_write(record,1,at,file);
    (void)diagnostic_hw_close(file);
}
#ifdef DIAGNOSTIC_HOST_TEST
void diagnostic_test_reset(void) {
    verified=have_last=last_code=0;last_detail=0;last_length=0;last_stage[0]=0;
}
#endif

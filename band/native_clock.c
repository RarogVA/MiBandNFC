#include "native_clock.h"
#include <stddef.h>
typedef struct { int64_t seconds; int32_t nanoseconds; int32_t padding; } native_timespec_t;
_Static_assert(sizeof(native_timespec_t)==16,"exact timespec allocation");
_Static_assert(offsetof(native_timespec_t,nanoseconds)==8,"exact nanoseconds offset");
static int verified;
static int verify_clock(void) {
    static const uint8_t first[24]={0xf0,0xb5,0x85,0xb0,0x00,0xaf,0x00,0x29,0x5a,0xd0,0x04,0x28,0x58,0xd8,0x01,0x28,0x0c,0x46,0x0d,0xd0,0x04,0x28,0x0b,0xd0};
    static const uint8_t second[24]={0x00,0x22,0x00,0x23,0x2d,0xe9,0xf0,0x43,0x04,0x46,0x83,0xb0,0x00,0xaf,0x38,0x46,0xc7,0xe9,0x00,0x23,0xb4,0xf7,0x4a,0xf9};
    const volatile uint8_t *a=(const volatile uint8_t *)(uintptr_t)0x2c1c8e70u;
    const volatile uint8_t *b=(const volatile uint8_t *)(uintptr_t)0x2c1c9024u;
    for(unsigned i=0;i<24;i++)if(a[i]!=first[i] || b[i]!=second[i])return -1;
    return 1;
}
int native_clock_ms(uint32_t *milliseconds) {
    if(!milliseconds)return -1;
    if(!verified)verified=verify_clock();
    if(verified<0)return -1;
    native_timespec_t time;
    time.seconds=0;time.nanoseconds=0;time.padding=0;
    int (*clock_gettime_exact)(int,native_timespec_t *)=
        (int (*)(int,native_timespec_t *))(uintptr_t)0x2c1c8e71u;
    if(clock_gettime_exact(1,&time)!=0 || time.seconds<0 ||
       time.nanoseconds<0 || time.nanoseconds>=1000000000)return -1;
    *milliseconds=(uint32_t)time.seconds*1000u+(uint32_t)time.nanoseconds/1000000u;
    return 0;
}

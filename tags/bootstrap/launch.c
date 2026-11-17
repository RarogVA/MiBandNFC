#include "launch.h"
#include <stddef.h>

/* The ROM Widget builds this 44-byte request on the stack. Animation records
 * are {callback, context, unknown}; both firmware callbacks explicitly accept
 * NULL context and then choose the normal transition defaults. */
typedef struct {
    uint32_t reserved0;
    uint16_t reserved4;
    uint16_t app_id;
    uint8_t launch_kind;
    uint8_t reserved9[3];
    uint32_t app_data;
    uint32_t app_name;
    uint32_t enter_callback;
    uint32_t enter_context;
    uint32_t enter_reserved;
    uint32_t exit_callback;
    uint32_t exit_context;
    uint32_t exit_reserved;
} launch_request;
_Static_assert(sizeof(launch_request)==44,"Firmware launch request size");
_Static_assert(offsetof(launch_request,app_id)==6,"Firmware app id offset");
_Static_assert(offsetof(launch_request,enter_callback)==20,"Enter animation offset");
_Static_assert(offsetof(launch_request,exit_callback)==32,"Exit animation offset");

static const struct {uintptr_t address;uint8_t bytes[24];} checks[]={
 {0x2c7aed1cu,{0x2d,0xe9,0xf0,0x47,0xdf,0xf8,0xd0,0x90,0x04,0x46,0xd9,0xf8,0x18,0x80,0xc5,0x88,0xd0,0xf8,0x10,0xa0,0x00,0xaf,0xb8,0xf1}},
 {0x2c787ee4u,{0x2d,0xe9,0xf0,0x43,0x87,0xb0,0x81,0x46,0x88,0x46,0x14,0x46,0x06,0xaf,0x00,0x2a,0x35,0xd0,0xd4,0xe9,0x0b,0x56,0xd2,0xe9}},
 {0x2c787f74u,{0x2d,0xe9,0xf0,0x43,0x87,0xb0,0x81,0x46,0x88,0x46,0x14,0x46,0x06,0xaf,0x00,0x2a,0x36,0xd0,0xd4,0xe9,0x0b,0x56,0xd2,0xe9}},
 {0x2c2025d4u,{0xb0,0xb5,0x8e,0xb0,0x05,0x46,0x02,0xaf,0x00,0x28,0x45,0xd0,0x07,0xf1,0x1c,0x00,0xb6,0xf7,0x76,0xfb,0x04,0x46,0x00,0x28}}
};

int tag_bootstrap_bindings_verified(void) {
    for(unsigned i=0;i<sizeof(checks)/sizeof(checks[0]);i++) {
        const volatile uint8_t *p=(const volatile uint8_t *)checks[i].address;
        for(unsigned j=0;j<24;j++)if(p[j]!=checks[i].bytes[j])return 0;
    }
    uintptr_t launch=*(volatile uintptr_t *)(uintptr_t)0x200b7368u;
    return launch==0x0c7aed1du || launch==0x2c7aed1du;
}

int tag_bootstrap_open_app(uint16_t app_id) {
    if(app_id!=0xd4u || !tag_bootstrap_bindings_verified())return -1;
    launch_request request={0};
    request.app_id=app_id;
    request.launch_kind=9;
    request.enter_callback=0x0c787ee5u;
    request.exit_callback=0x0c787f75u;
    int (*launch)(const launch_request *)=*(int (**)(const launch_request *))(uintptr_t)0x200b7368u;
    return launch(&request);
}

int tag_bootstrap_insmod_core(void) {
    if(!tag_bootstrap_bindings_verified())return -1;
    /* Caller must check the resident native provider and legacy/attempt guards
     * before invoking this. A successful system status is not a health check. */
    return ((int (*)(const char *))(uintptr_t)0x2c2025d5u)(
        "insmod /data/tag_catalog/core-catalog.elf Tag_Catalog_V1");
}

int tag_bootstrap_reboot(void) {
    if(!tag_bootstrap_bindings_verified())return -1;
    /* NSH command table entry "shutdown" -> boardctl(BOARDIOC_RESET, 0).
     * No user/permission check exists in this build. */
    return ((int (*)(const char *))(uintptr_t)0x2c2025d5u)("shutdown --reboot");
}

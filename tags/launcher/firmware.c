#include "bindings.h"
#include "fingerprints.h"
#ifdef TAG_UI_SCROLL_V2
#include "scroll_fingerprints.h"
#endif
int ui_firmware_verified(void) {
    for(unsigned i=0;i<sizeof(checks)/sizeof(checks[0]);i++) {
        const volatile uint8_t *p=(const volatile uint8_t *)checks[i].address;
        for(unsigned j=0;j<24;j++)if(p[j]!=checks[i].bytes[j])return 0;
    }
#ifdef TAG_UI_SCROLL_V2
    for(unsigned i=0;i<sizeof(scroll_checks)/sizeof(scroll_checks[0]);i++) {
        const volatile uint8_t *p=(const volatile uint8_t *)scroll_checks[i].address;
        for(unsigned j=0;j<24;j++)if(p[j]!=scroll_checks[i].bytes[j])return 0;
    }
#endif
    /* Initialized PM table is in SRAM. Accept only the exact .026 install
     * function's two executable aliases; never call an arbitrary RAM pointer. */
    uintptr_t install=*(volatile uintptr_t *)(uintptr_t)0x200b72e0u;
    return install==0x0c7abb69u || install==0x2c7abb69u;
}

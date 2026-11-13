/* Reuse the established stdio bindings and their exact firmware checks.
 * This translation unit replaces band/diagnostic.c in the tag-profile build. */
#include "../../band/diagnostic.c"
#include "tag_display.h"

static int display_checked;
int tag_display_write(const tag_record *record,const char *state) {
    if(!state)return -1;
    if(!display_checked)display_checked=diagnostic_hw_verified()>0?1:-1;
    if(display_checked<0)return -1;
    char data[160];size_t at=0;
    const char *fields[]={"TAG1",record?record->name:"No tag selected",record?record->uid:"",
        record?record->technology:"",record&&record->supported?"1":"0",state,"END"};
    for(unsigned i=0;i<7;i++) {
        const char *p=fields[i];
        while(*p && at<sizeof(data)-1)data[at++]=*p++;
        if(*p || at>=sizeof(data))return -1;
        data[at++]='\n';
    }
    void *file=diagnostic_hw_open("/tmp/tag_catalog_state","w");
    if(!file)return -1;
    size_t written=diagnostic_hw_write(data,1,at,file);
    int closed=diagnostic_hw_close(file);
    return written==at && closed==0 ? 0 : -1;
}
#ifdef DIAGNOSTIC_HOST_TEST
void tag_display_test_reset(void) {display_checked=0;}
#endif

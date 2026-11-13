/* Minimal .026 native-app ABI derived from MiWatchNativeApps (MIT).
 * See THIRD_PARTY_LICENSE and README.md for pinned source/provenance. */
#ifndef TAG_UI_ABI_H
#define TAG_UI_ABI_H
#include <stddef.h>
#include <stdint.h>
typedef struct ui_obj ui_obj;
typedef struct ui_event ui_event;
typedef struct ui_timer ui_timer;
typedef int (*page_callback)(void *,ui_obj *,void *);
typedef struct {
    int proto_app,field_4;
    const char *name,*icon;
    uint16_t app_id,flags;
    int field_14,field_18;
    const char *(*get_name)(void);
    void *on_uninstall;
    int field_24,field_28,field_2c,field_30,field_34;
    void *on_signal;
    int field_3c;
} ui_app;
typedef struct {
    void *parent_descriptor;
    uint8_t pad4[12];
    const char *name;
    uint16_t page_id,app_id,flags;
    uint8_t pad1a[2];
    int32_t scheduler_deadline,scheduler_priority;
    uint32_t async_destroy_state;
    uint8_t lifecycle_state,layer,page_kind,pad2b;
    void *activity_context,*root_object,*on_signal,*runtime_default_56;
    uint8_t pad3c[4];
    void *registry_prev,*registry_next,*runtime_parent;
    page_callback on_create,on_resume,on_newdata,on_start,on_pause,on_stop,on_destroy,on_back,on_keyevent;
    void *get_scroll_obj;
} ui_page;
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(ui_app)==0x40,".026 app ABI");
_Static_assert(sizeof(ui_page)==0x74,".026 page ABI");
_Static_assert(offsetof(ui_app,get_name)==0x1c,".026 name callback");
_Static_assert(offsetof(ui_page,on_create)==0x4c,".026 create callback");
_Static_assert(offsetof(ui_page,on_destroy)==0x64,".026 destroy callback");
#endif
#endif

/* Compact catalog UI. No RF start and no background UI polling. */
#include "bindings.h"
#include "catalog_ui_bindings.h"
#include "../catalog/api.h"
#include "../bootstrap/launch.h"
#define APP_ID 0xd4u
#define PROVIDER_ID 0xd6u
static const char app_name[]="dev.mbapdu.tags";
static ui_app app;
static ui_page page,*pages[1];
static ui_obj *title,*badge,*module_badge,*module_text,*previous,*next;
static ui_timer *refresh_timer,*load_timer;
static unsigned refresh_count,initialized;
static int initialization_result=-1;
static uint32_t revision;
static int current=-1,total;
static const tag_catalog_api *api;

static int equal(const char *a,const char *b) {
    if(!a||!b)return 0;
    while(*a&&*a==*b){a++;b++;}return *a==*b;
}
static int file_read(const char *path,char *out,size_t cap) {
    void *f=ui_open(path,"r");if(!f)return -1;
    size_t n=ui_read(out,1,cap-1,f);int rc=ui_close(f);
    if(rc)return -1;out[n]=0;return (int)n;
}
static int file_write(const char *path,const char *text) {
    size_t n=0;while(text[n])n++;
    void *f=ui_open(path,"w");if(!f)return -1;
    size_t wrote=ui_write(text,1,n,f);int closed=ui_close(f);
    return wrote==n&&!closed?0:-1;
}
static void diagnostic(const char *text) {(void)file_write("/tmp/nfc_tags_ui_state",text);}
static void notice(const char *text) {(void)file_write("/tmp/nfc_tags_ui_notice",text);}
static int exists(const char *path) {
    void *f=ui_open(path,"r");if(!f)return 0;
    /* An existing load guard must remain a guard even if close fails. */
    (void)ui_close(f);return 1;
}
static const tag_catalog_api *provider(void) {
    /* Discover through the firmware registry, never execute a pointer read
     * from a writable file. The provider owns this immutable resident blob. */
    ui_app *p=ui_lookup(PROVIDER_ID);
    if(!p||!equal(p->name,"dev.mbapdu.catalog.api.v1")||!p->get_name)return 0;
    const char *blob=p->get_name();
    if(!blob)return 0;
    static const char expected[32]="NFC Catalog API v1";
    for(unsigned i=0;i<32;i++)if(blob[i]!=expected[i])return 0;
    const tag_catalog_api *a=(const tag_catalog_api *)(const void *)(blob+32);
    if(a->magic!=TAG_CATALOG_API_MAGIC||a->version!=TAG_CATALOG_API_VERSION||
       a->bytes!=sizeof(*a)||a->profile!=TAG_CATALOG_API_PROFILE||
       !a->health||!a->get||!a->select||!a->stop)return 0;
    return a;
}
static void pads(ui_obj *o,int n) {
    ui_pad_left(o,n,0);ui_pad_right(o,n,0);ui_pad_top(o,n,0);ui_pad_bottom(o,n,0);
}
static ui_obj *text(ui_obj *parent,const char *value,int x,int y,int w,int h,unsigned size) {
    ui_obj *o=ui_label(parent);if(!o)return 0;
    uintptr_t style=size==22?0x200d689cu:size==20?0x200d6890u:0x200d6884u;
    ui_style(o,(void *)style,255,0);pads(o,0);ui_size(o,w,h);
    ui_align(o,1,x,y);ui_text_align(o,2,0);ui_long_mode(o,1);ui_text(o,value);return o;
}
static ui_obj *button(ui_obj *root,int x,int y,int w,int h,void (*cb)(ui_event *)) {
    ui_obj *o=ui_button(root,0);if(!o)return 0;
    pads(o,0);ui_size(o,w,h);ui_align(o,1,x,y);ui_bg(o,0xff252b34u,0);ui_opa(o,255,0);
    ui_clear_flag(o,0x10u);
    if(cb)ui_event_cb(o,cb,7,0);else ui_clear_flag(o,2u);
    return o;
}
static void nav_icon(ui_obj *parent,const char *path,const char *fallback) {
    if(exists(path)) {
        ui_obj *o=ui_image(parent,0);
        if(o){ui_image_src(o,path);ui_size(o,32,32);ui_align(o,9,0,0);return;}
    }
    ui_obj *o=text(parent,fallback,0,0,32,32,22);if(o)ui_align(o,9,0,0);
}
static int contains(const char *value,const char *part) {
    for(;*value;value++){unsigned i=0;while(part[i]&&part[i]==value[i])i++;if(!part[i])return 1;}
    return 0;
}
static void update(void) {
    if(!title)return;
    tag_catalog_health health;
    api=provider();int available=api&&api->health(&health)==0;
    int healthy=available&&health.loop_live&&!health.error;
    ui_text(module_text,healthy?"OK":"FAIL");
    ui_bg(module_badge,healthy?0xff195d39u:0xff842f3bu,0);
    if(!available){current=-1;total=0;revision=0;ui_text(title,"Нет модуля");ui_text(badge,"—");return;}
    revision=health.catalog_revision;total=health.count;current=health.selected_index;
    if(current<0||current>=total){ui_text(title,total?"Выберите метку":"Нет меток");ui_text(badge,"—");return;}
    tag_catalog_entry entry;
    if(api->get(revision,(uint16_t)current,&entry)!=0){ui_text(title,"Обновите данные");return;}
    ui_text(title,entry.name);
    const char *kind=entry.kind==1?"NDEF":contains(entry.technology,"IsoDep")?"ISO-DEP":
        contains(entry.technology,"NfcA")?"NFC-A":contains(entry.technology,"NfcB")?"NFC-B":"Сведения";
    ui_text(badge,kind);
    /* The badge describes type only, not UID emulation or RF readiness. */
    ui_bg(previous,current>0?0xff252b34u:0xff12161cu,0);
    ui_bg(next,current+1<total?0xff252b34u:0xff12161cu,0);
}
static void refresh_later(ui_timer *t) {
    if(!title||!refresh_count){ui_timer_delete(t);refresh_timer=0;return;}
    update();
    if(!--refresh_count){ui_timer_delete(t);refresh_timer=0;}
}
static void delayed_refresh(void) {
    refresh_count=40;
    if(!refresh_timer)refresh_timer=ui_timer_create(refresh_later,500,0);
}
static void choose(int delta) {
    if(!title)return;
    update();
    int index=current<0?0:current+delta;
    if(!api||index<0||index>=total)return;
    tag_catalog_entry entry;
    if(api->get(revision,(uint16_t)index,&entry))return;
    int rc=api->select(revision,entry.id,entry.revision);
    notice(rc==0?"":rc==-2?"Дождитесь остановки NFC":"Обновите каталог на Pixel");
    if(!rc)delayed_refresh();
}
static void go_previous(ui_event *e){(void)e;choose(-1);}
static void go_next(ui_event *e){(void)e;choose(1);}
static void refreshed(ui_event *e){(void)e;if(!title)return;notice("");update();}
static void load_core(ui_timer *t) {
    ui_timer_delete(t);load_timer=0;
    if(provider()){notice("Модуль уже загружен");update();return;}
    if(exists("/tmp/Tag_Catalog_V1_attempted")||exists("/tmp/apdu_lab_loaded")||exists("/tmp/tag_catalog_loaded")) {
        notice("Нужна перезагрузка браслета");return;
    }
    if(file_write("/tmp/Tag_Catalog_V1_attempted","1\n")) {
        notice("Не удалось сохранить состояние");return;
    }
    int rc=tag_bootstrap_insmod_core();
    if(rc==0)(void)file_write("/tmp/tag_catalog_v1_loaded","catalog_v1\n");
    notice(rc==0?"Загрузка…":"Ошибка. Перезагрузите браслет");
    if(title)delayed_refresh();
}
static void load_clicked(ui_event *e) {
    (void)e;if(!title||load_timer)return;
    api=provider();
    if(api){tag_catalog_health h;int ok=api->health(&h)==0&&h.loop_live&&!h.error;
        notice(ok?"Модуль работает":"Нужна перезагрузка браслета");update();return;}
    notice("Загрузка…");load_timer=ui_timer_create(load_core,50,0);
}
static int create(void *pm,ui_obj *parent,void *data) {
    (void)pm;(void)data;if(!parent||title)return -1;
    ui_obj *root=ui_object(parent,0);if(!root)return -1;
    /* A plain LVGL object inherits a border/radius from the system theme.
     * Reset only our own background before restoring its explicit geometry. */
    ui_remove_style_all(root);
    ui_size(root,336,480);ui_align(root,1,0,0);pads(root,0);ui_clear_flag(root,0x10u);
    ui_bg(root,0xff000000u,0);ui_opa(root,255,0);
    /* Keep the original card width; halve its height. All main controls
     * share its horizontal bounds, without a page title or module caption. */
    ui_obj *card=button(root,40,32,256,128,0);if(!card)goto failed;
    ui_bg(card,0xff1c2531u,0);
    title=text(card,"Нет меток",12,20,232,48,20);
    ui_obj *type=button(card,12,88,112,28,0);if(!type)goto failed;
    ui_bg(type,0xff304661u,0);badge=text(type,"—",4,2,104,24,17);
    previous=button(root,40,204,120,56,go_previous);
    next=button(root,176,204,120,56,go_next);
    if(!previous||!next)goto failed;
    nav_icon(previous,"/data/tag_catalog/left.bin","‹");nav_icon(next,"/data/tag_catalog/right.bin","›");
    ui_obj *refresh=button(root,40,304,256,56,refreshed);if(!refresh)goto failed;
    (void)text(refresh,"Обновить",8,13,240,30,20);
    module_badge=button(root,105,400,66,48,0);if(!module_badge)goto failed;
    module_text=text(module_badge,"FAIL",3,10,60,28,20);
    ui_obj *load=button(root,183,400,48,48,load_clicked);if(!load)goto failed;
    nav_icon(load,"/data/tag_catalog/refresh.bin","↻");
    if(!title||!badge||!module_text)goto failed;
    update();diagnostic("NFC_CATALOG_UI3\nPAGE_OPEN\n0\nEND\n");return 0;
failed:
    /* PM owns all created children, including a partial page. Never retain
     * partial handles or allow its callbacks to access missing siblings. */
    title=badge=module_badge=module_text=previous=next=0;api=0;
    diagnostic("NFC_CATALOG_UI3\nUI_ALLOC\n-1\nEND\n");return -1;
}
static int destroy(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;
    if(refresh_timer){ui_timer_delete(refresh_timer);refresh_timer=0;}
    if(load_timer){ui_timer_delete(load_timer);load_timer=0;}
    title=badge=module_badge=module_text=previous=next=0;api=0;
    diagnostic("NFC_CATALOG_UI3\nREADY\n0\nEND\n");return 0;
}
static const char *get_name(void){return "NFC-метки";}
static void register_app(ui_timer *t) {
    ui_timer_delete(t);
    if(ui_lookup(APP_ID)){diagnostic("NFC_CATALOG_UI3\nID_OCCUPIED\n-1\nEND\n");return;}
    app.name=app_name;app.app_id=APP_ID;app.get_name=get_name;
    page.name="main";page.app_id=APP_ID;page.layer=1;page.on_create=create;page.on_destroy=destroy;pages[0]=&page;
    int rc=ui_install(&app,pages,1);ui_app *installed=ui_lookup(APP_ID);
    if(rc||!installed||!equal(installed->name,app_name)){diagnostic("NFC_CATALOG_UI3\nREGISTER_FAILED\n-1\nEND\n");return;}
    if(file_write("/tmp/nfc_tags_catalog_ui_ready","catalog_ui_v3\n")) {
        diagnostic("NFC_CATALOG_UI3\nMARKER_FAILED\n-1\nEND\n");return;
    }
    diagnostic("NFC_CATALOG_UI3\nREADY\n0\nEND\n");
    char request[32];
    if(file_read("/tmp/nfc_tags_ui_open_request",request,sizeof(request))>0&&equal(request,"catalog_ui_v3\n")) {
        (void)file_write("/tmp/nfc_tags_ui_open_request","");
        (void)tag_bootstrap_open_app(APP_ID);
    }
}
int module_initialize(void) {
    if(initialized)return initialization_result;
    if(!ui_firmware_verified()||!catalog_ui_bindings_verified()||!tag_bootstrap_bindings_verified())return -1;
    initialized=1;
    diagnostic("NFC_CATALOG_UI3\nDEFERRED\n0\nEND\n");
    initialization_result=ui_timer_create(register_app,500,0)?0:-1;
    return initialization_result;
}
#ifndef TAG_UI_HOST_TEST
__attribute__((constructor,used)) static void module_constructor(void){(void)module_initialize();}
#endif

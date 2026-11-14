/* Pult UI4. Manual refresh, bounded service check, no RF start/idle polling. */
#include "bindings.h"
#include "catalog_ui_bindings.h"
#include "catalog_layout.h"
#include "catalog_assets.h"
#include "../catalog/api.h"
#include "../bootstrap/launch.h"
#define APP_ID 0xd4u
#define PROVIDER_ID 0xd6u
#define SERVICE_TICKS 100u /* 20 seconds at 200ms, only after a user tap. */
static const char app_name[]="dev.mbapdu.tags";
static ui_app app;
static ui_page page,*pages[1];
static ui_obj *title,*badge,*module_badge,*module_surface,*previous,*next,*previous_image,*next_image;
static ui_timer *refresh_timer,*load_timer,*service_timer;
static unsigned refresh_count,initialized,service_ticks,service_frame;
static int initialization_result=-1,service_busy,service_state=-1,page_active;
static uint32_t revision;
static int current=-1,total,selection_target=-1;
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
static int file_write(const char *path,const char *value) {
    size_t n=0;while(value[n])n++;
    void *f=ui_open(path,"w");if(!f)return -1;
    size_t wrote=ui_write(value,1,n,f);int closed=ui_close(f);
    return wrote==n&&!closed?0:-1;
}
static void diagnostic(const char *value){(void)file_write("/tmp/nfc_tags_ui_state",value);}
static void notice(const char *value){(void)file_write("/tmp/nfc_tags_ui_notice",value);}
static int exists(const char *path) {
    void *f=ui_open(path,"r");if(!f)return 0;
    (void)ui_close(f);return 1;
}
static const tag_catalog_api *provider(void) {
    /* Never execute a pointer supplied by a writable file. */
    ui_app *p=ui_lookup(PROVIDER_ID);
    if(!p||!equal(p->name,TAG_CATALOG_PROVIDER_NAME)||!p->get_name)return 0;
    const char *blob=p->get_name();if(!blob)return 0;
    static const char expected[32]=TAG_CATALOG_PROVIDER_LABEL;
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
static ui_obj *text(ui_obj *parent,const char *value,int x,int y,int w,int h,unsigned size,int align) {
    ui_obj *o=ui_label(parent);if(!o)return 0;
    /* Existing MiSans-Demibold 26/20 styles; initialization and literal
     * addresses are fingerprinted. Never mutate a shared firmware style. */
    uintptr_t style=size==TITLE_FONT?0x200d69f8u:0x200d69d4u;
    ui_style(o,(void *)style,255,0);pads(o,0);ui_size(o,w,h);ui_align(o,1,x,y);
    ui_text_align(o,(uint8_t)align,0);ui_long_mode(o,1);ui_clear_flag(o,0x12u);ui_text(o,value);return o;
}
static ui_obj *owned(ui_obj *parent,int x,int y,int w,int h,void (*cb)(ui_event *)) {
    ui_obj *o=cb?ui_button(parent,0):ui_object(parent,0);if(!o)return 0;
    /* Own objects only: remove theme borders, radius, padding and layout. */
    ui_remove_style_all(o);pads(o,0);ui_size(o,w,h);ui_align(o,1,x,y);ui_clear_flag(o,0x10u);
    if(cb)ui_event_cb(o,cb,7,0);else ui_clear_flag(o,2u);
    return o;
}
static ui_obj *surface(ui_obj *parent,const char *path,int x,int y,int w,int h) {
    ui_obj *o=ui_image(parent,0);if(!o)return 0;
    ui_remove_style_all(o);ui_image_src(o,path);ui_size(o,w,h);ui_align(o,1,x,y);ui_clear_flag(o,0x12u);return o;
}
static int contains(const char *value,const char *part) {
    for(;*value;value++){unsigned i=0;while(part[i]&&part[i]==value[i])i++;if(!part[i])return 1;}
    return 0;
}
static void service_draw(int state) {
    if(!module_surface)return;
    if(state==2)ui_image_src(module_surface,ui4_assets[4+(service_frame%4)]);
    else if(service_state!=state)ui_image_src(module_surface,ui4_assets[state==0?2:3]);
    service_state=state;
}
static void nav_update(void) {
    if(!previous_image||!next_image)return;
    ui_image_src(previous_image,ui4_assets[current>0?8:10]);
    ui_image_src(next_image,ui4_assets[total>0&&current+1<total?9:11]);
}
static void update(void) {
    if(!title||!page_active)return;
    tag_catalog_health health;
    api=provider();int available=api&&api->health(&health)==0;
    if(!service_busy)service_draw(available&&health.loop_live&&!health.error?0:1);
    if(!available){current=-1;total=0;revision=0;ui_text(title,"Нет модуля");ui_text(badge,"—");nav_update();return;}
    revision=health.catalog_revision;total=health.count;current=health.selected_index;
    if(current<0||current>=total){ui_text(title,total?"Выберите метку":"Нет меток");ui_text(badge,"—");nav_update();return;}
    tag_catalog_entry entry;
    if(api->get(revision,(uint16_t)current,&entry)!=0){ui_text(title,"Обновите данные");ui_text(badge,"—");nav_update();return;}
    ui_text(title,entry.name);
    const char *kind=entry.kind==1?"NDEF":contains(entry.technology,"IsoDep")?"ISO-DEP":
        contains(entry.technology,"NfcA")?"NFC-A":contains(entry.technology,"NfcB")?"NFC-B":"Сведения";
    ui_text(badge,kind);nav_update();
}
static void refresh_later(ui_timer *t) {
    if(!title||!page_active||!refresh_count){ui_timer_delete(t);refresh_timer=0;return;}
    update();
    if(current==selection_target||!--refresh_count){ui_timer_delete(t);refresh_timer=0;selection_target=-1;}
}
static void delayed_refresh(int target) {
    refresh_count=40;selection_target=target;
    if(!refresh_timer)refresh_timer=ui_timer_create(refresh_later,500,0);
}
static void choose(int delta) {
    if(!title||!page_active||service_busy)return;
    update();int index=current<0?0:current+delta;
    if(!api||index<0||index>=total)return;
    tag_catalog_entry entry;if(api->get(revision,(uint16_t)index,&entry))return;
    int rc=api->select(revision,entry.id,entry.revision);
    notice(rc==0?"":rc==-2?"Дождитесь остановки NFC":"Обновите каталог на Pixel");
    if(!rc)delayed_refresh(index);
}
static void go_previous(ui_event *e){(void)e;choose(-1);}
static void go_next(ui_event *e){(void)e;choose(1);}
static void refreshed(ui_event *e){(void)e;if(!title||!page_active)return;notice("");update();}
static void service_finish(int ok,const char *message) {
    if(service_timer){ui_timer_delete(service_timer);service_timer=0;}
    service_busy=0;service_ticks=0;notice(message);update();service_draw(ok?0:1);
}
static void service_poll(ui_timer *t) {
    (void)t;
    if(!title||!page_active){service_finish(0,"");return;}
    tag_catalog_health health;const tag_catalog_api *a=provider();
    if(a&&a->health(&health)==0){
        if(health.error){service_finish(0,"Ошибка модуля. Перезагрузите браслет");return;}
        if(health.loop_live){service_finish(1,"");return;}
    }
    if(!service_ticks||!--service_ticks){service_finish(0,"Модуль не ответил за 20 секунд");return;}
    service_frame=(service_frame+1)%4;service_draw(2);
}
static void load_core(ui_timer *t) {
    ui_timer_delete(t);load_timer=0;
    if(!title||!page_active){service_finish(0,"");return;}
    if(!provider()) {
        if(exists("/tmp/Tag_Catalog_V1_attempted")||exists("/tmp/apdu_lab_loaded")||exists("/tmp/tag_catalog_loaded")) {
            service_finish(0,"Нужна перезагрузка браслета");return;
        }
        if(file_write("/tmp/Tag_Catalog_V1_attempted","1\n")) {
            service_finish(0,"Не удалось сохранить состояние");return;
        }
        int rc=tag_bootstrap_insmod_core();
        if(rc){service_finish(0,"Ошибка. Перезагрузите браслет");return;}
        (void)file_write("/tmp/tag_catalog_v1_loaded","catalog_v1\n");
    }
    /* Even successful insmod remains grey until registry health is live. */
    service_ticks=SERVICE_TICKS;
    service_timer=ui_timer_create(service_poll,200,0);
    if(!service_timer)service_finish(0,"Не удалось запустить проверку");
}
static void load_clicked(ui_event *e) {
    (void)e;if(!title||!page_active||service_busy)return;
    service_busy=1;service_frame=0;service_draw(2);
    load_timer=ui_timer_create(load_core,50,0);
    if(!load_timer)service_finish(0,"Не удалось запустить проверку");
}
static void handles_clear(void) {
    title=badge=module_badge=module_surface=previous=next=previous_image=next_image=0;api=0;
    page_active=0;service_state=-1;service_busy=0;selection_target=-1;
}
static void timers_stop(void) {
    if(refresh_timer){ui_timer_delete(refresh_timer);refresh_timer=0;}
    if(load_timer){ui_timer_delete(load_timer);load_timer=0;}
    if(service_timer){ui_timer_delete(service_timer);service_timer=0;}
    refresh_count=service_ticks=0;service_busy=0;selection_target=-1;
}
static int pause_page(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;page_active=0;timers_stop();return 0;
}
static int resume_page(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;if(title){page_active=1;update();}return 0;
}
static int create(void *pm,ui_obj *parent,void *data) {
    (void)pm;(void)data;if(!parent||title)return -1;
    for(unsigned i=0;i<sizeof(ui4_assets)/sizeof(ui4_assets[0]);i++)if(!exists(ui4_assets[i])) {
        diagnostic("NFC_CATALOG_UI4\nUI_ASSET\n-1\nEND\n");return -1;
    }
    ui_obj *root=owned(parent,0,0,SCREEN_W,SCREEN_H,0);if(!root)return -1;
    ui_bg(root,0xff000000u,0);ui_opa(root,255,0);
    module_badge=owned(root,STATUS_X,STATUS_Y,STATUS_W,STATUS_H,load_clicked);if(!module_badge)goto failed;
    module_surface=surface(module_badge,ui4_assets[3],0,0,STATUS_W,STATUS_H);if(!module_surface)goto failed;
    ui_obj *card=owned(root,CARD_X,CARD_Y,CARD_W,CARD_H,0);if(!card)goto failed;
    if(!surface(card,ui4_assets[0],0,0,CARD_W,CARD_H))goto failed;
    title=text(card,"Нет меток",TITLE_X,TITLE_Y,TITLE_W,TITLE_H,TITLE_FONT,1);
    badge=text(card,"—",TYPE_X,TYPE_Y+1,TYPE_W,TYPE_H-1,TYPE_FONT,2);
    ui_obj *controls=owned(root,CONTROLS_X,CONTROLS_Y,CONTROLS_W,CONTROLS_H,0);if(!controls)goto failed;
    if(!surface(controls,ui4_assets[1],0,0,CONTROLS_W,CONTROLS_H))goto failed;
    previous=owned(controls,0,NAV_Y,NAV_W,NAV_H,go_previous);
    next=owned(controls,NEXT_X,NAV_Y,NAV_W,NAV_H,go_next);
    if(!previous||!next)goto failed;
    previous_image=surface(previous,ui4_assets[8],65,20,32,32);
    next_image=surface(next,ui4_assets[9],65,20,32,32);
    if(!owned(controls,0,REFRESH_Y,CONTROLS_W,REFRESH_H,refreshed))goto failed;
    if(!title||!badge||!previous_image||!next_image)goto failed;
    page_active=1;service_state=-1;update();diagnostic("NFC_CATALOG_UI4\nPAGE_OPEN\n0\nEND\n");return 0;
failed:
    handles_clear();diagnostic("NFC_CATALOG_UI4\nUI_ALLOC\n-1\nEND\n");return -1;
}
static int destroy(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;timers_stop();handles_clear();
    diagnostic("NFC_CATALOG_UI4\nREADY\n0\nEND\n");return 0;
}
static const char *get_name(void){return "NFC-метки";}
static void register_app(ui_timer *t) {
    ui_timer_delete(t);
    if(ui_lookup(APP_ID)){diagnostic("NFC_CATALOG_UI4\nID_OCCUPIED\n-1\nEND\n");return;}
    app.name=app_name;app.app_id=APP_ID;app.get_name=get_name;
    page.name="main";page.app_id=APP_ID;page.layer=1;page.on_create=create;page.on_destroy=destroy;page.on_pause=pause_page;page.on_stop=pause_page;page.on_resume=resume_page;pages[0]=&page;
    int rc=ui_install(&app,pages,1);ui_app *installed=ui_lookup(APP_ID);
    if(rc||!installed||!equal(installed->name,app_name)){diagnostic("NFC_CATALOG_UI4\nREGISTER_FAILED\n-1\nEND\n");return;}
    if(file_write("/tmp/nfc_tags_catalog_ui_ready","catalog_ui_v4\n")) {
        diagnostic("NFC_CATALOG_UI4\nMARKER_FAILED\n-1\nEND\n");return;
    }
    diagnostic("NFC_CATALOG_UI4\nREADY\n0\nEND\n");
    char request[32];
    if(file_read("/tmp/nfc_tags_ui_open_request",request,sizeof(request))>0&&equal(request,"catalog_ui_v4\n")) {
        (void)file_write("/tmp/nfc_tags_ui_open_request","");
        (void)tag_bootstrap_open_app(APP_ID);
    }
}
int module_initialize(void) {
    if(initialized)return initialization_result;
    if(!ui_firmware_verified()||!catalog_ui_bindings_verified()||!tag_bootstrap_bindings_verified())return -1;
    initialized=1;
    diagnostic("NFC_CATALOG_UI4\nDEFERRED\n0\nEND\n");
    initialization_result=ui_timer_create(register_app,500,0)?0:-1;
    return initialization_result;
}
#ifndef TAG_UI_HOST_TEST
__attribute__((constructor,used)) static void module_constructor(void){(void)module_initialize();}
#endif

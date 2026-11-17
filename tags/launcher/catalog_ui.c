/* Pult UI4/UI5. Manual refresh, bounded service check, no RF start/idle polling.
 * TAG_UI_LOCAL_NFC builds the UI5 page that starts and stops the band-local
 * NDEF responder. The default build keeps the UI4 diagnostics and behavior. */
#ifdef TAG_UI_LOCAL_NFC
#define UI_DIAG "NFC_CATALOG_UI5"
#define UI_READY "catalog_ui_v5\n"
#else
#define UI_DIAG "NFC_CATALOG_UI4"
#define UI_READY "catalog_ui_v4\n"
#endif
#include "bindings.h"
#include "catalog_ui_bindings.h"
#include "catalog_layout.h"
#include "catalog_assets.h"
#include "../catalog/api.h"
#include "../bootstrap/launch.h"
#define APP_ID 0xd4u
#define PROVIDER_ID 0xd6u
#define MIHOME_ID 0xd5u
#define SERVICE_ID 0xd7u
#define SERVICE_TICKS 100u /* 20 seconds at 200ms, only after a user tap. */
static const char app_name[]="dev.mbapdu.tags";
static const char mihome_name[]="dev.mbapdu.mihome";
static const char service_name[]="dev.mbapdu.service";
static ui_app app;
static ui_page page,*pages[1];
static ui_app mihome_app;
static ui_page mihome_page,*mihome_pages[1];
static ui_app service_app;
static ui_page service_page,*service_pages[1];
static ui_obj *mihome_status,*modules_status;
static ui_obj *title,*title_heavy,*badge,*module_badge,*module_surface,*previous,*next,*previous_image,*next_image;
static ui_timer *refresh_timer,*load_timer,*service_timer;
static unsigned refresh_count,initialized,service_ticks,service_frame;
static int initialization_result=-1,service_busy,service_state=-1,page_active;
static uint32_t revision;
static int current=-1,total,selection_target=-1;
static const tag_catalog_api *api;
#ifdef TAG_UI_LOCAL_NFC
static ui_timer *nfc_timer;
static unsigned nfc_ticks;
static int local_requested;
#endif

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
       !a->health||!a->get||!a->select||!a->start||!a->stop)return 0;
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
/* A second resident-font pass adds weight without changing font metrics,
 * creating a font, or mutating the firmware's shared style. */
static void title_text(const char *value) {
    ui_text(title,value);ui_text(title_heavy,value);
}
static void update(void) {
    if(!title||!page_active)return;
    tag_catalog_health health;
    api=provider();
    int available=api&&api->health(&health)==0;
#ifdef TAG_UI_LOCAL_NFC
    int nfc=available&&(health.nfc_ready||health.phase==TAG_CATALOG_STARTING);
#endif
    if(!service_busy)service_draw(available&&health.loop_live&&!health.error?0:1);
    if(!available){current=-1;total=0;revision=0;title_text("Нет модуля");ui_text(badge,"—");nav_update();return;}
    revision=health.catalog_revision;total=health.count;current=health.selected_index;
    if(current<0||current>=total){title_text(total?"Выберите метку":"Нет меток");ui_text(badge,"—");nav_update();return;}
    tag_catalog_entry entry;
    if(api->get(revision,(uint16_t)current,&entry)!=0){title_text("Обновите данные");ui_text(badge,"—");nav_update();return;}
    title_text(entry.name);
    const char *kind=entry.kind==1?"NDEF":contains(entry.technology,"IsoDep")?"ISO-DEP":
        contains(entry.technology,"NfcA")?"NFC-A":contains(entry.technology,"NfcB")?"NFC-B":"Сведения";
#ifdef TAG_UI_LOCAL_NFC
    ui_text(badge,nfc?(health.nfc_ready?"NFC":"NFC…"):health.error?"Ошибка":kind);
#else
    ui_text(badge,kind);
#endif
    nav_update();
}
#ifdef TAG_UI_LOCAL_NFC
static void nfc_poll(ui_timer *t) {
    if(!title||!page_active){ui_timer_delete(t);nfc_timer=0;return;}
    update();
    tag_catalog_health health;
    if(api&&api->health(&health)==0&&(health.nfc_ready||health.phase==TAG_CATALOG_STARTING)){
        nfc_ticks=120;return;
    }
    if(!nfc_ticks){ui_timer_delete(t);nfc_timer=0;return;}
    nfc_ticks--;
}
static void local_start(void) {
    if(!title||!page_active)return;
    if(!api){api=provider();if(!api)return;}
    tag_catalog_health health;
    if(api->health(&health)!=0||health.selected_index<0||health.count<=0)return;
    if(api->start()!=0)return;
    local_requested=1;
    nfc_ticks=120;
    if(!nfc_timer)nfc_timer=ui_timer_create(nfc_poll,500,0);
}
static void local_stop(void) {
    if(nfc_timer){ui_timer_delete(nfc_timer);nfc_timer=0;}
    nfc_ticks=0;
    if(local_requested&&api&&api->stop)(void)api->stop();
    local_requested=0;
}
#endif
static void refresh_later(ui_timer *t) {
    if(!title||!page_active||!refresh_count){ui_timer_delete(t);refresh_timer=0;return;}
    update();
    if(current==selection_target||!--refresh_count){
#ifdef TAG_UI_LOCAL_NFC
        if(current==selection_target)local_start();
#endif
        ui_timer_delete(t);refresh_timer=0;selection_target=-1;
    }
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
static void refreshed(ui_event *e){(void)e;if(!title||!page_active)return;notice("");update();
#ifdef TAG_UI_LOCAL_NFC
    local_start();
#endif
}
static void service_finish(int ok,const char *message) {
    if(service_timer){ui_timer_delete(service_timer);service_timer=0;}
    service_busy=0;service_ticks=0;notice(message);update();service_draw(ok?0:1);
#ifdef TAG_UI_LOCAL_NFC
    if(ok)local_start();
#endif
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
    title=title_heavy=badge=module_badge=module_surface=previous=next=previous_image=next_image=0;api=0;
    page_active=0;service_state=-1;service_busy=0;selection_target=-1;
}
static void timers_stop(void) {
    if(refresh_timer){ui_timer_delete(refresh_timer);refresh_timer=0;}
    if(load_timer){ui_timer_delete(load_timer);load_timer=0;}
    if(service_timer){ui_timer_delete(service_timer);service_timer=0;}
#ifdef TAG_UI_LOCAL_NFC
    if(nfc_timer){ui_timer_delete(nfc_timer);nfc_timer=0;}nfc_ticks=0;local_requested=0;
#endif
    refresh_count=service_ticks=0;service_busy=0;selection_target=-1;
}
static int pause_page(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;
#ifdef TAG_UI_LOCAL_NFC
    local_stop();
#endif
    page_active=0;timers_stop();return 0;
}
static int resume_page(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;if(title){page_active=1;update();
#ifdef TAG_UI_LOCAL_NFC
        local_start();
#endif
    }return 0;
}
static int create(void *pm,ui_obj *parent,void *data) {
    (void)pm;(void)data;if(!parent||title)return -1;
    for(unsigned i=0;i<sizeof(ui4_assets)/sizeof(ui4_assets[0]);i++)if(!exists(ui4_assets[i])) {
        diagnostic(UI_DIAG "\nUI_ASSET\n-1\nEND\n");return -1;
    }
    ui_obj *root=owned(parent,0,0,SCREEN_W,SCREEN_H,0);if(!root)return -1;
    ui_bg(root,0xff000000u,0);ui_opa(root,255,0);
    module_badge=owned(root,STATUS_X,STATUS_Y,STATUS_W,STATUS_H,load_clicked);if(!module_badge)goto failed;
    module_surface=surface(module_badge,ui4_assets[3],0,0,STATUS_W,STATUS_H);if(!module_surface)goto failed;
    ui_obj *card=owned(root,CARD_X,CARD_Y,CARD_W,CARD_H,0);if(!card)goto failed;
    if(!surface(card,ui4_assets[0],0,0,CARD_W,CARD_H))goto failed;
    title=text(card,"Нет меток",TITLE_X,TITLE_Y,TITLE_W,TITLE_H,TITLE_FONT,1);
    title_heavy=text(card,"Нет меток",TITLE_X+TITLE_WEIGHT_OFFSET,TITLE_Y,TITLE_W,TITLE_H,TITLE_FONT,1);
    badge=text(card,"—",TYPE_X,TYPE_Y+(TYPE_H-TYPE_TEXT_H)/2,TYPE_W,TYPE_TEXT_H,TYPE_FONT,2);
    ui_obj *controls=owned(root,CONTROLS_X,CONTROLS_Y,CONTROLS_W,CONTROLS_H,0);if(!controls)goto failed;
    if(!surface(controls,ui4_assets[1],0,0,CONTROLS_W,CONTROLS_H))goto failed;
    previous=owned(controls,0,NAV_Y,NAV_W,NAV_H,go_previous);
    next=owned(controls,NEXT_X,NAV_Y,NAV_W,NAV_H,go_next);
    if(!previous||!next)goto failed;
    previous_image=surface(previous,ui4_assets[8],65,20,32,32);
    next_image=surface(next,ui4_assets[9],65,20,32,32);
    if(!owned(controls,0,REFRESH_Y,CONTROLS_W,REFRESH_H,refreshed))goto failed;
    if(!title||!title_heavy||!badge||!previous_image||!next_image)goto failed;
    page_active=1;service_state=-1;update();
#ifdef TAG_UI_LOCAL_NFC
    local_start();
#endif
    diagnostic(UI_DIAG "\nPAGE_OPEN\n0\nEND\n");return 0;
failed:
    handles_clear();diagnostic(UI_DIAG "\nUI_ALLOC\n-1\nEND\n");return -1;
}
static int destroy(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;
#ifdef TAG_UI_LOCAL_NFC
    local_stop();
#endif
    timers_stop();handles_clear();
    diagnostic(UI_DIAG "\nREADY\n0\nEND\n");return 0;
}
static ui_timer *mihome_timer;
static unsigned mihome_ticks;
static const char *const mihome_labels[6]={"Отец ВКЛ","Сынок ВКЛ",
    "Розетка ВКЛ","Розетка ВЫКЛ","Чайник 45 °C","—  ·  пусто"};
static void mihome_text(const char *value) {
    if(mihome_status)ui_text(mihome_status,value);
}
static void mihome_stop(void) {
    if(mihome_timer){ui_timer_delete(mihome_timer);mihome_timer=0;}
    mihome_ticks=0;
}
static void mihome_poll(ui_timer *t) {
    if(!mihome_status){ui_timer_delete(t);mihome_timer=0;return;}
    char buf[16];
    int n=file_read("/tmp/mihome_status",buf,sizeof(buf));
    if(n>0) {
        mihome_text(contains(buf,"OK")?"Pixel: OK":"Pixel: ошибка");
        (void)file_write("/tmp/mihome_status","");
        mihome_stop();return;
    }
    if(++mihome_ticks>=20) {
        mihome_text("Нет ответа Pixel");
        (void)file_write("/tmp/mihome_action","");
        mihome_stop();return;
    }
}
static void mihome_action(int index) {
    char value[4]={(char)('0'+index),'\n',0};
    if(!file_write("/tmp/mihome_action",value)) {
        mihome_text("Отправлено в Pixel");
        mihome_ticks=0;
        if(!mihome_timer)mihome_timer=ui_timer_create(mihome_poll,500,0);
    } else mihome_text("Ошибка записи действия");
}
static void mihome_1(ui_event *e){(void)e;mihome_action(1);}
static void mihome_2(ui_event *e){(void)e;mihome_action(2);}
static void mihome_3(ui_event *e){(void)e;mihome_action(3);}
static void mihome_4(ui_event *e){(void)e;mihome_action(4);}
static void mihome_5(ui_event *e){(void)e;mihome_action(5);}
static ui_obj *mihome_button(ui_obj *parent,int index,int x,int y) {
    void (*cb)(ui_event *)=index==1?mihome_1:index==2?mihome_2:index==3?mihome_3:
        index==4?mihome_4:index==5?mihome_5:0;
    ui_obj *button=owned(parent,x,y,148,84,cb);if(!button)return 0;
    ui_bg(button,cb?0xff16202cu:0xff101010u,0);ui_opa(button,255,0);
    if(!text(button,mihome_labels[index-1],0,0,148,84,TYPE_FONT,1))return 0;
    return button;
}
static const char *mihome_get_name(void){return "Mi Home";}
static int mihome_create(void *pm,ui_obj *parent,void *data) {
    (void)pm;(void)data;if(!parent||mihome_status)return -1;
    (void)file_write("/tmp/mihome_action","");(void)file_write("/tmp/mihome_status","");
    ui_obj *root=owned(parent,0,0,SCREEN_W,SCREEN_H,0);if(!root)return -1;
    ui_bg(root,0xff000000u,0);ui_opa(root,255,0);
    if(!text(root,"Mi Home",0,24,SCREEN_W,40,TITLE_FONT,1))return -1;
    for(int i=1;i<=6;i++) {
        int x=(i%2)?16:172,y=88+(i-1)/2*100;
        if(!mihome_button(root,i,x,y))return -1;
    }
    mihome_status=text(root,"Откройте Mi Home на Pixel",0,404,SCREEN_W,48,TYPE_FONT,1);
    if(!mihome_status)return -1;
    return 0;
}
static int mihome_destroy(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;mihome_stop();mihome_status=0;return 0;
}
static int mihome_pause(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;mihome_stop();return 0;
}
static void modules_text(const char *value) {
    if(modules_status)ui_text(modules_status,value);
}
static void modules_reload(ui_event *e) {
    (void)e;
    if(!modules_status)return;
    if(ui_lookup(PROVIDER_ID)){modules_text("Ядро уже загружено");return;}
    if(exists("/tmp/Tag_Catalog_V1_attempted")||exists("/tmp/apdu_lab_loaded")||
       exists("/tmp/tag_catalog_loaded")){modules_text("Нужна перезагрузка");return;}
    if(file_write("/tmp/Tag_Catalog_V1_attempted","1\n")){modules_text("Нет доступа к /tmp");return;}
    int rc=tag_bootstrap_insmod_core();
    if(rc){modules_text("Ошибка загрузки ядра");return;}
    modules_text(ui_lookup(PROVIDER_ID)?"Ядро загружено":"Команда выполнена");
}
static void modules_reboot(ui_event *e) {
    (void)e;
    if(!modules_status)return;
    if(tag_bootstrap_reboot())modules_text("Перезагрузка недоступна");
}
static ui_obj *modules_button(ui_obj *parent,const char *caption,int y,void (*cb)(ui_event *)) {
    ui_obj *button=owned(parent,0,y,304,64,cb);if(!button)return 0;
    ui_bg(button,0xff16202cu,0);ui_opa(button,255,0);
    if(!text(button,caption,0,0,304,64,TYPE_FONT,1))return 0;
    return button;
}
static const char *service_get_name(void){return "Модули";}
static int modules_create(void *pm,ui_obj *parent,void *data) {
    (void)pm;(void)data;if(!parent||modules_status)return -1;
    ui_obj *root=owned(parent,0,0,SCREEN_W,SCREEN_H,0);if(!root)return -1;
    ui_bg(root,0xff000000u,0);ui_opa(root,255,0);
    if(!text(root,"Модули",0,24,SCREEN_W,40,TITLE_FONT,1))return -1;
    modules_status=text(root,"",0,80,304,148,TYPE_FONT,1);
    if(!modules_status)return -1;
    modules_text(ui_lookup(PROVIDER_ID)?"Ядро: загружено\nСтраница: NFC-метки":
                                       "Ядро: не загружено\nНажмите загрузку");
    if(!modules_button(root,"Загрузить модули",244,modules_reload))return -1;
    if(!modules_button(root,"Перезагрузить",324,modules_reboot))return -1;
    if(!text(root,"Модуль работает до перезагрузки",0,420,304,40,TYPE_FONT,1))return -1;
    return 0;
}
static int modules_destroy(void *pm,ui_obj *root,void *data) {
    (void)pm;(void)root;(void)data;modules_status=0;return 0;
}
static const char *get_name(void){return "NFC-метки";}
static void register_app(ui_timer *t) {
    ui_timer_delete(t);
    if(ui_lookup(APP_ID)){diagnostic(UI_DIAG "\nID_OCCUPIED\n-1\nEND\n");return;}
    app.name=app_name;app.app_id=APP_ID;app.get_name=get_name;
    /* A launcher entry needs a readable image source. This stock LVGL image
     * exists in the 3.201.026 resource set; a custom icon is a separate,
     * unverified FACE/resource change. */
    app.icon="/resource/app/nfccard/pay_green_icon.bin";
    page.name="main";page.app_id=APP_ID;page.layer=1;page.on_create=create;page.on_destroy=destroy;page.on_pause=pause_page;page.on_stop=pause_page;page.on_resume=resume_page;pages[0]=&page;
    int rc=ui_install(&app,pages,1);ui_app *installed=ui_lookup(APP_ID);
    if(rc||!installed||!equal(installed->name,app_name)){diagnostic(UI_DIAG "\nREGISTER_FAILED\n-1\nEND\n");return;}
    /* Add the same app to the general launcher grid. The watchface's native
     * direct-ID action remains independent and keeps working if this fails. */
    (void)ui_launcher_add(APP_ID);
    /* Separate Mi Home entry: a mock Phase 1 page, never required for NFC. */
    if(!ui_lookup(MIHOME_ID)) {
        mihome_app.name=mihome_name;mihome_app.app_id=MIHOME_ID;
        mihome_app.get_name=mihome_get_name;
        mihome_app.icon="/resource/app/settings/icon/my_device.bin";
        mihome_page.name="main";mihome_page.app_id=MIHOME_ID;mihome_page.layer=1;
        mihome_page.on_create=mihome_create;mihome_page.on_destroy=mihome_destroy;
        mihome_page.on_pause=mihome_pause;mihome_page.on_stop=mihome_pause;
        mihome_pages[0]=&mihome_page;
        if(!ui_install(&mihome_app,mihome_pages,1)) (void)ui_launcher_add(MIHOME_ID);
    }
    /* Recovery entry: loads the catalog core if it is missing and exposes the
     * statically verified NSH reboot path. It is not a substitute for the
     * watchface bootstrap when this whole module failed to load. */
    if(!ui_lookup(SERVICE_ID)) {
        service_app.name=service_name;service_app.app_id=SERVICE_ID;
        service_app.get_name=service_get_name;
        service_app.icon="/resource/app/settings/icon/app_warning.bin";
        service_page.name="main";service_page.app_id=SERVICE_ID;service_page.layer=1;
        service_page.on_create=modules_create;service_page.on_destroy=modules_destroy;
        service_pages[0]=&service_page;
        if(!ui_install(&service_app,service_pages,1)) (void)ui_launcher_add(SERVICE_ID);
    }
    if(file_write("/tmp/nfc_tags_catalog_ui_ready",UI_READY)) {
        diagnostic(UI_DIAG "\nMARKER_FAILED\n-1\nEND\n");return;
    }
    diagnostic(UI_DIAG "\nREADY\n0\nEND\n");
    char request[32];
    if(file_read("/tmp/nfc_tags_ui_open_request",request,sizeof(request))>0&&equal(request,UI_READY)) {
        (void)file_write("/tmp/nfc_tags_ui_open_request","");
        (void)tag_bootstrap_open_app(APP_ID);
    }
}
int module_initialize(void) {
    if(initialized)return initialization_result;
    if(!ui_firmware_verified()||!catalog_ui_bindings_verified()||!tag_bootstrap_bindings_verified())return -1;
    initialized=1;
    diagnostic(UI_DIAG "\nDEFERRED\n0\nEND\n");
    initialization_result=ui_timer_create(register_app,500,0)?0:-1;
    return initialization_result;
}
#ifndef TAG_UI_HOST_TEST
__attribute__((constructor,used)) static void module_constructor(void){(void)module_initialize();}
#endif

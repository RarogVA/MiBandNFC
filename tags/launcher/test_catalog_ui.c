/* Host fault injection for the catalog UI. No firmware/device calls. */
#define TAG_UI_HOST_TEST 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "catalog_ui.c"

struct ui_obj {
    struct ui_obj *parent;
    int x,y,width,height,align;
    unsigned font,long_mode,flags,color,style_reset,text_align,bold;
    char image_path[96];
    char text[256];
    void (*click)(ui_event *);
};
struct ui_timer {void (*callback)(ui_timer *);unsigned period,live;};
typedef struct {char path[96],data[256];size_t length;int present,close_error;} mock_file;
static ui_obj nodes[256],parent;
static ui_timer timers[128];
static mock_file files[32];
static unsigned node_count,timer_count,file_count,alloc_attempts,fail_allocation,image_count;
static unsigned invalid_objects,install_count,open_count,insmod_count,select_count,start_count,stop_count,launcher_count,mihome_install_count,service_install_count;
static unsigned health_calls,get_calls,name_calls,api_file_reads;
static int firmware_ok,bindings_ok,launch_ok,timer_fail,install_rc,install_partial,install_wrong;
static int insmod_rc,health_rc,get_rc,select_rc,provider_present,ui_present,mihome_present,service_present,write_short,write_close;
static const char *write_failure_path;
static tag_catalog_provider blob;
static tag_catalog_health health_value;
static tag_catalog_entry entries[3];
static ui_app provider_app,other_app;
static unsigned failures,checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)

static mock_file *file(const char *path,int make) {
    for(unsigned i=0;i<file_count;i++)if(!strcmp(files[i].path,path))return &files[i];
    if(!make)return 0;
    assert(file_count<32&&strlen(path)<sizeof(files[0].path));
    mock_file *f=&files[file_count++];strcpy(f->path,path);return f;
}
static void put(const char *path,const char *data) {
    mock_file *f=file(path,1);f->present=1;f->length=strlen(data);
    assert(f->length<sizeof(f->data));memcpy(f->data,data,f->length+1);
}
static const char *diagnostic_value(void) {mock_file *f=file("/tmp/nfc_tags_ui_state",0);return f?f->data:"";}
static const char *notice_value(void) {mock_file *f=file("/tmp/nfc_tags_ui_notice",0);return f?f->data:"";}
int ui_firmware_verified(void){return firmware_ok;}
int catalog_ui_bindings_verified(void){return bindings_ok;}
int tag_bootstrap_bindings_verified(void){return launch_ok;}
void *ui_open(const char *path,const char *mode) {
    if(!strcmp(path,TAG_CATALOG_API_PATH))api_file_reads++;
    if(!strcmp(mode,"w")) {
        if(write_failure_path&&!strcmp(path,write_failure_path))return 0;
        mock_file *f=file(path,1);f->present=1;f->length=0;f->data[0]=0;f->close_error=write_close;return f;
    }
    assert(!strcmp(mode,"r"));mock_file *f=file(path,0);return f&&f->present?f:0;
}
size_t ui_read(void *out,size_t size,size_t count,void *handle) {
    assert(size==1&&handle);mock_file *f=handle;size_t n=f->length<count?f->length:count;memcpy(out,f->data,n);return n;
}
size_t ui_write(const void *in,size_t size,size_t count,void *handle) {
    assert(size==1&&handle&&count<256);mock_file *f=handle;
    size_t n=write_short&&count?count-1:count;memcpy(f->data,in,n);f->data[n]=0;f->length=n;return n;
}
int ui_close(void *handle){assert(handle);return ((mock_file *)handle)->close_error;}
ui_timer *ui_timer_create(void (*cb)(ui_timer *),uint32_t ms,void *data) {
    assert(cb&&!data&&(ms==50||ms==200||ms==500));if(timer_fail)return 0;
    assert(timer_count<128);ui_timer *t=&timers[timer_count++];t->callback=cb;t->period=ms;t->live=1;return t;
}
void ui_timer_delete(ui_timer *t){assert(t&&t->live);t->live=0;}
static unsigned live_timers(void){unsigned n=0;for(unsigned i=0;i<timer_count;i++)n+=timers[i].live;return n;}
static void tick(unsigned period) {
    unsigned limit=timer_count;
    for(unsigned i=0;i<limit;i++)if(timers[i].live&&timers[i].period==period)timers[i].callback(&timers[i]);
}
ui_app *ui_lookup(uint16_t id) {
    if(id==PROVIDER_ID)return provider_present?&provider_app:0;
    if(id==MIHOME_ID)return mihome_present?&mihome_app:0;
    if(id==SERVICE_ID)return service_present?&service_app:0;
    assert(id==APP_ID);return ui_present?(install_wrong?&other_app:&app):0;
}
int ui_install(ui_app *a,ui_page **p,int n) {
    if(a==&mihome_app){assert(p[0]==&mihome_page&&n==1);mihome_present=1;mihome_install_count++;return 0;}
    if(a==&service_app){assert(p[0]==&service_page&&n==1);service_present=1;service_install_count++;return 0;}
    assert(a==&app&&p[0]==&page&&n==1);install_count++;
    ui_present=!install_rc||install_partial;return install_rc;
}
int ui_launcher_add(uint16_t id) {
    assert(id==APP_ID||id==MIHOME_ID||id==SERVICE_ID);launcher_count++;return 0;
}
int tag_bootstrap_open_app(uint16_t id){assert(id==APP_ID);open_count++;return 0;}
int tag_bootstrap_insmod_core(void) {
    /* This must exist and have completed its write/close before any command. */
    mock_file *f=file("/tmp/Tag_Catalog_V1_attempted",0);
    CHECK(f&&f->present&&!f->close_error&&f->length==2&&!memcmp(f->data,"1\n",2));
    insmod_count++;return insmod_rc;
}
int tag_bootstrap_reboot(void){return 0;}
static ui_obj *object(ui_obj *p) {
    assert(p);if(++alloc_attempts==fail_allocation)return 0;
    assert(node_count<256);ui_obj *o=&nodes[node_count++];o->parent=p;o->flags=0xffff;return o;
}
ui_obj *ui_object(ui_obj *p,void *data){assert(!data);return object(p);}
ui_obj *ui_label(ui_obj *p){return object(p);}
ui_obj *ui_button(ui_obj *p,void *data){assert(!data);return object(p);}
ui_obj *ui_image(ui_obj *p,void *data){assert(!data);ui_obj *o=object(p);if(o)image_count++;return o;}
static int valid(ui_obj *o){if(!o){invalid_objects++;return 0;}return 1;}
void ui_remove_style_all(ui_obj *o) {
    assert(o&&o!=&parent);
    ui_obj *ancestor=o->parent;while(ancestor&&ancestor!=&parent)ancestor=ancestor->parent;
    assert(ancestor==&parent);
    /* Reset must precede explicit geometry/background, and never touch PM. */
    CHECK(!o->width&&!o->height&&!o->align&&!o->font&&!o->color&&!o->style_reset);
    o->style_reset=1;
}
void ui_size(ui_obj *o,int32_t w,int32_t h){if(valid(o)){assert(w>0&&h>0);o->width=w;o->height=h;}}
void ui_align(ui_obj *o,int a,int32_t x,int32_t y){if(valid(o)){assert(a==1||a==9);o->align=a;o->x=x;o->y=y;}}
void ui_clear_flag(ui_obj *o,uint32_t f){if(valid(o))o->flags&=~f;}
void ui_event_cb(ui_obj *o,void (*cb)(ui_event *),uint8_t f,void *data){if(valid(o)){assert(cb&&f==7&&!data);o->click=cb;}}
void ui_text(ui_obj *o,const char *value){if(valid(o)){assert(value&&strlen(value)<sizeof(o->text));strcpy(o->text,value);}}
void ui_style(ui_obj *o,void *style,uint8_t opacity,uint32_t sel){
    if(valid(o)){assert(opacity==255&&!sel);uintptr_t s=(uintptr_t)style;
        o->font=s==0x200d69f8?26:s==0x200d69d4?20:0;o->bold=1;assert(o->font);}
}
void ui_bg(ui_obj *o,uint32_t color,uint32_t sel){assert(!sel);if(valid(o))o->color=color;}
void ui_opa(ui_obj *o,uint8_t opacity,uint32_t sel){assert(opacity==255&&!sel);(void)valid(o);}
#define PAD(name) void name(ui_obj *o,int32_t n,uint32_t sel){assert(!n&&!sel);(void)valid(o);}
PAD(ui_pad_left) PAD(ui_pad_right) PAD(ui_pad_top) PAD(ui_pad_bottom)
void ui_text_align(ui_obj *o,uint8_t a,uint32_t s){assert((a==1||a==2)&&!s);if(valid(o))o->text_align=a;}
void ui_long_mode(ui_obj *o,uint8_t m){assert(m==1);if(valid(o))o->long_mode=m;}
void ui_image_src(ui_obj *o,const void *p){assert(p&&strlen(p)<96);if(valid(o))strcpy(o->image_path,p);}
static int health_mock(tag_catalog_health *out){health_calls++;*out=health_value;return health_rc;}
static int get_mock(uint32_t rev,uint16_t index,tag_catalog_entry *out) {
    get_calls++;if(get_rc||rev!=health_value.catalog_revision||index>=health_value.count)return -1;
    assert(index<3);*out=entries[index];return 0;
}
static int select_mock(uint32_t rev,const uint8_t id[16],uint32_t entry_rev) {
    CHECK(rev==health_value.catalog_revision);int found=-1;
    for(unsigned i=0;i<3;i++)if(!memcmp(id,entries[i].id,16))found=(int)i;
    CHECK(found>=0&&entry_rev==entries[found].revision);select_count++;return select_rc;
}
static int start_mock(void){start_count++;return -1;}
static int stop_mock(void){stop_count++;return 0;}
static const char *provider_name(void){name_calls++;return (const char *)&blob;}
static void reset(void) {
    memset(nodes,0,sizeof(nodes));memset(timers,0,sizeof(timers));memset(files,0,sizeof(files));
    memset(&app,0,sizeof(app));memset(&page,0,sizeof(page));pages[0]=0;
    memset(&mihome_app,0,sizeof(mihome_app));memset(&mihome_page,0,sizeof(mihome_page));
    memset(&service_app,0,sizeof(service_app));memset(&service_page,0,sizeof(service_page));
    mihome_pages[0]=service_pages[0]=0;mihome_status=modules_status=0;
    handles_clear();refresh_timer=load_timer=service_timer=0;service_ticks=service_frame=0;
    refresh_count=initialized=revision=0;initialization_result=-1;current=-1;total=0;api=0;
    node_count=timer_count=file_count=alloc_attempts=fail_allocation=image_count=0;
    invalid_objects=install_count=open_count=insmod_count=select_count=start_count=stop_count=launcher_count=mihome_install_count=0;
    health_calls=get_calls=name_calls=api_file_reads=0;
    firmware_ok=bindings_ok=launch_ok=1;
    timer_fail=install_rc=install_partial=install_wrong=insmod_rc=health_rc=get_rc=select_rc=0;mihome_present=0;service_present=0;
    provider_present=ui_present=write_short=write_close=0;write_failure_path=0;
    memset(&blob,0,sizeof(blob));strcpy(blob.label,TAG_CATALOG_PROVIDER_LABEL);
    blob.api=(tag_catalog_api){TAG_CATALOG_API_MAGIC,TAG_CATALOG_API_VERSION,sizeof(tag_catalog_api),
        TAG_CATALOG_API_PROFILE,health_mock,get_mock,select_mock,start_mock,stop_mock};
    memset(&provider_app,0,sizeof(provider_app));provider_app.name=TAG_CATALOG_PROVIDER_NAME;provider_app.get_name=provider_name;
    memset(&other_app,0,sizeof(other_app));other_app.name="unrelated";
    memset(&health_value,0,sizeof(health_value));health_value.loop_live=1;health_value.catalog_revision=7;
    health_value.count=3;health_value.selected_index=1;
    memset(entries,0,sizeof(entries));
    for(unsigned i=0;i<3;i++){entries[i].id[15]=(uint8_t)(i+1);entries[i].revision=i+9;entries[i].kind=1;
        strcpy(entries[i].name,i==1?"NFC-Test":"Длинное русское название NFC-метки");strcpy(entries[i].technology,"NDEF");strcpy(entries[i].uid,"AABBCCDD");}
    memset(&parent,0,sizeof(parent));parent.width=336;parent.height=480;
    for(unsigned i=0;i<sizeof(ui4_assets)/sizeof(ui4_assets[0]);i++)put(ui4_assets[i],"artwork");
}
static void open_page(void){CHECK(create(0,&parent,0)==0);CHECK(!invalid_objects);}
static void trust_test(void) {
    reset();CHECK(!provider());provider_present=1;CHECK(provider()==&blob.api);
    provider_app.name="dev.mbapdu.catalog.api.v2";CHECK(!provider()&&name_calls==1);
    provider_app.name=TAG_CATALOG_PROVIDER_NAME;provider_app.get_name=0;CHECK(!provider());provider_app.get_name=provider_name;
    for(unsigned i=0;i<32;i++){char before=blob.label[i];blob.label[i]^=1;CHECK(!provider());blob.label[i]=before;}
    blob.api.magic^=1;CHECK(!provider());blob.api.magic^=1;
    blob.api.version++;CHECK(!provider());blob.api.version--;
    blob.api.bytes--;CHECK(!provider());blob.api.bytes++;
    blob.api.profile++;CHECK(!provider());blob.api.profile--;
    blob.api.health=0;CHECK(!provider());blob.api.health=health_mock;
    blob.api.get=0;CHECK(!provider());blob.api.get=get_mock;
    blob.api.select=0;CHECK(!provider());blob.api.select=select_mock;
    blob.api.stop=0;CHECK(!provider());blob.api.stop=stop_mock;
    put(TAG_CATALOG_API_PATH,"0x41414141\n");CHECK(provider()==&blob.api&&!api_file_reads&&!health_calls);
}
static int overlap(ui_obj *a,ui_obj *b){return a->x<b->x+b->width&&b->x<a->x+a->width&&a->y<b->y+b->height&&b->y<a->y+a->height;}
static ui_obj *control(void (*callback)(ui_event *)) {
    for(unsigned i=0;i<node_count;i++)if(nodes[i].click==callback)return &nodes[i];
    return 0;
}
static void box(ui_obj *o,int x,int y,int width,int height) {
    CHECK(o&&o->align==1&&o->x==x&&o->y==y&&o->width==width&&o->height==height);
}
static void layout_test(void) {
    reset();provider_present=1;open_page();ui_obj *root=&nodes[0];
    CHECK(root->width==336&&root->height==480&&!(root->flags&0x10));
    CHECK(root->style_reset==1&&root->color==0xff000000u&&!parent.style_reset);
    box(title->parent,5,87,326,216);
    CHECK(title->parent->style_reset&&!(title->parent->flags&0x12u));
    box(previous->parent,5,336,326,139);
    box(previous,0,0,163,72);box(next,163,0,163,72);
    box(control(refreshed),0,72,326,67);
    box(module_badge,5,5,326,49);CHECK(control(load_clicked)==module_badge);
    CHECK(title->font==26&&title->width==222&&title->height==60&&title->long_mode==1&&title->text_align==1);
    box(title_heavy,TITLE_X+1,TITLE_Y,TITLE_W,TITLE_H);
    CHECK(title_heavy->font==title->font&&title_heavy->long_mode==title->long_mode&&title_heavy->text_align==title->text_align);
    CHECK(badge->font==20&&badge->text_align==2&&badge->bold&&title->bold);
    CHECK(badge->x==20&&badge->y==24&&badge->height==28&&title->y==98&&badge->y+badge->height<title->y);
    CHECK(title->parent->y+title->parent->height+33==previous->parent->y);
    CHECK(module_badge->y+module_badge->height+33==title->parent->y);
    for(unsigned i=1;i<node_count;i++) {
        ui_obj *o=&nodes[i];int x=o->align==9?(o->parent->width-o->width)/2+o->x:o->x;
        int y=o->align==9?(o->parent->height-o->height)/2+o->y:o->y;
        CHECK(x>=0&&y>=0&&x+o->width<=o->parent->width&&y+o->height<=o->parent->height);
        CHECK(!strstr(o->text,"AABBCCDD"));if(o->font)CHECK(o->height>=(int)o->font);
        CHECK(strcmp(o->text,"NFC-метки")&&strcmp(o->text,"Модуль")&&strcmp(o->text,"Загрузить"));
        if(o->click)CHECK(o->style_reset&&o->width>=44&&o->height>=44&&!(o->flags&0x10u));
        if(o->image_path[0])CHECK(!(o->flags&0x12u));
        if(o->parent==root)for(unsigned j=1;j<i;j++)if(nodes[j].parent==root)CHECK(!overlap(o,&nodes[j]));
    }
    CHECK(!strcmp(title->text,"NFC-Test")&&!strcmp(badge->text,"NDEF")&&service_state==0);
    CHECK(strstr(module_surface->image_path,"status-ok-v4.bin"));
    CHECK(previous->parent->y+previous->parent->height==475);
    unsigned children=0;for(unsigned i=1;i<node_count;i++)children+=nodes[i].parent==root;CHECK(children==3);
    CHECK(!start_count&&!stop_count&&!insmod_count&&!live_timers());
    CHECK(image_count==5);destroy(0,&parent,0);
}
static void registration_test(void) {
    for(unsigned i=0;i<3;i++){reset();if(i==0)firmware_ok=0;if(i==1)bindings_ok=0;if(i==2)launch_ok=0;
        CHECK(module_initialize()==-1&&!live_timers()&&!install_count);}
    reset();timer_fail=1;CHECK(module_initialize()==-1);timer_fail=0;
    CHECK(module_initialize()==-1&&!live_timers()); /* Resident failed initialization stays explicitly failed. */
    reset();CHECK(module_initialize()==0&&module_initialize()==0&&live_timers()==1);
    tick(500);CHECK(ui_present&&install_count==1&&mihome_install_count==1&&launcher_count==3&&service_install_count==1&&!live_timers());
    CHECK(!strcmp(app.icon,"/resource/app/nfccard/pay_green_icon.bin")&&launcher_count==3&&!page.on_back&&!strcmp(diagnostic_value(),"NFC_CATALOG_UI4\nREADY\n0\nEND\n"));
    CHECK(file("/tmp/nfc_tags_catalog_ui_ready",0)&&!strcmp(file("/tmp/nfc_tags_catalog_ui_ready",0)->data,"catalog_ui_v4\n"));
    reset();ui_present=1;module_initialize();tick(500);CHECK(!install_count&&strstr(diagnostic_value(),"ID_OCCUPIED"));
    reset();install_rc=-12;install_partial=1;module_initialize();tick(500);
    CHECK(ui_present&&strstr(diagnostic_value(),"REGISTER_FAILED")&&!open_count);
    reset();install_wrong=1;module_initialize();tick(500);CHECK(strstr(diagnostic_value(),"REGISTER_FAILED"));
    reset();put("/tmp/nfc_tags_ui_open_request","catalog_ui_v4\n");module_initialize();tick(500);
    CHECK(open_count==1&&file("/tmp/nfc_tags_ui_open_request",0)->length==0);
    static const char *invalid_requests[]={"catalog_ui_v1\n","catalog_ui_v2\n","catalog_ui_v3\n","catalog_ui_v4","catalog_ui_v4\nextra","garbage",""};
    for(unsigned i=0;i<sizeof(invalid_requests)/sizeof(invalid_requests[0]);i++) {
        reset();put("/tmp/nfc_tags_ui_open_request",invalid_requests[i]);module_initialize();tick(500);CHECK(!open_count);
    }
    reset();put("/tmp/nfc_tags_ui_open_request","catalog_ui_v4\n");file("/tmp/nfc_tags_ui_open_request",0)->close_error=-1;
    module_initialize();tick(500);CHECK(!open_count);
    reset();write_failure_path="/tmp/nfc_tags_catalog_ui_ready";module_initialize();tick(500);
    CHECK(ui_present&&strstr(diagnostic_value(),"MARKER_FAILED")&&!open_count);
}
static void selection_test(void) {
    reset();provider_present=1;open_page();go_next(0);CHECK(select_count==1&&live_timers()==1);
    CHECK(refresh_timer&&refresh_timer->period==500&&refresh_count==40);
    go_previous(0);CHECK(select_count==2&&live_timers()==1);
    for(unsigned i=0;i<40;i++)tick(500);CHECK(!live_timers()&&!start_count&&!stop_count);
    health_value.selected_index=0;update();go_previous(0);CHECK(select_count==2);
    health_value.selected_index=2;update();go_next(0);CHECK(select_count==2);
    health_value.selected_index=1;select_rc=-2;go_previous(0);CHECK(select_count==3&&strstr(notice_value(),"остановки")&&!live_timers());
    select_rc=-1;go_next(0);CHECK(select_count==4&&strstr(notice_value(),"Pixel")&&!live_timers());
    get_rc=-1;go_next(0);CHECK(select_count==4&&!live_timers());
    get_rc=0;health_value.count=0;health_value.selected_index=-1;update();go_next(0);
    CHECK(select_count==4&&!strcmp(title->text,"Нет меток"));
    health_value.count=3;health_value.selected_index=0;entries[0].kind=0;strcpy(entries[0].technology,"NfcA IsoDep");update();
    CHECK(!strcmp(badge->text,"ISO-DEP"));health_value.loop_live=0;update();CHECK(service_state==1);
    health_rc=-1;update();CHECK(!strcmp(title->text,"Нет модуля"));
    CHECK(!start_count&&!stop_count&&!insmod_count);
}
static void title_weight_test(void) {
    reset();provider_present=1;open_page();
    CHECK(!strcmp(title->text,title_heavy->text));
    health_value.selected_index=0;update();CHECK(!strcmp(title->text,entries[0].name)&&!strcmp(title->text,title_heavy->text));
    get_rc=-1;update();CHECK(!strcmp(title->text,"Обновите данные")&&!strcmp(title->text,title_heavy->text));
    get_rc=0;health_value.selected_index=-1;update();CHECK(!strcmp(title->text,"Выберите метку")&&!strcmp(title->text,title_heavy->text));
    health_value.count=0;update();CHECK(!strcmp(title->text,"Нет меток")&&!strcmp(title->text,title_heavy->text));
    health_rc=-1;update();CHECK(!strcmp(title->text,"Нет модуля")&&!strcmp(title->text,title_heavy->text));
    destroy(0,&parent,0);CHECK(!title&&!title_heavy&&!badge);
}
static void lifecycle_test(void) {
    reset();file("/data/tag_catalog/card-v4.bin",0)->present=0;CHECK(create(0,&parent,0)==-1&&!title&&!live_timers());
    reset();provider_present=1;open_page();unsigned allocation_count=alloc_attempts;
    CHECK(create(0,&parent,0)==-1);go_next(0);CHECK(live_timers()==1);
    destroy(0,&parent,0);CHECK(!live_timers()&&!title&&!title_heavy&&!api);
    unsigned before=node_count;go_next(0);refreshed(0);load_clicked(0);
    CHECK(before==node_count&&!invalid_objects&&!live_timers());open_page();destroy(0,&parent,0);
    for(unsigned fail=1;fail<=allocation_count;fail++) {
        reset();fail_allocation=fail;int rc=create(0,&parent,0);
        CHECK(!invalid_objects);
        if(rc){CHECK(!title&&!title_heavy&&!badge&&!module_surface&&!previous&&!next);
            go_next(0);refreshed(0);load_clicked(0);CHECK(!invalid_objects&&!live_timers());}
        destroy(0,&parent,0);CHECK(!live_timers());fail_allocation=0;open_page();destroy(0,&parent,0);
    }
    reset();open_page();load_clicked(0);CHECK(live_timers()==1);destroy(0,&parent,0);tick(50);CHECK(!insmod_count&&!live_timers());
    reset();provider_present=1;open_page();timer_fail=1;go_next(0);CHECK(!live_timers());destroy(0,&parent,0);
}
static void loader_test(void) {
    static const char *guards[]={"/tmp/Tag_Catalog_V1_attempted","/tmp/apdu_lab_loaded","/tmp/tag_catalog_loaded"};
    for(unsigned i=0;i<3;i++)for(unsigned close_error=0;close_error<2;close_error++) {
        reset();put(guards[i],"present");file(guards[i],0)->close_error=close_error?-1:0;open_page();load_clicked(0);tick(50);
        CHECK(!insmod_count&&strstr(notice_value(),"перезагрузка")&&!live_timers()&&service_state==1);destroy(0,&parent,0);
    }
    for(unsigned failure=0;failure<3;failure++) {
        reset();open_page();if(!failure)write_failure_path="/tmp/Tag_Catalog_V1_attempted";
        if(failure==1)write_short=1;if(failure==2)write_close=-1;
        load_clicked(0);tick(50);CHECK(!insmod_count&&strstr(notice_value(),"сохранить")&&service_state==1&&!live_timers());destroy(0,&parent,0);
    }
    reset();open_page();load_clicked(0);load_clicked(0);CHECK(live_timers()==1&&service_state==2&&service_busy);tick(50);
    CHECK(insmod_count==1&&file("/tmp/tag_catalog_v1_loaded",0)->present&&live_timers()==1&&service_state==2);
    tick(200);CHECK(strstr(module_surface->image_path,"busy1"));
    tick(200);CHECK(strstr(module_surface->image_path,"busy2"));
    tick(200);CHECK(strstr(module_surface->image_path,"busy3"));
    tick(200);CHECK(strstr(module_surface->image_path,"busy0"));
    refreshed(0);CHECK(service_state==2);load_clicked(0);CHECK(insmod_count==1&&live_timers()==1);
    for(unsigned i=4;i<100;i++)tick(200);
    CHECK(!live_timers()&&!service_busy&&service_state==1&&strstr(notice_value(),"20 секунд"));
    load_clicked(0);tick(50);CHECK(insmod_count==1&&!live_timers());destroy(0,&parent,0);open_page();load_clicked(0);tick(50);CHECK(insmod_count==1);
    reset();insmod_rc=-1;open_page();load_clicked(0);tick(50);CHECK(insmod_count==1&&!file("/tmp/tag_catalog_v1_loaded",0)&&service_state==1&&!live_timers());
    load_clicked(0);tick(50);CHECK(insmod_count==1);destroy(0,&parent,0);
    reset();open_page();load_clicked(0);provider_present=1;tick(50);
    CHECK(!insmod_count&&live_timers()==1&&service_state==2);tick(200);CHECK(!live_timers()&&service_state==0&&!service_busy);
    reset();open_page();load_clicked(0);put("/tmp/Tag_Catalog_V1_attempted","1\n");tick(50);
    CHECK(!insmod_count&&strstr(notice_value(),"перезагрузка"));
    reset();provider_present=1;open_page();load_clicked(0);CHECK(service_state==2);tick(50);tick(200);CHECK(!insmod_count&&!live_timers()&&service_state==0);
    health_value.error=-1;load_clicked(0);tick(50);tick(200);CHECK(service_state==1&&!live_timers()&&!insmod_count);
    health_value.error=0;health_value.loop_live=0;load_clicked(0);tick(50);for(unsigned i=0;i<100;i++)tick(200);CHECK(service_state==1&&!live_timers()&&!insmod_count);
    reset();open_page();timer_fail=1;load_clicked(0);CHECK(service_state==1&&!live_timers()&&!insmod_count);
    reset();open_page();load_clicked(0);timer_fail=1;tick(50);CHECK(service_state==1&&!live_timers()&&insmod_count==1);
    CHECK(!start_count&&!stop_count);
}
static void pause_test(void) {
    reset();open_page();load_clicked(0);pause_page(0,&parent,0);tick(50);
    CHECK(!live_timers()&&!insmod_count&&!service_busy&&!page_active);
    load_clicked(0);go_next(0);refreshed(0);CHECK(!live_timers());
    provider_present=1;resume_page(0,&parent,0);CHECK(page_active&&service_state==0&&!live_timers());
    go_next(0);CHECK(live_timers()==1);pause_page(0,&parent,0);CHECK(!live_timers());
    resume_page(0,&parent,0);load_clicked(0);tick(50);CHECK(live_timers()==1);
    destroy(0,&parent,0);tick(200);CHECK(!live_timers()&&!title&&!module_surface);
    reset();open_page();load_clicked(0);tick(50);provider_present=1;health_value.loop_live=0;tick(200);
    CHECK(service_state==2);health_value.loop_live=1;tick(200);CHECK(service_state==0&&!live_timers());
    go_next(0);health_value.selected_index=2;tick(500);CHECK(!live_timers()&&!strcmp(title->text,entries[2].name));
    CHECK(!start_count&&!stop_count);
}
static void pages_test(void) {
    reset();
    CHECK(mihome_create(0,&parent,0)==0);
    CHECK(mihome_status&&!strcmp(mihome_status->text,"Откройте Mi Home на Pixel"));
    CHECK(node_count==15);
    mihome_action(3);
    CHECK(!strcmp(file("/tmp/mihome_action",0)->data,"3\n"));
    CHECK(!strcmp(mihome_status->text,"Отправлено в Pixel")&&live_timers()==1);
    put("/tmp/mihome_status","OK\n");
    tick(500);
    CHECK(!strcmp(mihome_status->text,"Pixel: OK")&&!live_timers());
    CHECK(!strcmp(file("/tmp/mihome_status",0)->data,""));
    CHECK(modules_create(0,&parent,0)==0);
    CHECK(modules_status&&strstr(modules_status->text,"Ядро: не загружено"));
    CHECK(node_count==23);
    modules_reload(0);
    CHECK(insmod_count==1&&modules_status&&strstr(modules_status->text,"Команда выполнена"));
    modules_destroy(0,&parent,0);CHECK(!modules_status);
    mihome_destroy(0,&parent,0);CHECK(!mihome_status);
}
int main(void) {
    trust_test();layout_test();registration_test();selection_test();title_weight_test();lifecycle_test();loader_test();pause_test();pages_test();
    if(failures){fprintf(stderr,"catalog UI: %u of %u checks failed\n",failures,checks);return 1;}
    printf("catalog UI trust, geometry, registration, selection, lifecycle and guarded loader: %u checks PASS\n",checks);return 0;
}

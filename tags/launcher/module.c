/* Read-only NFC tag page. The verified bridge owns BLE/NFC; this module never
 * starts/stops it, deletes its timer, or unloads either resident module. */
#include "bindings.h"
#include "state.h"
#define APP_ID 0xd4u
static const char app_name[]="dev.mbapdu.tags";
static ui_app app;
static ui_page page;
static ui_page *pages[1];
static ui_timer *registration_timer;
static ui_obj *content;
static unsigned initialized;
#ifdef TAG_UI_SCROLL_V2
static ui_obj *uid_field,*technology_field,*support_field,*state_field;
#endif

static int equal(const char *a,const char *b) {
    if(!a||!b)return 0;
    while(*a&&*a==*b){a++;b++;}return *a==*b;
}
static void append(char *out,size_t *at,size_t cap,const char *text) {
    while(*text && *at+1<cap)out[(*at)++]=*text++;
    out[*at]=0;
}
static void diagnostic(const char *stage,int rc) {
    char bytes[96]={0},digits[10];size_t at=0,n=0;
#ifdef TAG_UI_SCROLL_V2
    append(bytes,&at,sizeof(bytes),"NFC_TAGS_UI2\n");
#else
    append(bytes,&at,sizeof(bytes),"NFC_TAGS_UI1\n");
#endif
    append(bytes,&at,sizeof(bytes),stage);append(bytes,&at,sizeof(bytes),"\n");
    uint32_t magnitude=(uint32_t)rc;
    if(rc<0){append(bytes,&at,sizeof(bytes),"-");magnitude=0u-magnitude;}
    do{digits[n++]=(char)('0'+magnitude%10u);magnitude/=10u;}while(magnitude);
    while(n && at+1<sizeof(bytes))bytes[at++]=digits[--n];bytes[at]=0;
    append(bytes,&at,sizeof(bytes),"\nEND\n");
    void *file=ui_open("/tmp/nfc_tags_ui_state","w");
    if(file){(void)ui_write(bytes,1,at,file);(void)ui_close(file);}
}
#ifndef TAG_UI_SCROLL_V2
static void refresh(void) {
    if(!content)return;
    char bytes[161];tag_ui_state state;
    void *file=ui_open("/tmp/tag_catalog_state","r");
    if(!file){ui_text(content,"Нет данных метки.\n\nВыберите метку\nв приложении Pixel.");return;}
    size_t n=ui_read(bytes,1,sizeof(bytes),file);int rc=ui_close(file);
    if(rc || !tag_ui_parse(&state,bytes,n)) {
        ui_text(content,"Данные обновляются.\n\nНажмите «Обновить»\nещё раз.");return;
    }
    char text[384]={0};size_t at=0;
    append(text,&at,sizeof(text),state.name);
    append(text,&at,sizeof(text),"\nUID: ");
    append(text,&at,sizeof(text),state.uid[0]?state.uid:"—");
    append(text,&at,sizeof(text),"\n");append(text,&at,sizeof(text),state.technology);
    append(text,&at,sizeof(text),state.supported?"\nNDEF: поддерживается":"\nТолько сведения");
    /* A file snapshot is not a live NFC/ATT acknowledgement. Do not display
     * stale READY as a new successful connection or promise field detection. */
    append(text,&at,sizeof(text),"\nСнимок: ");append(text,&at,sizeof(text),state.state);
    ui_text(content,text);
}
#else
static void field(ui_obj *obj,const char *prefix,const char *value) {
    if(!obj)return;
    if(!value){ui_add_flag(obj,1u);return;}
    char text[192]={0};size_t at=0;
    append(text,&at,sizeof(text),prefix);append(text,&at,sizeof(text),value);
    ui_text(obj,text);ui_clear_flag(obj,1u);
}
static void unavailable(const char *name,const char *explanation) {
    field(content,"",name);field(technology_field,"",explanation);
    field(uid_field,"",0);field(support_field,"",0);field(state_field,"",0);
}
static const char *state_name(const char *state) {
    static const char *const values[]={"OFFLINE","SELECTED","STARTING","READY","STOPPING","ERROR"};
    static const char *const names[]={"Ожидание","Выбрана","Подготовка","Готово","Остановка","Ошибка"};
    for(unsigned i=0;i<6;i++)if(equal(state,values[i]))return names[i];
    return "Неизвестно";
}
static void refresh(void) {
    if(!content)return;
    char bytes[161];tag_ui_state state;
    void *file=ui_open("/tmp/tag_catalog_state","r");
    if(!file){unavailable("Метка не выбрана","Выберите метку на Pixel");return;}
    size_t n=ui_read(bytes,1,sizeof(bytes),file);int rc=ui_close(file);
    if(rc || !tag_ui_parse(&state,bytes,n)) {
        unavailable("Данные обновляются","Нажмите «Обновить» ещё раз");return;
    }
    if(equal(state.name,"No tag selected") && equal(state.state,"OFFLINE") && !state.supported) {
        unavailable("Метка не выбрана","Выберите метку на Pixel");return;
    }
    field(content,"Название\n",state.name);
    field(uid_field,"UID\n",state.uid[0]?state.uid:0);
    field(technology_field,"Технология\n",state.technology[0]?state.technology:"Не указана");
    field(support_field,"",state.supported?"NDEF: поддерживается":"Только сведения");
    /* This is the last manually read state, never a fresh NFC acknowledgement. */
    field(state_field,"Последнее состояние\n",state_name(state.state));
}
#endif
static void clicked(ui_event *event) {(void)event;refresh();}
#ifndef TAG_UI_SCROLL_V2
static ui_obj *label(ui_obj *parent,const char *text,int y,int height) {
    ui_obj *obj=ui_label(parent);if(!obj)return 0;
    ui_style(obj,(void *)(uintptr_t)0x200d68fcu,255,0);
    ui_size(obj,304,height);ui_align(obj,1,16,y);ui_long_mode(obj,0);ui_text(obj,text);return obj;
}
#else
static ui_obj *flow_label(ui_obj *parent,const char *text,int width,int gap) {
    ui_obj *obj=ui_label(parent);if(!obj)return 0;
    ui_style(obj,(void *)(uintptr_t)0x200d68fcu,255,0);
    ui_size(obj,width,UI_SIZE_CONTENT);ui_long_mode(obj,0);
    ui_pad_left(obj,0,0);ui_pad_right(obj,0,0);ui_pad_top(obj,0,0);ui_pad_bottom(obj,gap,0);
    ui_text(obj,text);return obj;
}
static int create_scroll(ui_obj *parent) {
    ui_obj *root=ui_object(parent,0);if(!root)return -1;
    ui_size(root,336,480);ui_align(root,1,0,0);
    ui_pad_left(root,16,0);ui_pad_right(root,16,0);ui_pad_top(root,20,0);ui_pad_bottom(root,24,0);
    ui_bg(root,0xff07111fu,0);ui_opa(root,255,0);
    ui_add_flag(root,0x10u);ui_scroll_dir(root,UI_DIR_VERTICAL);
    ui_flex_flow(root,UI_FLOW_COLUMN);ui_flex_align(root,0,0,0);
    /* Native flex places children after their actual wrapped text heights.
     * 288 px leaves room for root border, padding and a vertical scrollbar.
     * No fixed y positions, text heights, or guessed font metrics are used. */
    (void)flow_label(root,"NFC-метки",288,20);
    content=flow_label(root,"",288,16);
    uid_field=flow_label(root,"",288,16);
    technology_field=flow_label(root,"",288,16);
    support_field=flow_label(root,"",288,16);
    state_field=flow_label(root,"",288,20);
    if(!content || !uid_field || !technology_field || !support_field || !state_field) {
        diagnostic("UI_ALLOC",-1);return -1;
    }
    ui_obj *button=ui_button(root,0);
    if(button){
        ui_size(button,288,UI_SIZE_CONTENT);ui_clear_flag(button,0x10u);
        ui_pad_left(button,16,0);ui_pad_right(button,16,0);ui_pad_top(button,12,0);ui_pad_bottom(button,12,0);
        ui_bg(button,0xff14508au,0);ui_opa(button,255,0);
        ui_flex_flow(button,UI_FLOW_COLUMN);ui_flex_align(button,0,2,0);
        (void)flow_label(button,"Обновить",244,0);
        ui_event_cb(button,clicked,7,0);
    }
    ui_obj *footer=flow_label(root,"Pixel управляет NFC\nНазад — смахните вправо",288,0);
    if(footer)ui_pad_top(footer,20,0);
    refresh();diagnostic("PAGE_OPEN",0);return 0;
}
#endif
static const char *get_name(void) {return "NFC-метки";}
static int create(void *pm_page,ui_obj *parent,void *data) {
    (void)pm_page;(void)data;
    if(!parent || content)return -1;
#ifdef TAG_UI_SCROLL_V2
    return create_scroll(parent);
#else
    ui_obj *root=ui_object(parent,0);if(!root)return -1;
    ui_size(root,336,480);ui_align(root,1,0,0);ui_clear_flag(root,0x10u);
    ui_pad_left(root,0,0);ui_pad_right(root,0,0);ui_pad_top(root,0,0);ui_pad_bottom(root,0,0);
    ui_bg(root,0xff07111fu,0);ui_opa(root,255,0);
    (void)label(root,"NFC-метки",18,42);
    content=label(root,"",74,242);
    if(!content){diagnostic("UI_ALLOC",-1);return -1;}
    ui_obj *button=ui_button(root,0);
    if(button){
        ui_size(button,304,52);ui_align(button,1,16,324);
        ui_bg(button,0xff14508au,0);ui_opa(button,255,0);
        ui_obj *caption=label(button,"Обновить",0,40);
        if(caption){ui_size(caption,272,40);ui_align(caption,9,0,0);}
        ui_event_cb(button,clicked,7,0);
    }
    (void)label(root,"NFC: управление на Pixel\nНазад — смахните вправо",392,72);
    refresh();diagnostic("PAGE_OPEN",0);return 0;
#endif
}
static int destroy(void *pm_page,ui_obj *root,void *data) {
    (void)pm_page;(void)root;(void)data;
    /* The package manager owns root and all children. No double deletion. */
    content=0;
#ifdef TAG_UI_SCROLL_V2
    uid_field=technology_field=support_field=state_field=0;
#endif
    diagnostic("READY",0);return 0;
}
static void register_app(ui_timer *timer) {
    ui_timer_delete(timer);registration_timer=0;
    if(ui_lookup(APP_ID)){diagnostic("ID_OCCUPIED",-1);return;}
    app.name=app_name;app.app_id=APP_ID;app.get_name=get_name;
    page.name="main";page.app_id=APP_ID;page.layer=1;
    page.on_create=create;page.on_destroy=destroy;pages[0]=&page;
    int rc=ui_install(&app,pages,1);
    ui_app *installed=ui_lookup(APP_ID);
    if(rc || !installed || !equal(installed->name,app_name)) {
        diagnostic("REGISTER_FAILED",rc?rc:-1);return;
    }
    diagnostic("READY",0);
}
int module_initialize(void) {
    if(initialized)return 0;
    if(!ui_firmware_verified())return -1;
    initialized=1;diagnostic("DEFERRED",0);
    registration_timer=ui_timer_create(register_app,500,0);
    if(!registration_timer){diagnostic("TIMER_FAILED",-1);return -1;}
    return 0;
}
#ifndef TAG_UI_HOST_TEST
__attribute__((constructor,used)) static void module_constructor(void) {(void)module_initialize();}
#endif

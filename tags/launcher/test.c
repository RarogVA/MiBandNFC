#define TAG_UI_HOST_TEST 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "module.c"

struct ui_obj {
    unsigned id;
#ifdef TAG_UI_SCROLL_V2
    struct ui_obj *parent;
    int32_t width,height;
    unsigned flags,flow,direction,manual_position;
    char text[512];
#endif
};
struct ui_timer {unsigned id;};
static struct ui_obj objects[32];static unsigned object_count;
static struct ui_timer timer_object;
static int firmware_ok,timer_fail,install_rc,occupied,installed,deleted,timer_count,install_count,read_close;
static void (*deferred)(ui_timer *);
static char diagnostic_text[128],content_text[512];
static const char *read_text;
static char read_file,write_file;
int ui_firmware_verified(void){return firmware_ok;}
void *ui_open(const char *p,const char *m){
    if(!strcmp(m,"w")){assert(!strcmp(p,"/tmp/nfc_tags_ui_state"));return &write_file;}
    assert(!strcmp(p,"/tmp/tag_catalog_state"));return read_text?&read_file:0;
}
size_t ui_read(void *p,size_t s,size_t n,void *f){assert(f==&read_file&&s==1);size_t l=strlen(read_text);if(l>n)l=n;memcpy(p,read_text,l);return l;}
size_t ui_write(const void *p,size_t s,size_t n,void *f){assert(f==&write_file&&s==1&&n<sizeof(diagnostic_text));memcpy(diagnostic_text,p,n);diagnostic_text[n]=0;return n;}
int ui_close(void *f){return f==&read_file?read_close:0;}
ui_timer *ui_timer_create(void (*cb)(ui_timer *),uint32_t ms,void *data){assert(ms==500&&!data);timer_count++;deferred=cb;return timer_fail?0:&timer_object;}
void ui_timer_delete(ui_timer *t){assert(t==&timer_object);deleted++;}
ui_app *ui_lookup(uint16_t id){assert(id==APP_ID);return occupied||installed?&app:0;}
int ui_install(ui_app *a,ui_page **p,int n){assert(a==&app&&p[0]==&page&&n==1);install_count++;installed=!install_rc;return install_rc;}
ui_obj *ui_object(ui_obj *parent,void *data){
    assert(parent&&!data&&object_count<32);
    ui_obj *obj=&objects[object_count++];memset(obj,0,sizeof(*obj));
#ifdef TAG_UI_SCROLL_V2
    obj->parent=parent;
#endif
    return obj;
}
ui_obj *ui_label(ui_obj *parent){return ui_object(parent,0);}
ui_obj *ui_button(ui_obj *parent,void *data){return ui_object(parent,data);}
void ui_size(ui_obj *o,int32_t w,int32_t h){assert(o&&w>0&&h>0);
#ifdef TAG_UI_SCROLL_V2
    o->width=w;o->height=h;
#endif
}
void ui_align(ui_obj *o,int align,int32_t x,int32_t y){(void)x;(void)y;assert(o&&(align==1||align==9));
#ifdef TAG_UI_SCROLL_V2
    o->manual_position++;
#endif
}
void ui_clear_flag(ui_obj *o,uint32_t f){
#ifdef TAG_UI_SCROLL_V2
    assert(o&&(f==0x10||f==1));o->flags&=~f;
#else
    assert(o&&f==0x10);
#endif
}
void ui_event_cb(ui_obj *o,void (*cb)(ui_event *),uint8_t filter,void *data){assert(o&&cb==clicked&&filter==7&&!data);}
void ui_text(ui_obj *o,const char *t){assert(o&&t);if(o==content){assert(strlen(t)<sizeof(content_text));strcpy(content_text,t);}
#ifdef TAG_UI_SCROLL_V2
    assert(strlen(t)<sizeof(o->text));strcpy(o->text,t);
#endif
}
void ui_style(ui_obj *o,void *s,uint8_t opacity,uint32_t selector){assert(o&&s&&opacity==255&&!selector);}
void ui_bg(ui_obj *o,uint32_t c,uint32_t s){(void)c;assert(o&&!s);}
void ui_opa(ui_obj *o,uint8_t p,uint32_t s){assert(o&&p==255&&!s);}
#ifdef TAG_UI_SCROLL_V2
#define PAD(name) void name(ui_obj *o,int32_t p,uint32_t s){assert(o&&p>=0&&p<=24&&!s);}
void ui_add_flag(ui_obj *o,uint32_t flags){assert(o&&(flags==1||flags==0x10));o->flags|=flags;}
void ui_scroll_dir(ui_obj *o,uint8_t dir){assert(o);o->direction=dir;}
void ui_flex_flow(ui_obj *o,uint8_t flow){assert(o);o->flow=flow;}
void ui_flex_align(ui_obj *o,uint8_t main,uint8_t cross,uint8_t track){assert(o&&!main&&(cross==0||cross==2)&&!track);}
#else
#define PAD(name) void name(ui_obj *o,int32_t p,uint32_t s){assert(o&&!p&&!s);}
#endif
PAD(ui_pad_left) PAD(ui_pad_right) PAD(ui_pad_top) PAD(ui_pad_bottom)
void ui_long_mode(ui_obj *o,uint8_t mode){assert(o&&mode==0);}
static void reset(void){
    memset(&app,0,sizeof(app));memset(&page,0,sizeof(page));pages[0]=0;
    registration_timer=0;content=0;initialized=0;
#ifdef TAG_UI_SCROLL_V2
    uid_field=technology_field=support_field=state_field=0;
#endif
    firmware_ok=1;timer_fail=install_rc=occupied=installed=deleted=timer_count=install_count=read_close=0;
    object_count=0;deferred=0;read_text=0;diagnostic_text[0]=content_text[0]=0;
}
static const char valid[]="TAG1\nExample\nAABBCCDD\nNDEF / ISO-DEP\n1\nREADY\nEND\n";
static void parser_test(void){
    tag_ui_state s,old;memset(&s,0xa5,sizeof(s));old=s;
    for(size_t n=0;n<sizeof(valid)-1;n++){assert(!tag_ui_parse(&s,valid,n));assert(!memcmp(&s,&old,sizeof(s)));}
    assert(tag_ui_parse(&s,valid,sizeof(valid)-1));assert(!strcmp(s.name,"Example")&&!strcmp(s.uid,"AABBCCDD")&&s.supported);
    const char *bad[]={"TAG1\nName\nA\nTech\n1\nREADY\nEND\n","TAG1\nName\nZZ\nTech\n1\nREADY\nEND\n",
      "TAG1\nName\n\nTech\n2\nREADY\nEND\n","TAG1\nName\n\nTech\n1\nBAD\nEND\n",
      "TAG1\nName\n\nTech\n1\nREADY\nEND\nextra","TAG1\nName\r\n\nTech\n1\nREADY\nEND\n",
      "TAG1\n\n\nTech\n1\nREADY\nEND\n","TAG1\n12345678901234567890123456789012345\n\nT\n1\nREADY\nEND\n"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!tag_ui_parse(&s,bad[i],strlen(bad[i])));
    const char empty[]="TAG1\nNo tag selected\n\n\n0\nOFFLINE\nEND\n";assert(tag_ui_parse(&s,empty,sizeof(empty)-1));
}
int main(void){
    parser_test();reset();firmware_ok=0;assert(module_initialize()==-1&&!timer_count);
    reset();timer_fail=1;assert(module_initialize()==-1&&strstr(diagnostic_text,"TIMER_FAILED"));
    reset();assert(module_initialize()==0&&module_initialize()==0&&timer_count==1);occupied=1;deferred(&timer_object);
    assert(deleted==1&&!install_count&&strstr(diagnostic_text,"ID_OCCUPIED"));
    reset();module_initialize();install_rc=-12;deferred(&timer_object);assert(strstr(diagnostic_text,"REGISTER_FAILED\n-12"));
    reset();module_initialize();deferred(&timer_object);
    assert(installed&&deleted==1&&timer_count==1&&!page.on_back&&!app.icon);
    assert(strstr(diagnostic_text,"READY\n0")&&page.on_create&&page.on_destroy);
    struct ui_obj parent={.id=99};read_text=valid;assert(page.on_create(&page,&parent,0)==0);
#ifdef TAG_UI_SCROLL_V2
    assert(strstr(content_text,"Example")&&strstr(state_field->text,"Последнее состояние\nГотово"));
    /* Font-independent layout contract: every text/control sizes from content
     * and participates in native column layout; no fixed child y/height can
     * collide when text wraps. We do not emulate or guess firmware font metrics. */
    ui_obj *viewport=content->parent;
    assert(viewport->height==480&&viewport->width==336&&viewport->flow==1);
    assert((viewport->flags&0x10)&&viewport->direction==12);
    for(unsigned i=1;i<object_count;i++) {
        assert(objects[i].height==UI_SIZE_CONTENT&&!objects[i].manual_position);
        assert(objects[i].width<=288&&objects[i].parent->flow==1);
    }
    const char longest[]="TAG1\nWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW\n0123456789ABCDEF0123\nMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n1\nSTARTING\nEND\n";
    read_text=longest;clicked(0);
    assert(!strcmp(content->text,"Название\nWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"));
    assert(!strcmp(uid_field->text,"UID\n0123456789ABCDEF0123"));
    assert(!strcmp(technology_field->text,"Технология\nMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"));
    assert(!strcmp(state_field->text,"Последнее состояние\nПодготовка"));
    read_text="TAG1\nNo tag selected\n\n\n0\nOFFLINE\nEND\n";clicked(0);
    assert(!strcmp(content_text,"Метка не выбрана"));
    assert(!strcmp(technology_field->text,"Выберите метку на Pixel"));
    assert((uid_field->flags&1)&&(support_field->flags&1)&&(state_field->flags&1));
    read_text=valid;clicked(0);assert(!(uid_field->flags&1)&&!(state_field->flags&1));
#else
    assert(strstr(content_text,"Example")&&strstr(content_text,"Снимок: READY"));
#endif
    assert(create(&page,&parent,0)==-1);read_text="TAG1\n";clicked(0);assert(strstr(content_text,"Данные обновляются"));
    read_text=valid;read_close=-1;clicked(0);assert(strstr(content_text,"Данные обновляются"));
    read_text=0;read_close=0;clicked(0);
#ifdef TAG_UI_SCROLL_V2
    assert(strstr(content_text,"Метка не выбрана"));
#else
    assert(strstr(content_text,"Нет данных"));
#endif
    assert(page.on_destroy(&page,&parent,0)==0&&!content);unsigned before=object_count;clicked(0);assert(object_count==before);
    assert(page.on_create(&page,&parent,0)==0&&timer_count==1);page.on_destroy(&page,&parent,0);
    puts("launcher parser, registration, lifecycle, file race, manual refresh and active layout profile: PASS");return 0;
}

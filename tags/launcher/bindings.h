#ifndef TAG_UI_BINDINGS_H
#define TAG_UI_BINDINGS_H
#include "abi.h"
int ui_firmware_verified(void);
void *ui_open(const char *,const char *);
size_t ui_read(void *,size_t,size_t,void *);
size_t ui_write(const void *,size_t,size_t,void *);
int ui_close(void *);
ui_timer *ui_timer_create(void (*)(ui_timer *),uint32_t,void *);
void ui_timer_delete(ui_timer *);
ui_app *ui_lookup(uint16_t);
int ui_install(ui_app *,ui_page **,int);
ui_obj *ui_object(ui_obj *,void *);
ui_obj *ui_label(ui_obj *);
ui_obj *ui_button(ui_obj *,void *);
void ui_size(ui_obj *,int32_t,int32_t);
void ui_align(ui_obj *,int,int32_t,int32_t);
void ui_clear_flag(ui_obj *,uint32_t);
void ui_event_cb(ui_obj *,void (*)(ui_event *),uint8_t,void *);
void ui_text(ui_obj *,const char *);
void ui_style(ui_obj *,void *,uint8_t,uint32_t);
void ui_bg(ui_obj *,uint32_t,uint32_t);
void ui_opa(ui_obj *,uint8_t,uint32_t);
void ui_pad_left(ui_obj *,int32_t,uint32_t);
void ui_pad_right(ui_obj *,int32_t,uint32_t);
void ui_pad_top(ui_obj *,int32_t,uint32_t);
void ui_pad_bottom(ui_obj *,int32_t,uint32_t);
void ui_long_mode(ui_obj *,uint8_t);
#ifndef TAG_UI_HOST_TEST
#define UI_FN(addr,type) ((type)(uintptr_t)((addr)|1u))
#define ui_open UI_FN(0x2c1b9364u,void *(*)(const char *,const char *))
#define ui_read UI_FN(0x2c1b9568u,size_t (*)(void *,size_t,size_t,void *))
#define ui_write UI_FN(0x2c1b9ad4u,size_t (*)(const void *,size_t,size_t,void *))
#define ui_close UI_FN(0x2c1b9438u,int (*)(void *))
#define ui_timer_create UI_FN(0x2c558208u,ui_timer *(*)(void (*)(ui_timer *),uint32_t,void *))
#define ui_timer_delete UI_FN(0x2c558330u,void (*)(ui_timer *))
#define ui_lookup UI_FN(0x2c7ab19cu,ui_app *(*)(uint16_t))
#define ui_install (*(int (**)(ui_app *,ui_page **,int))(uintptr_t)0x200b72e0u)
#define ui_object UI_FN(0x2c558560u,ui_obj *(*)(ui_obj *,void *))
#define ui_label UI_FN(0x2c559b28u,ui_obj *(*)(ui_obj *))
#define ui_button UI_FN(0x2c559998u,ui_obj *(*)(ui_obj *,void *))
#define ui_size UI_FN(0x2c558ee0u,void (*)(ui_obj *,int32_t,int32_t))
#define ui_align UI_FN(0x2c5593b0u,void (*)(ui_obj *,int,int32_t,int32_t))
#define ui_clear_flag UI_FN(0x2c559b38u,void (*)(ui_obj *,uint32_t))
#define ui_event_cb UI_FN(0x2c557df0u,void (*)(ui_obj *,void (*)(ui_event *),uint8_t,void *))
#define ui_text UI_FN(0x2c558fc0u,void (*)(ui_obj *,const char *))
#define ui_style UI_FN(0x2c44a66cu,void (*)(ui_obj *,void *,uint8_t,uint32_t))
#define ui_bg UI_FN(0x2c5594b8u,void (*)(ui_obj *,uint32_t,uint32_t))
#define ui_opa UI_FN(0x2c5594a0u,void (*)(ui_obj *,uint8_t,uint32_t))
#define ui_pad_left UI_FN(0x2c558750u,void (*)(ui_obj *,int32_t,uint32_t))
#define ui_pad_right UI_FN(0x2c5591c8u,void (*)(ui_obj *,int32_t,uint32_t))
#define ui_pad_top UI_FN(0x2c557c40u,void (*)(ui_obj *,int32_t,uint32_t))
#define ui_pad_bottom UI_FN(0x2c559470u,void (*)(ui_obj *,int32_t,uint32_t))
#define ui_long_mode UI_FN(0x2c5591f8u,void (*)(ui_obj *,uint8_t))
#endif
#ifdef TAG_UI_SCROLL_V2
#include "scroll_bindings.h"
#endif
#endif

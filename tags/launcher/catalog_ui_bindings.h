#ifndef TAG_CATALOG_UI_BINDINGS_H
#define TAG_CATALOG_UI_BINDINGS_H
#include "bindings.h"
int catalog_ui_bindings_verified(void);
void ui_text_align(ui_obj *,uint8_t,uint32_t);
ui_obj *ui_image(ui_obj *,void *);
void ui_image_src(ui_obj *,const void *);
/* Lua remove_style_all wrapper 0x2c7e9c30 calls this with the native object
 * only. Reset an owned backdrop before assigning its geometry and colors. */
void ui_remove_style_all(ui_obj *);
#ifndef TAG_UI_HOST_TEST
#define ui_text_align UI_FN(0x2c5599a0u,void (*)(ui_obj *,uint8_t,uint32_t))
#define ui_image UI_FN(0x2c5587a0u,ui_obj *(*)(ui_obj *,void *))
#define ui_image_src UI_FN(0x2c557dd0u,void (*)(ui_obj *,const void *))
#define ui_remove_style_all UI_FN(0x2c5582b8u,void (*)(ui_obj *))
#endif
#endif

#ifndef TAG_UI_SCROLL_BINDINGS_H
#define TAG_UI_SCROLL_BINDINGS_H
/* This firmware's Lua export differs from upstream LVGL: do not substitute
 * 0x3fffffff for its content-size sentinel. See scroll-evidence.json. */
#define UI_SIZE_CONTENT ((int32_t)0x200007d1)
#define UI_FLOW_COLUMN 1u
#define UI_DIR_VERTICAL 12u
void ui_add_flag(ui_obj *,uint32_t);
void ui_scroll_dir(ui_obj *,uint8_t);
void ui_flex_flow(ui_obj *,uint8_t);
void ui_flex_align(ui_obj *,uint8_t,uint8_t,uint8_t);
#ifndef TAG_UI_HOST_TEST
#define ui_add_flag UI_FN(0x2c557c98u,void (*)(ui_obj *,uint32_t))
#define ui_scroll_dir UI_FN(0x2c559138u,void (*)(ui_obj *,uint8_t))
#define ui_flex_flow UI_FN(0x2c5598f8u,void (*)(ui_obj *,uint8_t))
#define ui_flex_align UI_FN(0x2c558aa8u,void (*)(ui_obj *,uint8_t,uint8_t,uint8_t))
#endif
#endif

#ifndef TAG_UI_STATE_H
#define TAG_UI_STATE_H
#include <stddef.h>
typedef struct {
    char name[33], uid[21], technology[49], state[9];
    unsigned supported;
} tag_ui_state;
/* Accept one complete, bounded snapshot. A partial fopen("w") update fails. */
int tag_ui_parse(tag_ui_state *out, const char *bytes, size_t length);
#endif

#ifndef TAG_DISPLAY_H
#define TAG_DISPLAY_H
#include "tag_record.h"
/* Control-loop only. Writes on selection/state changes, never each APDU. */
/* Success means one full file write and a successful close. */
int tag_display_write(const tag_record *record,const char *state);
#ifdef DIAGNOSTIC_HOST_TEST
void tag_display_test_reset(void);
#endif
#endif

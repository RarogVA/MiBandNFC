#ifndef TAG_RECORD_H
#define TAG_RECORD_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char name[33], uid[21], technology[49];
    uint8_t supported;
} tag_record;
/* Entire validated update is committed at once; a failure leaves out intact. */
int tag_record_parse(tag_record *out,const uint8_t *payload,size_t length);
int tag_apdu_allowed(const uint8_t *apdu,size_t length);
#endif

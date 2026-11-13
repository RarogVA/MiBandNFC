#ifndef APDU_LAB_DIAGNOSTIC_H
#define APDU_LAB_DIAGNOSTIC_H
#include <stdint.h>
/* Best-effort ASCII snapshot in /tmp/apdu_lab_state. Call only from the
 * initialization/control thread, never interrupt or Bluetooth/HAL callbacks.
 * Stage is capped at 48 bytes; non-printable/non-ASCII bytes become '_'.
 * An unchanged normalized (stage,code,detail) is not written again, including
 * after an I/O failure. A changed tuple permits another attempt. */
void diagnostic_set(const char *stage,int code,uint32_t detail);
#ifdef DIAGNOSTIC_HOST_TEST
void diagnostic_test_reset(void);
#endif
#endif

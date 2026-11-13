#ifndef MB_NATIVE_CLOCK_H
#define MB_NATIVE_CLOCK_H
#include <stdint.h>
/* CLOCK_MONOTONIC on exact p67glt 3.201.026. 0 success, -1 mismatch/error.
 * Milliseconds wrap modulo 2^32; compare deadlines with unsigned subtraction.
 * No wall-clock changes and no assumption about the UI polling interval. */
int native_clock_ms(uint32_t *milliseconds);
#endif

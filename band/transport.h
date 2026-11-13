#ifndef MB_APDU_TRANSPORT_H
#define MB_APDU_TRANSPORT_H
#include <stdint.h>
#define TRANSPORT_MAX_REQUEST 261u
#define TRANSPORT_MAX_RESPONSE 258u
#define TRANSPORT_REQUEST 1u
#define TRANSPORT_RESPONSE 2u
#define TRANSPORT_STATUS 3u
#define TRANSPORT_CONTROL 4u
#ifdef TRANSPORT_TAG_PROFILE
#define TRANSPORT_TAG 5u
#define TRANSPORT_MAX_TAG 128u
#endif
typedef void (*transport_frame_cb)(uint8_t type, uint32_t sequence,
                                  const uint8_t *payload, uint16_t length);
/* Exact p67glt 3.201.026 only. Calls must originate from the module's single
 * control/UI thread, never the Bluetooth callback thread. Call poll frequently.
 * start registers a separate service and owns only its new advertising handle.
 * The current task group's process-level Bluetooth instance is selected or
 * created by firmware and retained until reboot; it is never deleted here.
 * No pairing, adapter reset, or modification of Xiaomi's services/advertisers. */
int transport_start(transport_frame_cb callback);
int transport_notify(uint8_t type, uint32_t sequence,
                     const uint8_t *payload, uint16_t length);
void transport_poll(uint32_t monotonic_ms);
void transport_stop(void);
int transport_ready(void);
int transport_error(void);
/* Read-only diagnostic snapshot, sampled from the control/UI poll thread.
 * bit0 running; bits4-5 table_state (0 pending,1 added,2 failed);
 * bits8-9 adv_state (0 pending,1 started,2 stopped,3 failed);
 * bit12 advertising attempted; bit16 service present; bit20 subscribed.
 * Callback-owned fields use atomic loads; this is not a coherent state lock. */
uint32_t transport_debug_state(void);
/* Monotonically changes when a peer/subscription changes; root must invalidate
 * its pending request and timeout state when this value changes. */
uint32_t transport_generation(void);
/* The captured session of the frame currently passed to the callback.
 * Read only from that callback; compare with transport_generation(). */
uint32_t transport_frame_generation(void);
#endif

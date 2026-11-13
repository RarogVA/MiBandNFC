#ifndef APDU_LAB_NFC_PROBE_H
#define APDU_LAB_NFC_PROBE_H
#include <stddef.h>
#include <stdint.h>

#define NFC_PROBE_MAX_CAPDU 261u
#define NFC_PROBE_MAX_RAPDU 258u
/* Callback runs from nfc_probe_poll(), never the HAL RX thread. Bytes are valid
 * only until callback return; the owner must copy them before returning. */
typedef void (*nfc_probe_apdu_cb)(const uint8_t *, size_t, void *);
/* start/stop return 0 when accepted, not when their asynchronous work finishes.
 * All public functions must run on the same module/event-loop thread. */
int nfc_probe_start(nfc_probe_apdu_cb callback, void *context);
int nfc_probe_send(const uint8_t *response, size_t length);
int nfc_probe_stop(void);
void nfc_probe_poll(uint32_t now_ms);
int nfc_probe_ready(void);
/* Cleanup-state helper only. The enclosing module must NEVER be unloaded;
 * a preempted HAL callback can remain in flight after the RAM slot is restored. */
int nfc_probe_can_unload(void);
int nfc_probe_error(void);
const char *nfc_probe_status(void);
/* Last rejected/malformed command response, retained across STOP until the
 * next START passes preflight/discovery gates. Empty when none; at most 121
 * ASCII bytes. The raw field
 * is a separate four-byte HAL witness, not command correlation or acceptance. */
const char *nfc_probe_diagnostic(void);
/* Read cached capabilities only after exact fingerprints/stack and NCI2.0
 * layout gates. No controller command, discovery change, hook or route write.
 * Separate buffer, <=121 ASCII bytes, valid until the next INFO call. Reports
 * unavailable while another HAL hook is installed, including our active test.
 * chip=00 is NXP family, 01 is THN31, FF is unknown; AID support is not HCE proof. */
const char *nfc_probe_info(void);

#ifdef NFC_PROBE_HOST_TEST
void nfc_probe_test_rx(uint16_t length, uint8_t *packet);
#endif
#endif

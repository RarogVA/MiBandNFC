#ifndef TAG_BOOTSTRAP_LAUNCH_H
#define TAG_BOOTSTRAP_LAUNCH_H
#include <stdint.h>

/* Exact p67glt 3.201.026 only. Invoke from an LVGL/UI timer after native
 * registration, never synchronously from the NSH insmod constructor. */
int tag_bootstrap_bindings_verified(void);
int tag_bootstrap_open_app(uint16_t app_id);

/* Same synchronous spawn/wait backend used by the firmware Lua os.execute.
 * Only the fixed installer commands below are exposed, not arbitrary input. */
int tag_bootstrap_insmod_core(void);

#endif

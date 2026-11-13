#ifndef TAG_CATALOG_API_H
#define TAG_CATALOG_API_H
#include <stdint.h>
#include <stddef.h>
#define TAG_CATALOG_API_MAGIC 0x54434131u
#define TAG_CATALOG_API_VERSION 1u
#define TAG_CATALOG_API_PROFILE 0x00010001u
#define TAG_CATALOG_API_PATH "/tmp/tag_catalog_api"
#define TAG_CATALOG_PROVIDER_ID 0xd6u
#define TAG_CATALOG_PROVIDER_NAME "dev.mbapdu.catalog.api.v1"
#define TAG_CATALOG_PROVIDER_LABEL "NFC Catalog API v1"
#define TAG_CATALOG_MAX_RECORDS 32u
#define TAG_CATALOG_NAME_BYTES 96u
#define TAG_CATALOG_NDEF_BYTES 1024u
/* Native ARM ABI. Output structs are caller-owned copies. UI and core calls
 * originate on the same firmware UI/control thread, never the BLE callback. */
typedef struct {
    uint8_t id[16];
    uint32_t revision;
    uint8_t ndef_sha256[32];
    uint16_t ndef_length;
    uint8_t kind;
    uint8_t reserved;
    char name[97],uid[21],technology[49];
    uint8_t padding;
} tag_catalog_entry;
enum tag_catalog_phase {
    TAG_CATALOG_IDLE=0,TAG_CATALOG_STOPPING=1,TAG_CATALOG_WAIT_PIXEL=2,
    TAG_CATALOG_SELECTED=3,TAG_CATALOG_STARTING=4,TAG_CATALOG_READY=5,
    TAG_CATALOG_ERROR=6
};
typedef struct {
    uint32_t heartbeat,poll_age_ms,catalog_revision,peer_generation;
    uint16_t count;
    int16_t selected_index;
    uint8_t phase,ble_ready,nfc_ready,clean;
    uint8_t authorized,transaction_active,loop_live,reserved;
    int32_t error;
} tag_catalog_health;
typedef struct tag_catalog_api {
    uint32_t magic;
    uint16_t version,bytes;
    uint32_t profile;
    int (*health)(tag_catalog_health *out);
    int (*get)(uint32_t catalog_revision,uint16_t index,tag_catalog_entry *out);
    int (*select)(uint32_t catalog_revision,const uint8_t id[16],uint32_t record_revision);
    int (*start)(void); /* Always -1 in v1: activation stays explicit on Pixel. */
    int (*stop)(void);
} tag_catalog_api;
typedef struct {char label[32];tag_catalog_api api;} tag_catalog_provider;
/* get:0 success,-1 invalid/stale; select:0 queued,-1 invalid/stale,-2 busy.
 * stop:0 queued. health:0 snapshot available; inspect loop_live, not file marker.
 * uint32 poll_age_ms is unsigned monotonic elapsed time; UINT32_MAX means unknown.
 * selected_index=-1 means none. phase is local state, not an RF-field detector.
 * Trusted discovery: firmware app_lookup(0xD6), verify app.name exactly
 * TAG_CATALOG_PROVIDER_NAME, call its native get_name(). The returned pointer
 * is the beginning of static tag_catalog_provider; verify label[32], then API
 * magic/version/bytes/profile before any API call. Never execute an address
 * supplied by a filesystem descriptor. TAG_CATALOG_API_PATH is diagnostic only. */
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(tag_catalog_entry)==224,"catalog entry ARM ABI");
_Static_assert(sizeof(tag_catalog_health)==32,"catalog health ARM ABI");
_Static_assert(sizeof(tag_catalog_api)==32,"catalog API ARM ABI");
_Static_assert(sizeof(tag_catalog_provider)==64,"catalog provider ARM ABI");
#endif
#endif

#ifndef APDU_LAB_ROUTE_TABLE_H
#define APDU_LAB_ROUTE_TABLE_H
#include <stddef.h>
#include <stdint.h>

#define RT_CAPACITY 4096u
#define RT_MAX_ENTRIES 256u
#define RT_NCI_PAYLOAD_MAX 255u
#ifdef TRANSPORT_TAG_PROFILE
#define RT_PROBE_AID_LENGTH 7u
#else
#define RT_PROBE_AID_LENGTH 8u
#endif

enum { RT_OK=0, RT_BAD=-1, RT_FULL=-2, RT_CONFLICT=-3, RT_INCOMPLETE=-4 };
typedef struct {
    uint8_t bytes[RT_CAPACITY];
    uint16_t length;
    uint16_t entries;
    uint8_t complete;
    uint8_t invalid;
} route_table;

extern const uint8_t rt_probe_aid[RT_PROBE_AID_LENGTH];
void rt_init(route_table *table);
/* Payload of one RF_GET_LISTEN_MODE_ROUTING_NTF, excluding NCI header.
 * Unknown TLVs are preserved byte-for-byte; malformed/oversized snapshots
 * become unusable. A complete snapshot is mandatory before any mutation. */
int rt_snapshot_fragment(route_table *table, const uint8_t *payload, size_t length);
int rt_add_probe(route_table *table);
/* Removes exactly the route this prototype installs. A different route with
 * the same AID fails instead of overwriting another owner's changes. */
int rt_remove_probe(route_table *table);
/* Serialize one SET_ROUTING payload. cursor starts at 0 and advances only on
 * success; output includes more/count. Returns payload length, 0 if finished,
 * negative on error. Empty table emits a single 00 00 payload. */
int rt_set_chunk(const route_table *table, size_t *cursor,
                 uint8_t *output, size_t capacity);
int rt_equal(const route_table *a, const route_table *b);
#endif

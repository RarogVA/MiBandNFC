# Catalog profile v1

This is a separate native profile. Frozen tag core v2 remains unchanged.
Service/RX/TX UUIDs and MB/v1 framing stay the same. All integers below are
**unsigned, big endian**, except the local C ABI which uses native ARM layout.
UUIDs are 16 canonical network-order bytes. MB payloads remain at most 261 bytes.
Before using types 6/7, Pixel sends CONTROL `[04]`; the matching STATUS must be
exactly `CATALOG_V1`. A legacy core does not implement this capability.

## Snapshot

Maximum 65536 bytes, 32 records, 1024 NDEF bytes per record. Header (12 bytes):
`"TCAT"`, u16 format version=1, u16 record count, u32 catalog revision (>0).
Each record is a u16 body length followed by:

| Field | Bytes |
|---|---:|
| Stable UUID | 16 |
| Record revision (>0) | 4 |
| Kind: 0 metadata only, 1 NDEF | 1 |
| Name byte length | 1 |
| UID hex byte length | 1 |
| Technology byte length | 1 |
| NDEF length | 2 |
| Name, valid UTF-8, no controls, 1..96 bytes | variable |
| UID, uppercase ASCII hex, even length, 0..20 bytes | variable |
| Technology, printable ASCII, 0..48 bytes | variable |
| NDEF bytes, 0..1024 | variable |

The fixed body is 26 bytes. IDs must be nonzero and unique. Kind 0 requires zero
NDEF length; kind 1 requires nonempty NDEF. No implicit padding/trailing bytes.
SHA256 covers the entire snapshot, including header and lengths. A record's
NDEF SHA256 covers only its NDEF bytes (including the standard empty digest for
metadata-only records). Hashes ensure content identity/integrity, not pairing
or sender authentication. No card secrets, payment data or access-key dumps are
part of this format.

## Type 6: serialized catalog transaction

One command at a time. Android waits for both ATT completion and matching
STATUS sequence before sending the next command. Commands only succeed while
NFC is completely STOPPED; a transaction never silently interrupts active NFC.

| Opcode | Remaining payload | Matching STATUS |
|---|---|---|
| 01 BEGIN | u32 revision, u8 count, u32 total length, SHA256[32] | `CAT_BEGIN` |
| 02 DATA | u32 exact next offset, 1..240 bytes | `CAT_DATA:<nextOffset>` |
| 03 COMMIT | none | `CAT_COMMIT:<revision>` |
| 04 ABORT | none | `CAT_ABORT` |
| 05 QUERY | none | `CATALOG:<revision>:<count>:<lowercase SHA256 hex>` |

BEGIN is 42 bytes including opcode. Same committed revision and same hash yield
`CAT_SAME:<revision>` and no transaction is opened; lower revision or equal
revision with different content is rejected. BEGIN supersedes an incomplete
staging transaction. DATA must be contiguous, nonempty and within declared
length; ambiguous retries are rejected and Android restarts BEGIN. COMMIT
validates length/hash/format/count/revisions/IDs before atomic rename; errors
leave the prior catalog active. A successful COMMIT invalidates prior NFC
selection authorization. ABORT discards only staging state. Before any catalog
exists QUERY reports `CATALOG:0:0:` followed by 64 zero digits. ERROR replies use
the existing `ERROR:<reason>` STATUS convention. No payload may have extra bytes.

## Type 7: selection handshake

Local band selection can be requested while offline. It stops active NFC first,
then exposes the selected entry locally; it becomes eligible for START only
after a handshake with the currently subscribed Pixel session.

REQUEST from band: `[01] + u32 catalogRevision + UUID[16] + u32 recordRevision +
SHA256(NDEF)[32]` (57 bytes). Selection request sequences start in `0x20000000`
and are separate from CAPDU sequences. Pixel verifies exact ID, both revisions,
NDEF digest, and its immutable handler data. It invalidates any previous NFC
handler before replying. ACK from Pixel: `[02]` plus the same 56 bytes and the
same MB sequence. Native replies `SELECT_OK` on that sequence only after the
tuple matches and NFC is STOPPED. Exact duplicate ACKs for the current accepted
tuple are idempotent. Unsolicited/stale/mismatched ACKs are rejected.
The handshake deadline is 30 seconds, allowing Pixel to finish capability and
catalog-summary queries before processing an early REQUEST. After expiry, a
new catalog QUERY rearms the selected entry's request; a still-live request is
not duplicated. The APDU deadline remains 3 seconds.

A disconnect, CCC/session-generation change, catalog commit, local selection
change, or STOP/fault invalidates outstanding selection requests and/or active
session authorization as documented by module state. On reconnection the
currently selected entry requires a fresh REQUEST/ACK. There is **no automatic
START**. Pixel's explicit CONTROL `[01]` remains the activation action; metadata
only records cannot start NFC. Existing CONTROL query/STOP/info, APDU framing,
3000-ms response deadline and 120-second session limit remain in force.

Pixel may explicitly propose a selection with opcode `[03]` and the same tuple
(57 bytes). Native replies `SELECT_PENDING` on the proposal's sequence, queues
STOP if necessary, then sends its own fresh REQUEST after cleanup. It sends the
proposal STATUS before that REQUEST. On reconnect the band's persisted selection
is authoritative and native reissues REQUEST automatically; Android must not
automatically propose its stale prior selection. An explicit user selection or
START action on Pixel may propose a different catalog-matching entry. Sync alone
does not start NFC.

## Local API and persistence

`api.h` is the versioned UI contract. Catalog entries remain readable without
BLE and survive reboot. The core registers its own provider app ID `0xD6`, name
`dev.mbapdu.catalog.api.v1`, without a launcher entry or pages. The UI uses the
verified firmware app lookup, checks the exact provider name, then calls its
native name callback. The returned static blob is `char label[32]` containing
`NFC Catalog API v1` plus the API table. The UI validates label, API magic,
version, size and profile before calling `health`, whose poll age and heartbeat
establish whether the existing core loop is advancing. No function pointer read
from a file is ever executed. Selection and
STOP are queued through this C API and processed by that same loop; no extra
10-ms filesystem polling task is added. No offscreen operation or RF wake is
implemented. The core and its API remain resident until reboot, never rmmod.

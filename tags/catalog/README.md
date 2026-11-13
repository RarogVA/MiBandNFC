# Persistent catalog core, candidate v1

This profile stores up to **32 tags** with stable UUIDs, catalog and record
revisions, UTF-8 names up to 96 bytes, and NDEF payloads up to 1024 bytes each.
Catalog metadata and the selected entry remain readable without Bluetooth and
survive reboot. NFC still uses the existing Type 4 NDEF relay through Pixel.
There is no payment integration, original UID emulation, access-key cloning,
offscreen support or RF-triggered wake.

Build and check from the repository root:

```sh
python3 tags/catalog/test.py
python3 tags/catalog/build.py
```

Output: `tags/catalog/build/catalog_core_v1.elf`, `tags/catalog/build/build.json`,
`tags/catalog/build/inspection.txt`, and `tags/catalog/build/tests/results.json`.
This is larger than the original 31272-byte
Recorder slot; the separately maintained catalog/bootstrap packer must include
the full ELF. Never truncate it or replace a frozen released core. The original
`python3 tags/build_band.py` continues to reproduce core v2 SHA256
`91f63ed22532c81ba14c0f4d4e6271ace8b55df0a664c6eec8d706b7788c0673`.
The new profile is enabled only with `TAG_CATALOG_PROFILE` and uses separate
build outputs. First-tap bootstrap, catalog synchronization and selection have
been observed on the band with UI1; see the [hardware observation](../../docs/evidence/tags/catalog-v1-observation.json).
A later user photograph also confirms UI2 showing the selected tag and module
OK. UI3 fixes the visible border and spacing without changing this core.
The user confirmed UI3 opening, removed borders/message row and module OK
after the requested reboot on 2026-11-14. Catalog NFC readback, catalog reboot
persistence and broader UI states still require hardware checks;
the [current integration status](../../docs/catalog-v1.md) distinguishes these
from the earlier single-tag NFC pass.

## Persistence and wire

See `PROTOCOL.md` for exact big-endian bytes and `api.h` for the native UI ABI.
Pixel must discover `CATALOG_V1` via CONTROL4 before sending catalog frames.
Snapshot transfers are serialized BEGIN/DATA/COMMIT transactions with SHA256,
explicit total length/count/revision, bounded chunks and per-command replies.
Native rejects malformed UTF-8, invalid or duplicate IDs, unbounded fields,
invalid NDEF framing and noncontiguous chunks. NDEF records are canonical
unchunked records; the decoder accepts supported nonempty messages, including
unknown TNF payloads, but rejects the chunked/unchanged/reserved TNF forms.

The loader must create `/data/tag_catalog` before `insmod`. Fixed files:

- `catalog.bin`: committed complete snapshot.
- `catalog.next`: uncommitted staging snapshot; ignored after reboot.
- `selected.bin`: selected ID/revisions/NDEF hash.
- `selected.next`: uncommitted selection; ignored after reboot.

Every write checks byte count and close result. COMMIT parses the complete
staging file into a candidate metadata table, verifies SHA256 and revision,
then uses the exact firmware POSIX rename before replacing the in-memory table.
Failures retain the prior committed catalog. Rename atomicity is used for
process-level commits; power-loss durability and filesystem-specific recovery
have **not** been established by this project. No fsync binding is assumed.
The full NDEF data stays in the snapshot file; only two bounded metadata tables
and a single record buffer are needed during validation. Catalog size at all
maximum field sizes is 38924 bytes; wire hard limit is 65536.

## Selection and lifecycle

Selecting an entry on the band queues a STOP, waits for complete NFC routing
cleanup, persists the new choice, then requests a Pixel handshake if connected.
Android can explicitly propose a choice through type7 opcode03. Both revisions,
stable ID and NDEF SHA256 must match before native accepts the ACK. The request
is valid only for its current BLE generation. A reconnect requests the band's
persisted choice again; it does not adopt a stale Pixel selection automatically.

There is no automatic NFC START. Pixel starts the already confirmed supported
selection explicitly. The existing 3-second APDU response deadline, 120-second
session limit, restricted Type 4 SELECT/READ APDUs, routing restoration and
disconnect cleanup remain active. A catalog transaction is accepted only while
NFC is STOPPED. Idle staging transactions expire after 15 seconds; selection
handshakes have a separate 30-second deadline for capability-query ordering.

The core uses one existing 10-ms UI-thread control loop; no extra timer or
filesystem command-polling loop is added. UI actions queue in-memory requests.
`health()` returns the actual advancing heartbeat and monotonic poll age;
marker files are never treated as proof that the loop is alive. The API remains
resident until reboot and neither core nor provider can be safely unloaded.

## Trusted UI discovery

The core registers its own native provider **0xD6**, package name
`dev.mbapdu.catalog.api.v1`, without a menu icon or pages. The UI uses verified
firmware `app_lookup`, checks the package name, and invokes its native name
callback. That returns a static `tag_catalog_provider`: a 32-byte NUL-padded
label `NFC Catalog API v1`, followed by the 32-byte ARM API table. Validate the
label, magic, version, size and profile before calls. No memory address supplied
by an external file is read or executed. The provider and all callback storage
have module lifetime.

The exact `.026` install implementation checks page count at `0x2c7abc42` and
branches from `0x2c7abc46` to `0x2c7abc74` for zero pages. Its pages pointer is
never dereferenced in that case. The registry/dispatch/functions are checked
against firmware fingerprints before registration. Stdio and rename are also
fingerprinted; rename at `0x2c19fbe8` is independently corroborated by the
firmware Lua `os.rename` wrapper at `0x2c61ab80`.

## Validation and remaining hardware checks

Native ASAN/UBSAN suites cover standard SHA256 vectors, the Android 75-byte
golden snapshot/digests, maximum 32×1024 catalog, malformed/truncated input,
bounded random input, short writes, close/rename faults, atomic replacement,
reload, offline UI selection, stale revisions/digests/session ACKs,
STOP-before-selection, capability/handshake ordering, heartbeat staleness,
APDU/session deadlines, new frame fragmentation and transport generation changes.
Original tag-profile and APDU Lab host suites are separate regression checks.

Hardware acceptance still needs full catalog upload, application/face switching
with a live heartbeat, native list/selection, Pixel ACK, actual NFC readback,
STOP/cleanup, and reboot persistence. A successful build or marker file proves
none of those device behaviors.

# Native NFC-tags page

This document describes the frozen legacy single-tag UI profiles. For the Pult UI4 build see [UI4](UI4.md). For the
released catalog UI3, its layout, build commands and validation status, see
[Catalog v1 and Roundabout RU NFC 2](../../docs/catalog-v1.md).

Small resident module for `miwear.watch.p67glt`, firmware **3.201.026** only.
It registers its own native app `dev.mbapdu.tags`, ID **0xD4 / 212**, page **0**.
The catalog app is launched from the general app list; the legacy NFC 5 face
can also open it from the lower circle via the native-ID widget action. The
same module registers **Mi Home** (`0xD5`, five BLE-triggered slots) and
**Модули** (`0xD7`, core reload and reboot), and calls the firmware's
`app_launcher_add` for all three, so they appear in the general app list. No
stock app ID, event hook, ROM patch, extra polling loop, or NFC command is
involved. Launcher entries, boot-time reactivation, icons and the device
acceptance checklist are described in
[launcher-and-autostart.md](../../docs/launcher-and-autostart.md).

The page shows a manually refreshed snapshot of the selected tag, original UID,
technology, support flag, and bridge state from `/tmp/tag_catalog_state`.
`READY` is explicitly labelled a snapshot; it is not a new connection or NFC
acknowledgement. NFC remains controlled by Pixel and the verified core's session
deadline. Leaving this page does **not** stop NFC. Swipe right uses the normal
system back handler. The watchface's direct-ID route (NFC 5) does not depend on the launcher
entry; both paths address the same registered app. The three installer faces
(**SmartRound #1**, **SmartRound #2**, **SmartRound #3**) use the selected
stock app after setup and leave app launches to the menu.

## Build and check

```sh
python3 tags/launcher/test.py
python3 tags/launcher/build.py
```

The default still reproduces the hardware-tested v1 ELF byte for byte.
Scrollable v2 is a separate candidate:

```sh
python3 tags/launcher/test.py --scroll-v2
python3 tags/launcher/build.py --scroll-v2
```

This writes `build/nfc_tags_ui-v2.elf`, `build/build-v2.json`,
`build/inspection-v2.txt` and `build/host-tests-v2.json`. Its diagnostics begin
with `NFC_TAGS_UI2`; the path and read-only ownership stay the same. V2 changes
only the page UI. The title, separate metadata fields, refresh button and footer
participate in a native vertical flex column. All text and the button have
content-derived height; there are no fixed child y positions or text heights.
The viewport allows vertical scrolling while leaving horizontal back to the
system. Longer names and technology strings wrap and move later fields down.
An empty catalog now shows «Метка не выбрана / Выберите метку на Pixel».
Raw protocol state names are translated beneath «Последнее состояние».

`scroll-evidence.json` records the exact additional .026 API evidence. In
particular, this firmware exports `SIZE_CONTENT = 0x200007d1`, **not** the
upstream LVGL value `0x3fffffff`. V2 checks the firmware's constant-export code,
literal and new function veneers before calling them. It keeps the previous
verified text style and makes no guesses about its font metrics. Host tests
exercise maximum-width fields, hide/reveal transitions, and ensure every flow
child uses content height and has no manual coordinates. Actual typography,
scroll gestures and horizontal return still require device acceptance.

For a future compact screen, `font-evidence.json` records read-only findings
from the exact firmware. The existing `0x200d68fc` style requests **40 px
MiSans-Medium**; smaller predefined styles are available at 22, 20 and 17 px.
Candidate native font factories can request 16/18/22 px, but their new use and
fallback handling remain unverified on the device. Font size alone does not
determine rendered line height. This evidence supports the requested visual
proposal; it does not implement or install a compact UI. Frozen v1/v2 code and
binaries remain unchanged while the user reviews the mockup.

Output: `tags/launcher/build/nfc_tags_ui.elf`, at most **9841 bytes**, for the
Recorder container's otherwise unused `app/lua/recorder.bin` resource slot.
The dual-module packer renames that resource to `app/lua/launcher.elf` and keeps
the verified tag-v2 ELF untouched. Never overwrite existing released FACE/ELF
files with this candidate. `build/build.json` records inputs, firmware hash,
fingerprints, constructor verification, size, and output SHA256.

The `SHT_INIT_ARRAY` constructor calls `module_initialize`, which validates all
24 bindings and the actual PM-install SRAM dispatch target before creating one
500-ms registration timer. Its callback deletes that timer and installs the app
on the UI thread. A collision at ID 0xD4 fails closed. The module, descriptors,
strings and callbacks remain resident until reboot. Do not `rmmod` it.

`/tmp/nfc_tags_ui_state` is diagnostic only:

```text
NFC_TAGS_UI1
READY
0
END
```

Other stages: `DEFERRED`, `ID_OCCUPIED`, `REGISTER_FAILED`, `TIMER_FAILED`,
`PAGE_OPEN`, `UI_ALLOC`. `READY` here means app registration, not NFC readiness.
The Lua installer owns `/tmp/nfc_tags_ui_loaded`; successful `insmod` alone does
not prove deferred registration succeeded, so check the diagnostic stage.

The framework owns the page root and children. Destroy only clears our label
pointer; it does not delete framework objects or the core's existing timer.
The file parser accepts exactly seven complete ASCII lines, bounded field
lengths, uppercase even-length UID hex, a support flag and a known state. It
rejects partial writes, extra data, malformed flags and unknown states without
publishing a partially parsed result.

## Evidence and limits

ABI definitions and normal registration flow derive from
[MiWatchNativeApps, commit cd644697](https://github.com/m0tral/MiWatchNativeApps/tree/cd644697993b73f9fb83bedf4aad48de0e1d7d4a), principally
`common/miwear/app.h`, `platform/mb10p/fw_3.201.026/miwear_system.h`,
`mb10pro/mb10p_app.c` and `mb10pro/ui/calc_page.c`. Its MIT copyright and license
are included in `THIRD_PARTY_LICENSE`. Recorder's supplied ELF independently
confirms app size 0x40, page size 0x74, app ID offset 0x10, page create offset
0x4c, destroy offset 0x64, PM dispatch address and UI-timer registration.

Exact local firmware SHA256:
`66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40`.
Read addresses use base `0x2c0c0000`; executable pointers use the Thumb bit.
Key verified locations:

| Function or behavior | Address |
|---|---|
| Native app lookup, returns registry descriptor | `0x2c7ab19c` |
| Native install implementation | `0x2c7abb68` |
| Install RAM dispatch; only exact ROM aliases accepted | `0x200b72e0` |
| Install ROM table target | `0x2ca9bf74` → `0x0c7abb69` |
| Optional icon NULL guard | `0x2c7abbe4`–`0x2c7abbec` |
| Back callback at page +0x68; NULL follows parent chain | `0x2c7ae8ee`–`0x2c7ae902` |
| System back to previous stack page | `0x2c7ae996` onward |

Host tests cover parsing and truncated writes, register failure/collision,
constructor idempotence, duplicate creation, destroy/recreate, failed close,
and refresh after destroy under AddressSanitizer and UndefinedBehaviorSanitizer.
These checks do not prove hardware UI rendering, font size, click/back behavior,
or continued core operation across a watchface/app transition. Those require the
separate physical acceptance run. No native core/NFC changes are made here.

# UI4 — v1.0.0

The working tree now builds **Roundabout RU NFC 4**, ID `999999990`, with UI4.
The approved Pult screen is implemented with bold labels and bounded service checks.

For the selectable lower-circle candidate **NFC 5** — 43 stock Roundabout app
choices plus **NFC-метки** — see [CHOICE.md](CHOICE.md). It is a separate
candidate and does not replace the released v1.0.0 face.
The user reported checking UI4 before the v1.0.0 release. See
[UI4 build notes](../launcher/UI4.md) for the current build. The UI3 recipe
below documents tag `v0.4.2-alpha.1`; use that tag to reproduce UI3.

# Manual bootstrap from Roundabout

Target: the user's Mi Smart Band 10 Pro Ceramic GL (`miwear.watch.p67glt`), firmware **3.201.026** only. This adds a small Lua child to the **Find Phone** lower-circle clone in the four normal Roundabout themes. Clock, metric resources, other buttons and AOD remain native. The separate candidate is **Roundabout RU NFC 3**, ID `999999991`; released IDs `999999995` and `999999994` are immutable inputs. This fresh ID avoids Notify reusing the already-installed ID `999999992`; the previous NFC 2 files are preserved. Only the face ID and its 33 localized titles change from the UI3 NFC 2 candidate; the Lua, core/UI ELFs, icons and all live/preview resources are identical.

Evaluating the face performs no shell command and loads no module. The first
tap on the Find Phone installer circle copies the embedded core/UI/artwork,
loads them with the fixed `insmod` commands, waits up to three seconds for
the exact `/tmp/nfc_tags_catalog_ui_ready` marker, writes
`/data/tag_catalog/auto_load` and requests `shutdown --reboot`. It does not
open a screen: applications are launched from the menu. A failed load shows
the fixed error code and does not reboot.

After a confirmed install, the Lua child is transparent from the first frame
(`CLICKABLE` cleared), so the native circle launches stock Find Phone. A
single LVGL timer is armed for **2000 ms**; it performs the same guarded
copy/`insmod` path silently, does not open a screen, and tolerates failure.
The timer runs on the UI thread. `/tmp` guards reset after reboot. This is a
guarded boot-time attempt, not a firmware boot service. No START/APDU/RF
command is sent by the bootstrap.

`/tmp` guards reset after reboot. The catalog core remains `Tag_Catalog_V1`; the new UI is `NFC_Tags_Catalog_UI_V4`. Its resource `app/lua/catalog_ui_v4.elf` is copied to `/data/tag_catalog/ui-catalog-v4.elf`; the persisted older ELFs are not overwritten. Loaded/READY values are exactly `catalog_ui_v4\n`. The shared native app ID remains `0xD4` and is now reached from the launcher menu, not from the circle.

Before a cold load, all `NFC_Tags_Catalog_UI_V1_attempted`, `NFC_Tags_Catalog_UI_V2_attempted` and `NFC_Tags_Catalog_UI_V3_attempted` guards are checked. A v1/v2 loaded marker, a v1/v2 READY marker even without a loaded marker, or a partial v1/v2 attempt requires a reboot; none permits warm reuse as v3. Existing core v1 can be reused. Successful `insmod` is only a load hint: the native UI checks the core provider's health independently. A failed/partial load can require a reboot; the code does not force-unload live native code.

The exact `.026` NSH `mkdir -p` returns an error for an existing final directory (EEXIST). Earlier installers ignored that return; the first hybrid bootstrap incorrectly stopped there. The bootstrap now attempts mkdir, then requires successful write/close/readback of a fixed temporary probe in `/data/tag_catalog`. It removes the probe after success. This checks actual writability without assuming a failed mkdir means the directory is unusable. Separate `DIR`, `UI SRC`, `UI DST`, `UI WRITE` and `UI CLOSE` labels identify any remaining failures; `/tmp/tag_bootstrap_diagnostic` records the corresponding fixed path. No unverified extra resource references or loader changes are used.

## Why the hybrid route is supported by this firmware

`references/recorder.face` establishes the FileResource wire format and working Lua/ELF file extraction. In the exact firmware, `lv_wfwidget_create` dispatches a kind-5 child to `lv_render_file_create` at `0x2c7d0d1c`. That renderer recognizes `.lua`, creates an independent Lua parent and VM, and sets EVENT_BUBBLE on both parent containers. No global requirement that the entire face be Lua was found. Each normal lower-circle Widget gains one transparent 84×84 Lua child above its existing image; AOD gains no Lua. The Lua-owned root calls the verified `remove_style_all()` binding (`0x2c7e9c31`) before setting its 84×84 size and transparent background. This removes the inherited rectangular border from the ordinary face; status backgrounds remain circular with radius 42. The native Widget and editor frame are unaffected.

`launch.c` models the ordinary native Widget's 44-byte launch request, checks the initialized dispatch slot, and uses the firmware's default animation callbacks with NULL contexts (explicitly supported by both callbacks). `tag_bootstrap_open_app(0xD4)` must run on a UI timer **after** registration. `tag_bootstrap_insmod_core()` invokes the same synchronous spawn/wait `system()` backend as Lua `os.execute`; its caller must perform the resident-provider and attempt guards first. Fixed commands contain no tag or user text. Evidence bytes and addresses are recorded in `evidence.json`.

For a late READY after the bounded wait, `onPressed` (firmware event 1) enables bubbling before the same touch's native SHORT_CLICKED (event 4). `onClicked` is event 7, so enabling bubbling there alone would be too late. The firmware Lua event dispatcher calls the registered function with no return values and does not stop propagation. The Timer API is taken directly from the original recorder's Lua installer. On timeout, no module is loaded again and the clickable error state remains visible.

The inherited editor selection frame was 69×69 despite the live circle being 84×84. A separate code-drawn 84×84 frame now matches the live circle's local bounds in all eight themes. Existing icon pixels and layout position `(126,274)` remain unchanged.

Origin is checked too: the native editor positions both the preview image and the frame at `(cached_slot_x−3, cached_slot_y−3)` (`0x2c560054…56005e` and `0x2c560082…56008c`). In the previous NFC face, their sizes were 84 and 69, so their centers differed by 7.5 pixels on both axes. Both are now 84 with the same origin and center, and the frame uses the same circular ellipse `(2,2)…(81,81)` as the live artwork. `editor_bounds_validation.json` records the evidence. This does not assume that the editor's scaled cached coordinates equal the full-screen live coordinates.

The implementation is informed by the primary [MiWatchNativeApps](https://github.com/m0tral/MiWatchNativeApps) example framework and the user's original recorder package. Exact binding evidence comes from the saved firmware; support for other firmware versions is not inferred from those examples.

## Build and validation

Use the project research environment containing Pillow, after building the catalog core and UI:

```sh
build/shortcut-research-venv/bin/python tags/bootstrap/build_face.py \
  --core tags/catalog/build/catalog_core_v1.elf \
  --ui tags/launcher/build/nfc_tags_catalog_ui_v3.elf \
  --extra left.bin=PATH_TO_LEFT_ICON \
  --extra right.bin=PATH_TO_RIGHT_ICON \
  --extra refresh.bin=PATH_TO_REFRESH_ICON \
  --output build/roundabout-ru-nfc3.face
```

The append-only packer verifies the frozen baseline and exact firmware hashes, round-trips the original recorder FileResource, re-reads every embedded file, validates Widget references, checks every preserved resource and restricts mutations of the original prefix to explicit index/header fields. The format permits a 24-bit file length; the old recorder-template ELF slot limits do not apply. The four original gallery images remain intact at this stage; the separate horizontal-label pass can update them afterward.

```sh
build/shortcut-research-venv/bin/python tags/roundabout/level_labels.py \
  --input build/roundabout-ru-nfc3.face \
  --output build/roundabout-ru-nfc3-final.face \
  --preview build/roundabout-ru-nfc3-final.png
```

This final pass levels all 80 bottom-label resources (five metric options, both sides, normal/AOD themes) to zero rotation. It keeps the compiled icon pixels exact and updates the four gallery previews only within old/new lettering support. It leaves numeric data, positions, arcs, native action flags and bootstrap files unchanged. It uses the saved original ZIP, the existing localization map, Pillow 11.3 and the fingerprinted Arial Bold font; the baseline glyph comparison fails closed if rasterization differs. The source and previous released faces are not overwritten.

The right Calories and heart-rate labels additionally move 12 pixels right with Y unchanged. Numeric values retain their original rotation, positions, formatting, resources and gallery pixels. Only text labels are leveled:

```sh
build/shortcut-research-venv/bin/python tags/roundabout/check_clearance.py --face build/roundabout-ru-nfc3-final.face
build/shortcut-research-venv/bin/python tags/bootstrap/verify_final.py --face build/roundabout-ru-nfc3-final.face
```

The installable candidate is then `build/roundabout-ru-nfc3-final.face`. The clearance check uses all actual progress-frame alpha masks and confirms three empty pixels before Calories and two before heart rate, on both sides in all eight themes. Icon columns remain unchanged. Final readback checks every embedded file against its source, every original data/numeric resource byte-for-byte, the native app ID and immutable previous releases. Gallery numeric pixels remain unchanged.

`test.py` runs the actual Lua with a mocked filesystem, shell and LVGL using `lupa.lua54`: no command on render, cold load order and READY handshake, warm pass-through, loaded-without-READY timeout, late READY, core failure with UI recovery, incompatible resident core, partial UI load, firmware mismatch, mkdir EEXIST with writable storage, unwritable storage and distinct UI source/target errors and reboot failure. It also checks v1/v2 loaded/partial/READY upgrade guards, preservation of both older ELF files, copying the refresh icon, removal of the inherited root border before restoring dimensions, first-tap install plus reboot, silent boot-time load without an open request, reboot failure, and the partial-state skip. Twenty-eight host tests pass. The launch wrapper also compiles for Cortex-M33 with the project's warning-as-error flags, and all four fingerprints match the saved firmware.

The user confirmed on 13 November 2026 that the v1 native page works and tags synchronize on the band. The UI v3 style-reset upgrade still needs its own device check. Regression acceptance includes first tap after reboot, native page launch, subsequent taps, both other circle buttons, all four styles/AOD, and editor-frame alignment. Record actual device results separately; new packed candidates initially report `hardware_validated: false`.

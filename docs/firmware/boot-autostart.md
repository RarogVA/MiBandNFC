# Boot-time autostart research — miwear.watch.p67glt (Xiaomi Smart Band 10 Pro Ceramic, Global)

Firmware analysed: **3.201.026** (`ro.build.customer_version=CONBINE_LTALM080_T3.201.026_09041709`)
Image: `references/vela_ap.bin` (10,339,356 bytes), raw, `VA = file_offset + 0x2c0c0000`, also aliased at `0x0c0c0000`.
The alias is real and used: the NSH command table stores handlers in the `0x0c…` alias
(e.g. `0x0c1ff7f5` → function `0x2c1ff7f4`).

Scratch tooling used for this report (read-only, under `build/boot-research/`):
`annotate.py` (capstone disassembler that resolves PC-relative literal pools and prints the
string/pointer a literal points to) and `xref.py` (Thumb-2 `BL`/`BLX` xref + raw pointer scan).
Disassemble: `build/shortcut-research-venv/bin/python build/boot-research/annotate.py 0x2c1fb3fc 40`

Everything below is marked **[confirmed]** (bytes/strings at the stated address) or
**[inferred]** (a conclusion that follows from confirmed facts but is not directly proven by
the image).

---

## 0. Executive answers

| # | Question | Answer | Confidence |
|---|----------|--------|-----------|
| 1 | Where do `/etc/init.d/rc.sysinit` / `rcS` come from, and are they writable? | They live inside a **ROMFS image embedded in the firmware**, mounted **read-only** as `/etc` from a RAM disk (`/dev/ram0`) by `nx_bringup` before the init task starts. They cannot be edited at runtime and are re-created from flash on every boot. | confirmed |
| 2 | Is there a user-writable autostart path? | **No.** Only `/etc/init.d/rc.sysinit` + `/etc/init.d/rcS` are executed at boot (both read-only). No `/data/init.d`, no `rc.local`, no `/etc` overlay, no writable init script. The only user-controlled code executed at boot is the restored watchface / its Lua script. | confirmed for scripts; inferred for Lua |
| 3 | Is the active watchface restored at boot, and is its Lua evaluated? | `/data/app/watchface/watchface.db` holds face state; the watchface app creates the active face on startup (`watchface_create`, `cur_watchface`). The Lua runtime is a *fresh `lua_State` per app/script creation*, running the **whole chunk** via `pcall`. If the active face is a Lua-backed app/face, its top-level code runs at boot — this needs one device test to prove. | restore confirmed; Lua-at-boot inferred |
| 4 | Is `shutdown --reboot` real, and callable from a native module? | **Yes.** NSH command table entry `{"shutdown", 0x2c1ff7f4, …, "[--reboot]"}` → `boardctl(0xff04, 0)` → `board_reset(0)` → SoC reset via ROM. A module can call the firmware `system()` at `0x2c2025d4` (thumb `0x2c2025d5`) or call `boardctl` directly; there are no uid/permission checks in this build. | confirmed |
| 5 | Can `insmod` run early, and does `/data` persist? | `insmod` needs a mounted filesystem for the `.o/.elf`, i.e. it can only run **after `/data` is mounted by `rc.sysinit`** (which is still before the UI: `rcS` starts `miwear` later). `/data` is **yaffs2 on NAND** (`/dev/nand_data`, `autoformat`) → persists across reboot. `/etc` and `/resource` are read-only ROMFS; `/tmp` is tmpfs. | confirmed |

Bottom line: **you cannot hook the init scripts** (read-only firmware ROMFS). The realistic
autostart lever is the watchface/Lua path (option A), with the module's own reboot API and the
boot-start app registry as secondary leads.

---

## 1. Image layout and the embedded `/etc` ROMFS

### 1.1 Where `/etc` comes from

Strings are grouped in one translation unit `../../nuttx/sched/init/nx_bringup.c`
(`0x2c849424`), next to `nsh_main` (`0x2c849418`), `/etc` (`0x2c8493d4`), `/dev/ram0`
(`0x2c8493dc`), `romfs` (`0x2c8443f4`) and the error strings:

```
0x2c8493ac  "%s: ERROR: romdisk_register failed: %d\n"
0x2c8493e8  "%s: ERROR: nx_mount(%s,%s,romfs) failed: %d\n"
0x2c99a510  "nx_romfsetc"            <- function name passed to the log macro
```

Function `nx_romfsetc` at **`0x2c1c9d90`** (annotated disassembly):

```
2c1c9d96  ldr  r2, =0x2c99a618        ; &{u32 size; u8 image[]}
2c1c9d9a  ldr  r2, [r2]               ; r2 = 0x00005c00 (23552) = sector-aligned size
2c1c9d9c  movs r3, #0x40              ; sectsize = 64
2c1c9d9e  adds r2, #0x3f
2c1c9da2  movs r0, #0                  ; minor = 0  -> /dev/ram0
2c1c9da4  ldr  r1, =0x2c99a61c        ; buffer = ROMFS image (inside the firmware image!)
2c1c9da6  lsrs r2, r2, #6              ; nsectors = (0x5c00+63)/64 = 368
2c1c9daa  bl   0x2c173284             ; romdisk_register(0, image, 368, 64)
2c1c9db2  ldr  r5, =0x2c8493d4        ; "/etc"
2c1c9db6  movs r3, #1                 ; mount flags = MS_RDONLY (1)
2c1c9db8  mov  r1, r5                 ; target  = "/etc"
2c1c9dba  ldr  r2, =0x2c8443f4        ; fstype  = "romfs"
2c1c9dbc  ldr  r0, =0x2c8493dc        ; source  = "/dev/ram0"
2c1c9dbe  bl   0x2c195084             ; nx_mount(source,target,fstype,flags,data=NULL)
2c1c9dc6  bl   0x2c1d1544             ; unidentified bring-up call (between mount and spawn)
2c1c9dd2  movs r3, #0x64              ; spawn priority = 100
2c1c9dd4  mov.w r2, #0x1000           ; stack = 4096
2c1c9ddc  ldr  r1, =0x0c620bdd        ; entry = nsh_main (thumb) = 0x2c620bdc
2c1c9de4  mov  r2, r4                 ; file_actions = NULL
2c1c9de6  ldr  r0, =0x2c849418        ; task name "nsh_main"
2c1c9de8  bl   0x2c1d0120             ; task_spawn("nsh_main", nsh_main, ..., prio 100, 4 KB)
```

So: the kernel brings up a **RAM disk whose backing buffer is the firmware image itself**, mounts
it **`romfs` with `MS_RDONLY`** at `/etc`, and then starts `nsh_main` as the init task.
The `0x2c195084` callee is the NuttX mount entry (`nx_mount`/`mount`): it validates args and looks the
fstype up in a table starting with `"tmpfs"`.

### 1.2 The ROMFS image itself (confirmed by the `-rom1fs-` magic)

```
file offset 0x8da61c  -> VA 0x2c99a61c : "-rom1fs-"
file offset 0x8da618  -> VA 0x2c99a618 : 00 00 5c 00  (size 0x5c00 used for the ramdisk)
header fullsize = 0x5b90, volume name = "etc"
image end = 0x8da61c + 0x5b90 = 0x8e01ac (VA 0x2c9a01ac)
```

Files inside the `/etc` ROMFS (walked entry by entry; alignment relative to image start):

| path | size | notes |
|---|---|---|
| `md5test.txt` | 0x7e0 | `0123456789abc…Z` repeated — looks like a build self-test artefact |
| `init.d/rc.sysinit` | 0x193 | see §2.1 |
| `init.d/rcS` | 0x121 | see §2.2 |
| `build.prop` | 0xdb | `ro.build.version=3.201.026`, `ro.sf.lcd_density=336`, … |
| `font_config.json` | ~0x49xx | emoji/font config |
| `miwear_product.json` | ~0x2xx | product table; contains **`"miwear_model": "miwear.watch.p67glt"`, `"board_id": 24`, `"miwear_pid": "29600"`, `"device_model": "M2559B1"`** — provenance check for this exact device |

This is the *only* `/etc` in the boot path. There is no second mount of `/etc`, no unionfs
overlay of `/etc` (the unionfs driver exists in the image, but no mount call references it),
and nothing re-mounts it read-write. The ROMFS driver has no write path, and the mount is
`MS_RDONLY`, so a write to `/etc/init.d/rcS` fails with `EROFS` (`EROFS` string is present at
`0x2c85ac8c`), and any change would in any case be discarded at the next boot, because the
volume is rebuilt from the flash image.

**Consequence: editing/adding init scripts on the device is not an option.**

---

## 2. Startup order

### 2.1 `rc.sysinit` — exact content (confirmed bytes at VA `0x2c99aefc`, len 0x193)

```
set +e
       mount -t procfs "/proc"
mount -t tmpfs /tmp
pmconfig stay normal
echo "mount /resource"
mount -t romfs /dev/resource /resource
echo "mount /data"
mount -t yaffs -o autoformat /dev/nand_data /data
echo "mount /nv"
mount -t littlefs -o autoformat /dev/nv /misc
echo "mount /mode"
mount -t littlefs -o autoformat /dev/mode /mode
offline_log start -w &
kvdbd &
system_trace
miwear_rtc_checker
```

(Yes, `system_trace` and `miwear_rtc_checker` are foreground lines with no `&`; the shell runs
them sequentially. `set +e` is the first line, so a failing mount does not abort the script.)

### 2.2 `rcS` — exact content (confirmed bytes at VA `0x2c99b0bc`, len 0x121)

```
set +e
set -e
if [ ! -d /data/nobusiness ];then
miwear_product_load
bluetoothd &
miwear_bluetooth &
usleep 1
miwear_algo_service &
miwear_activity_service &
miwear_capture_service &
nfc_stack_bridge &
miconnect > /dev/log &
miwear &
charger_manage &
vibratord &
healthd &
fi
at_cmd &
exit
```

`/data/nobusiness` only *disables* the service block (factory/no-business mode); it is not a
hook. `set -e` after `set +e` means later failures abort rcS.

### 2.3 Who runs them, in what order

`nsh_main` (`0x2c620bdc`) → `nsh_initialize` (`0x2c1f9b18`) → `nsh_consolemain`
(`0x2c2010d4`). `nsh_initialize` is:

```
2c1f9b1c  bl 0x2c1ff748
2c1f9b20  bl 0x2c1ff798
2c1f9b24  bl 0x2c202334
2c1f9b28  ldr r0, =0x2c9b6e58 ; some ident/data pointer
2c1f9b2a  bl 0x2c20234c
2c1f9b30  movw r0, #0xff01
2c1f9b34  bl 0x2c1d126c              ; boardctl(0xff01) — boot-stage marker
2c1f9b38  movs r0, #0
2c1f9b3a  bl 0x2c1fb1cc              ; allocate NSH console/pstate
2c1f9b40  bl 0x2c1fb3fc              ; >>> nsh_sysinit() : /etc/init.d/rc.sysinit
2c1f9b44  bl 0x2c1f988c              ; no-op stub (returns 0)
2c1f9b4a  movw r0, #0xff02
2c1f9b4e  bl 0x2c1d126c              ; boardctl(0xff02) — boot-stage marker
2c1f9b54  bl 0x2c1fb414              ; >>> nsh_rcs() : /etc/init.d/rcS (runs once)
2c1f9b5a  ldr r3, [r4, #8] ; blx r3  ; console cleanup
```

* `nsh_sysinit` = `0x2c1fb3fc`:
  `nsh_script(pstate, "sysinit", "/etc/init.d/rc.sysinit", 0)` — literals
  `0x2c84a69c "sysinit"` (at `0x13b410`) and `0x2c84a684 "/etc/init.d/rc.sysinit"` (at `0x13b40c`).
* `nsh_rcs` = `0x2c1fb414`: guarded by a static bool in RAM at `0x200ca3b4` so it runs **once per
  boot**; then `nsh_script(pstate, "init", "/etc/init.d/rcS", 1)` —
  literals `0x2c90bc14 "init"` and `0x2c84a6a4 "/etc/init.d/rcS"`.
* `nsh_script` = `0x2c1fb250` (opens the path, `fopen`/`read`, tokenises, executes commands).

Task chain: `nx_start → nx_bringup → nx_romfsetc (mount /etc) → task_spawn("nsh_main") →
nsh_initialize → rc.sysinit → rcS → nsh_consolemain`.

### 2.4 Startup diagram

```
power-on / SoC reset
        │
        ▼
NuttX kernel init (nx_start)
        │
        ▼
nx_bringup ── nx_romfsetc()  [0x2c1c9d90]
        │        ├─ romdisk_register(minor 0, image @0x2c99a61c, 368×64 B)
        │        ├─ nx_mount("/dev/ram0", "/etc", "romfs", MS_RDONLY)   ← /etc ready, RO
        │        └─ task_spawn("nsh_main", 0x2c620bdc, prio 100, stack 4 KB)
        ▼
nsh_main [0x2c620bdc] → nsh_initialize [0x2c1f9b18]
        │   boardctl(0xff01)
        │   allocate NSH pstate
        ├─ nsh_sysinit → nsh_script(/etc/init.d/rc.sysinit)     ← FIRST SCRIPT, before any UI
        │      mount procfs /proc
        │      mount tmpfs  /tmp
        │      pmconfig stay normal
        │      mount romfs  /dev/resource /resource      (RO)
        │      mount yaffs  /dev/nand_data /data  autoformat  ← PERSISTENT USER DATA, HERE
        │      mount littlefs /dev/nv  /misc   autoformat     (persistent)
        │      mount littlefs /dev/mode /mode  autoformat     (persistent)
        │      offline_log start -w & ; kvdbd & ; system_trace ; miwear_rtc_checker
        │   boardctl(0xff02)
        ├─ nsh_rcs (once) → nsh_script(/etc/init.d/rcS)         ← UI/services start here
        │      miwear_product_load
        │      bluetoothd, miwear_bluetooth, usleep 1,
        │      miwear_algo_service, miwear_activity_service, miwear_capture_service,
        │      nfc_stack_bridge, miconnect, **miwear**, charger_manage, vibratord, healthd
        │      at_cmd
        ▼
nsh_consolemain  (NSH session; the product UI is the miwear process)
        │
        ▼
miwear starts the desktop/watchface app and restores the active face from /data
```

---

## 3. Q2 — is there a user-writable autostart path?

Searched the whole image for script/startup literals: `/etc/init.d`, `rc.local`, `init.d`,
`/data/init`, `autostart`, `startup`, `unionfs`, `overlay`.

* **Confirmed:** the only two boot scripts are the two ROMFS files above. There is no
  `rc.local`, no `/data/init.d`, no writable `/etc` overlay, and no code that copies `/etc`
  to a writable place.
* **Confirmed:** `nx_mount` is called exactly twice with `romfs` (`/etc` and `/resource`);
  `/data`, `/misc`, `/mode` are the only writable persistent mounts, and all three are
  *data* filesystems — nothing in the image scans them for scripts at boot.
* **Confirmed:** the app framework has a *static* app registry with a boot flag. Strings near
  the framework: `"[%s] add name=%s,addr=%p,boot_start:%d"` (`0x2c887ca0`),
  `"[%s] init applet name=%s,addr=%p,boot_start:%d"` (`0x2c887cd8`),
  `"[%s] init app name=%s,addr=%p,boot_start:%d"` (`0x2c88e84c`). Registration is a linked-list
  insert in kernel/rodata code, with `addr` = a compiled-in function pointer.
  **[inferred]** this is a compile-time registry (`boot_start` is a property of built-in
  applets), not a `/data`-driven autostart list — I found no code that reads an "autostart"
  list/config from `/data`. This is the one remaining lead worth a follow-up (see §8).
* `rcS` starts **miwear** as one process; everything user-visible (watchface, apps) is inside
  it. So the only user-controlled code that runs at boot is whatever miwear restores.

---

## 4. Q3 — watchface restore and Lua evaluation at boot

### 4.1 Watchface restore (confirmed)

* `/data/app/watchface/watchface.db` — `0x2c8d7b98` (4 literal-pool references at `0x2c564044`,
  `0x2c564058`, `0x2c564074`; the surrounding code uses the fs API on it).
* `/data/app/watchface/` — `0x2c89ae30` (referenced from the watchface app's pool at `0x2c4439e8`),
  next to `/resource/watchface/` (`0x2c89ae48`) and `miwear_init_appdata` (`0x2c9ef6a0`).
* Watchface app lifecycle strings (literal pool `0x2c556ed0`):
  `refresh_cur_watchface` (`0x2ca01dd0`), `"watchface"`, `"[%s] %s: watchface_create start/end"`,
  `"[%s] %s: create watchface(%s)"`, `"watchface_destroy start/end"`, `"cur_watchface"`,
  `"do not resume watchface in screen off state."`, `"watchface is active page by deco, need to updata"`.
* Unqlite is the store: `"set watchface album edit flag to unqlite failed"`,
  `"watchface_not_first_edit"`. Rooted in `/data/app/watchface/`.

**[inferred]** on boot, miwear's desktop/watchface app reads the persisted `cur_watchface` /
`watchface.db`, creates the active face (`watchface_create…`) without user interaction — which is
why the band shows the same watchface after a reboot.

### 4.2 Lua runtime (confirmed)

* Lua **5.4.0** is compiled in: `"Lua 5.4.0  Copyright (C) 1994-2020 Lua.org, PUC-Rio"` (`0x2c90275c`),
  `lua_State`, `lua_pushfstring`, `lua_panic`, `luaopen_*`, `?.lua` package pattern (`0x2c995a7c`).
* miwear Lua binding modules: `miwear` (`luaopen_miwear`), `screen`, `activity`, `dataman`,
  `navigator`, `topic` (`lua_topic.c` at `0x2c99592c`), `vibrator`, plus
  `lua_module_miwear_close`, `lua_module_screen_close`.
* **`miwear_lua_run` at `0x2c7f2c78`** — the one and only script runner:
  ```
  2c7f2c8c  log "6 / lua / miwear_lua_run / run script: %s"   (source-log: 6, tag "lua")
  2c7f2c9c  bl 0x2c5ff4bc            ; new lua_State (fresh per run)
  2c7f2ca6  malloc(0x70)             ; per-run context
  2c7f2cbe  bl 0x2c5fc57c            ; push the miwear C module (0x0c7f29e1)
  2c7f2ccc  bl 0x2c5fc4e8            ; push/load with the script name
  2c7f2cd4  bl 0x2c5fc624            ; push arg 1
  2c7f2cdc  bl 0x2c5fc624            ; push arg 2
  2c7f2cec  bl 0x2c5fd2c8            ; lua_pcall(3 args, 1 result) — whole chunk evaluated
  2c7f2d1c  on error: "mlua / %s: %s"
  ```
  There is no bytecode VM instantiated at boot for the face in a background sense — the *chunk*
  is executed when the script is run; a fresh `lua_State` is created for that run.
* Call sites of `miwear_lua_run`: only `0x2c7c91f8`, `0x2c7c9384` (app/applet creation) and
  `0x2c7d0d1c`. The loader that feeds it (`0x2c7c9f6c`) builds `"<dir>/<entry>"` and runs it only
  when the entry's extension `strcmp` equals `"lua"` (`0x2c89ebd4`):
  ```
  2c7c9f9e  path = dir
  2c7c9fb0  strcat(path, "/")
  2c7c9fc0  strcat(path, app->entry_name)
  2c7c9fc8  extension = ... ; 2c7c9fce: strcmp(ext, "lua")
  2c7c9fd6  bne  -> not a Lua app
  2c7c9fe0.. 2c7ca056: miwear_lua_run(ctx, path, args)
  ```
* **[inferred]** If the active watchface is such a Lua-backed app/face (which matches the
  observed `os.execute("insmod …")` behaviour on tap), then the same chunk is evaluated when the
  face is created after boot → **top-level statements run at boot, before any tap**. That is
  exactly the autostart hook we want, but it must be verified on device (§7).

### 4.3 Does `os.execute` exist in the sandbox? (inferred)

The user already runs `os.execute("insmod …")` from the face script on tap, so the `os` library
is present and `os.execute` reaches the firmware `system()` (`0x2c2025d4`, §5.3) — the same path
used below. No additional sandbox restriction was found in the `miwear` Lua bindings.

---

## 5. Q4 — `shutdown --reboot`, `system()`, and reboot internals

### 5.1 Command table entry (confirmed)

The NSH command map is an array of 16-byte records `{char *cmd; int (*func)(…); u32 maxargs|flags;
char *usage}` (the 4th word is e.g. `0x00000201` for shutdown = maxargs 1 / flag 2). Relevant
records (dumped around `0x2c9b7000` … `0x2c9b7500`):

```
0x2c9b73c4  "reboot"    0x0c1ffa0d  0x00000201  usage NULL
0x2c9b7364  "poweroff"  0x0c1ff9b9  0x00000201  usage NULL
0x2c9b7424  "shutdown"  0x0c1ff7f5  0x00000201  usage 0x2c84af18 "[--reboot]"
0x2c9b7254  "insmod"    0x0c200e91  0x00000303  usage 0x2c84ac00 "<file-path> <module-name>"
0x2c9b72b4  "lsmod"     0x0c200f29  0x00000101  usage NULL
0x2c9b7404  "rmmod"     0x0c200ed1  0x00000202  usage 0x2c84ac0c "<module-name>"
0x2c9b7304  "mount"     0x0c200bed  0x00000701  usage 0x2c84ad5c "[-t <fstype> …]"
0x2c9b74c4  "umount"    0x0c200ddd  0x00000302  usage 0x2c84afe8 "[-f] <dir-path>"
0x2c9b7444  "source"    0x0c1fcec1  0x00000202  usage 0x2c84a860 "<script-path>"
```

So `shutdown` is a real command, takes exactly one option, and that option must be `--reboot`.

### 5.2 Handler → boardctl → reset (confirmed)

`shutdown` handler **`0x2c1ff7f4`**:

```
2c1ff7f6  cmp  r1, #2                 ; argc must be 2
2c1ff800  bne  0x2c1ff84a             ; usage / invalid
2c1ff802  ldr  r1, =0x2c84b6a8 "--reboot"   ; literals at 0x2c1ff858/0x2c1ff860
2c1ff804  ldr  r0, [r2, #4]           ; argv[1]
2c1ff806  bl   0x2c558468             ; strcmp
2c1ff80a  cbz  r0, 0x2c1ff820
2c1ff80c  … vtbl->printf("nsh: %s: argument invalid\n") ; return -1
2c1ff820  mov  r1, r0                 ; arg2 = 0  (reset reason 0)
2c1ff822  movw r0, #0xff04
2c1ff826  bl   0x2c1d126c             ; boardctl(0xff04, 0)
2c1ff82a  … log "nsh: %s: %s failed: %d\n" with "boardctl" and errno
```

`boardctl` **`0x2c1d126c`** validates `cmd - 0xff01 <= 0x14` and switches (tbb at `0x2c1d127e`):
`0xff01→0x2c1d12bc`, `0xff02→0x2c1d12c4`, `0xff03→0x2c1d12cc`, **`0xff04→0x2c1d12da`**,
`0xff05→0x2c1d12e8`, … The `0xff04` case:

```
2c1d12da  movs r0, #1 ; bl 0x2c1ca628     ; power-state hook
2c1d12e0  mov  r0, r4                      ; r4 = arg (reason)
2c1d12e2  bl   0x2c16d9b8                 ; board_reset(reason)
```

`board_reset` **`0x2c16d9b8`**: formats and logs `"Version: %s %s %s %s %s\n"` (`0x2c849508`) and
`"Time: %s\n"` (`0x2c841b4c`) — a reboot log — then flushes, then switches on the reason
(`cmp r4,#6; tbb`), e.g.:

```
reason 0   -> 0x2c1f727c   log "system_reset" ; write 0x10000 to 0x40080000+0x38 ; -> 0x2c557ff0
reason 1,3 -> 0x2c16da66   write 0x02210000  ; -> 0x2c557ff0   (bootloader-ish)
reason 2   -> 0x2c1f72d0   write 0x01010000  ; -> 0x2c557ff0
default    -> 0x2c1f72b4   write 0x10810000  ; -> 0x2c557ff0
```

`0x2c557ff0` is a veneer (`ldr.w pc,[pc,#0]` → `0x0027f884`) into the SoC boot ROM, i.e. a real
cold reset (the helper `0x2c0c200c` sets bits at `0x40080000+0x38` before jumping).

**[inferred]** `shutdown --reboot` therefore does a full SoC reset and is not a no-op/stub.

### 5.3 `system()` at `0x2c2025d4` (confirmed)

`0x2c2025d4` is the NuttX `system()` from `apps/system/system/system.c` — confirmed by the
`NULL`-argument diagnostic at `0x2c20266e` which loads `0x2c84bbf8
"../../apps/system/system/system.c"` and line `0x52`, and by the spawn sequence:

```
2c2025d4  push {r4,r5,r7,lr} ; sub sp,#0x38
2c2025e0  spawn-attr init                                ; 0x2c1b8cd4
2c2025f8  attr: set field = 100 (priority)               ; 0x2c1b8d50
2c20260a  attr: set flags 0x2000                         ; 0x2c1b8d74
2c202618  attr: set field = 4                            ; 0x2c1b8d30
2c202620  r2 = "-c" (0x2c84a5f0)
2c20262e  argv = { "-c", cmd, NULL }
2c202636  r1 = 0x0c1fb485                                ; entry = system_main (thumb)
2c202638  r0 = "system" (0x2c8bc08c)                     ; task name
2c20263a  bl  0x2c1d0120                                 ; task_spawn("system", system_main, …)
2c20264c  bl  0x2c1ccf24                                 ; waitpid(pid) — blocks until done
2c202664  return status
```

`system_main` **`0x2c1fb484`** → `0x2c1fb450` → allocates an NSH pstate (`0x2c1fb1cc`, 0x5b8 B) and
runs the command string through nshlib (`nsh_system` in `nsh_system.c`; its
`nsh_script` call is at `0x2c2000e8`, and the `nsh_system.c` source string is `0x2c84a6b4`).

**From a native module:** you can either
* call `system("shutdown --reboot")` at `0x2c2025d5` (easiest; goes through the command table and
  also logs the error path), or
* call `boardctl(0xff04, 0)` / `board_reset(0)` directly (`0x2c1d126d` / `0x2c16d9b9`),
* or call `nsh_script`/`nsh_rcs` directly — **do not** call `nsh_rcs` to "restart the boot scripts";
  it is guarded so it will not re-run, and re-running rc.sysinit would re-mount `/data` and start a
  second copy of every daemon.

**Permissions:** none. This is a monolithic NuttX build with no user/uid checks in the path
(no `EPERM` check before `boardctl`; `mount`/`boardctl` are ordinary syscalls). The module runs in
kernel context (loaded by `insmod` → `insmod()` at `0x2c1ca664`), so it may call these directly.
Practical constraints: `system()` needs task-spawn + a 4 KB-stack child; the module init runs in the
`insmod` command's task, so that is satisfied.

---

## 6. Q5 — `insmod` timing, mount order, persistence

* `insmod` entry `0x2c9b7254`, handler `0x2c200e90`: it takes `argv[1]` (file path) and `argv[2]`
  (module name) and calls the module-load routine `0x2c1ca664(path, name)`; on failure it logs
  `"nsh: %s: %s failed: %d"` with the `"insmod"` string. `lsmod`/`rmmod` and `/proc/modules` (`0x2c84ba78`) confirm the module
  framework is present.
* `insmod` obviously requires the module file to exist on a mounted filesystem. The module lives in
  `/data/tag_catalog/...`, so the earliest possible point is **after** the
  `mount -t yaffs … /dev/nand_data /data` line in `/etc/init.d/rc.sysinit`.
  That is still early: `rc.sysinit` runs before `rcS`, and `rcS` is what starts `miwear` (the UI).
* There is no earlier writable storage: `/` is the XIP firmware image, `/etc` is RO ROMFS, and
  `/resource` is RO ROMFS. `/tmp` is tmpfs (created in `rc.sysinit`, wiped on reboot).
* Mount order and persistence:

| mount | source | type | flags | persists? |
|---|---|---|---|---|
| `/proc` | procfs | procfs | – | n/a |
| `/tmp` | tmpfs | tmpfs | – | no (RAM) |
| `/resource` | `/dev/resource` | romfs | – | ro, from flash |
| **`/data`** | `/dev/nand_data` | **yaffs** | `-o autoformat` | **yes** (NAND) |
| `/misc` | `/dev/nv` | littlefs | `-o autoformat` | yes (NAND/NV) |
| `/mode` | `/dev/mode` | littlefs | `-o autoformat` | yes |
| `/etc` | `/dev/ram0` (embedded image) | romfs | `MS_RDONLY` | **no** — rebuilt from firmware each boot |

* `autoformat` is a real risk factor: if a yaffs/littlefs mount fails, the partition is **formatted**
  (that is the documented NuttX `-o autoformat` behaviour and the option is right there in the
  script). Do not experiment with `/data` mount options.
* Verification aid that exists in the image: procfs exposes the mount table at
  **`/proc/fs/mount`** (plus `/proc/fs/usage`, `/proc/fs/blocks`, strings `0x2c84ba54`,
  `0x2c84ba34`, `0x2c84ba44`). `cat /proc/fs/mount` from a shell/module confirms every mount above.

---

## 7. Options for automatic module activation after reboot, ranked by risk

### A. Lua watchface chunk at face creation (recommended first try)

Move the `os.execute("insmod …")` call from the tap handler to the **top level of the Lua chunk**
(or to whatever load/enter callback the face framework calls), so it runs whenever the chunk is
evaluated. If the face is restored at boot, the chunk is evaluated then → module is loaded before
the user can tap, and it survives every reboot as long as `/data` survives.

* Risk: **low** (no firmware change; the Lua script and face live in `/data`, replaceable/removable
  from the phone app or by reinstalling the face).
* Failure mode: if the Lua VM/script is only created on face *switch* and not on boot restore, the
  call happens at the first face switch instead of at boot — annoying but not harmful.
* Test: §8 step 2/3.

### B. Lua lifecycle callback (onCreate / onResume / "face shown")

If the framework exposes a lifecycle event that fires on boot (strings show a watchface
`create`/`resume`/`destroy` cycle and a `do not resume watchface in screen off state` guard), hook
that instead of relying on chunk top-level order. Same risk profile as A, possibly more reliable
if the chunk is cached.

### C. Boot-start app registry (`boot_start`)

The app framework logs `add name=%s,addr=%p,boot_start:%d`. If it turns out that an **installed**
app (from `/data/app/...`) can be flagged `boot_start` (e.g. through `install_info_v1.db` /
`system.db` in `/data/app/system/`), an installed Lua app would run at boot independently of the
watchface. **[inferred / unproven]** — this is the main open lead; a Lua quick-app installed
through the normal channel plus `boot_start=1` would be a clean, low-risk autostart.

### D. Module self-reboot / watchdog tricks

`system("shutdown --reboot")` exists but does not help autostart; it only loses the module again.
A module that reboots the band at boot would create a boot loop. Avoid.

### E. Patch the `/etc` ROMFS (do not do)

The ROMFS image is inside the AP firmware image that is presumably verified/anti-rollback
protected, and `/data` is yaffs with `autoformat`. Editing the image means reflashing firmware
with a modified romfs; a checksum/signature mismatch can brick the band, and a bad `/data` mount
would reformat user data. **Not recommended.**

### F. Persist via `/misc` or `/mode`

They are persistent littlefs partitions but hold configuration/calibration data; no code path
executes scripts from them. Not an autostart mechanism.

**Ranking: A (or B) ≫ C ≫ D > E.**

---

## 8. Concrete device test plan (no brick risk)

Preconditions: keep the original face/script and the original module file backed up off-device;
do all Lua changes to a **copy** of the face that you can delete from the phone app. Never put an
unconditional `shutdown --reboot` in a boot-time path.

1. **Baseline / mount table.** From the currently-working tap path, run
   `cat /proc/fs/mount > /data/boot-research/mounts.txt` (via `os.execute` from the face or via the
   module) and pull it. Confirm `/etc` is `romfs` with `ro` and `/data` is `yaffs`.
2. **Prove `/etc` is read-only.** From the module or Lua, attempt
   `open("/etc/init.d/rcS", O_WRONLY)` and log `errno` (expect `EROFS`), and `cp /etc/init.d/rcS
   /data/boot-research/rcS.copy` and diff. This closes Q1 on real hardware without writing to flash.
3. **Prove whether the Lua chunk runs at boot.** In the face's Lua, at top level, do
   `os.execute("mkdir -p /data/boot-research; echo boot >> /data/boot-research/lua_boot_marker")`
   (or just `touch`). Reboot **without touching the screen**, wait for the face to appear, then pull
   `/data/boot-research/lua_boot_marker`. If it exists with a boot-time timestamp → chunk is
   evaluated at boot → option A works. If not, try a lifecycle callback (option B) and repeat.
4. **Move the `insmod` to the top level** (guarded: only if `/data/tag_catalog/enable` exists).
   Reboot untouched, then verify `/proc/modules` contains the module and that its own log/marker
   appeared. If it fails, capture the module's error output to `/data/boot-research/insmod.log`.
5. **Guard and recovery.** The guard file (`/data/tag_catalog/enable`) is the kill switch: if the
   module (or its Lua) ever misbehaves at boot, delete that file (via the phone file channel or by
   deleting/repairing the face from the phone app) and reboot. Do **not** test the reboot API until
   3–4 pass with the guard in place, and only then with a single manual invocation from a
   non-boot path.
6. **Only if needed, `insmod` earlier than the watchface.** If the module must be loaded before the
   UI, the only lever is an existing process that starts in `rcS` before `miwear`; none of them
   reads a script from `/data`, so this would require firmware changes — treat as out of scope.

---

## 9. Confirmed vs inferred, and open questions

**Confirmed by bytes/strings:**
* `/etc` = ROMFS embedded at file `0x8da61c` (VA `0x2c99a61c`), mounted `MS_RDONLY` from
  `/dev/ram0` by `nx_romfsetc` (`0x2c1c9d90`), before `nsh_main` (`0x2c620bdc`) is spawned.
* `rc.sysinit` then `rcS` execution order, exact script bodies, one-shot guard for `rcS`.
* `/data` = yaffs on `/dev/nand_data`, mounted in `rc.sysinit` before `rcS`/`miwear`; persistent.
* `shutdown` ∈ NSH command table (`0x2c9b7424`), only accepts `--reboot`, calls
  `boardctl(0xff04,0)` → `board_reset` → real SoC reset via ROM `0x0027f884`.
* `system()` at `0x2c2025d4` is `apps/system/system/system.c`, spawns the `"system"` task with
  `{"-c", cmd}` and runs the command through nshlib.
* `insmod`/`lsmod`/`rmmod` + `/proc/modules` exist; `miwear_lua_run` creates a fresh Lua 5.4 state,
  registers `miwear`/`screen`/`activity`/`dataman`/`navigator`/`topic`/`vibrator`, and `pcall`s the
  whole script file.

**Inferred / needs one device test:**
* Whether the active watchface's Lua chunk is evaluated during the boot-time face restore (test 3).
* Whether the `boot_start` app registry can be driven from `/data` (installed-app autostart).
* Whether `os.execute` is available in every face context (it is in the user's current face).

**Open leads worth a follow-up if option A fails:**
* `boot_start` registry (`0x2c887ca0`, `0x2c887cd8`, `0x2c88e84c`) and the install DBs
  (`/data/app/system/install_info_v1.db`, `system.db`) — is any of it user-influenceable?
* miwear's app/face install path (phone-pushed `.face`) — can a face declare a Lua entry that is
  executed at boot independent of the watchface UI?
* `nsh_consolemain` behaviour on this product: it is spawned as the init task, but there is no
  evidence the product exposes a UART console; if it does, a `source /data/…` hook could be
  relevant (not recommended: physical access required).

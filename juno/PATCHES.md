# Provenance: branch `rebase-4.22.3` vs upstream tuxedo-drivers v4.24.0

Documents every difference between the `rebase-4.22.3` branch tree and the
upstream tuxedo-drivers tree it derives from. The branch base is now upstream
v4.24.0 (clevo_acpi.c is a platform_driver since v4.24.0), carrying the fork's
renames, gate-disable, payload layout and FlexiCharger patches unchanged in
semantics. The previously vendored 4.6.2 payload is covered by the pre-rebase
audit at the bottom.

## BASE-REF

`e0e0b8a21252b796aa312b53d00dc0e8a3fd738b` — commit tagged `v4.24.0`
(annotated tag object 03537625ab4357adcd96e4b59559a45c5d1c1402), upstream
https://github.com/tuxedocomputers/tuxedo-drivers.git, dated 2026-09-23.

## Differences from BASE-REF

### 1. Module-set rename tuxedo-keyboard/tuxedo-drivers -> clevo-keyboard (carried)

- `src/tuxedo_keyboard.c` -> `src/clevo_keyboard.c`;
  `src/tuxedo_keyboard_common.h` -> `src/clevo_keyboard_common.h`;
  `src/tuxedo_compatibility_check/` -> `src/clevo_compatibility_check/`
  (both member files renamed).
- String renames inside sources: `DRIVER_NAME "tuxedo_keyboard"` ->
  `"clevo_keyboard"` (clevo_keyboard_common.h); pr_fmt `"tuxedo_keyboard"` ->
  `"clevo_keyboard"` (clevo_keyboard.c, clevo_wmi.c; clevo_acpi.c uses
  `KBUILD_MODNAME` since the v4.24.0 platform_driver refactor);
  input-device name `"TUXEDO Keyboard"` -> `"CLEVO Keyboard"`
  (clevo_keyboard.c); `.name = "TUXEDO Flexicharger Extension"` ->
  `"CLEVO ..."` (clevo_keyboard.h); `#include` paths adjusted accordingly.
- One trailing blank line at EOF of src/clevo_keyboard.h and
  src/uniwill_keyboard.h (inherited payload formatting).

### 2. Makefile builds only the Clevo trio (carried)

Top-level Makefile adds `obj-m := src/clevo_keyboard.o src/clevo_wmi.o
src/clevo_acpi.o`. Upstream has no obj-m there; its module list lives in
src/Kbuild.

### 3. Payload layout (carried): no Kbuild wiring

Neither the upstream top-level `Kbuild`, `src/Kbuild` nor
`tuxedo_compatibility_check/Kbuild` is vendored, and the renamed
`src/clevo_compatibility_check/` ships no Kbuild. Upstream drivers not built
by the fork are vendored for tree completeness: `src/gxtp7380/`,
`src/stk8321/`, `src/tuxedo_tuxi/` and the uniwill/ite/tuxedo_nb04/nb05
units.

### 4. Gate-disable + compat-check vendor strings (carried, juno-only)

- src/clevo_keyboard.c: the DMI compatibility gate is commented out
  (`// if (!tuxedo_is_compatible()) return -ENODEV;`, v4.24.0 site
  clevo_keyboard.c:202). The driver probes on any vendor, not just TUXEDO.
- src/clevo_compatibility_check/clevo_compatibility_check.c: three
  `DMI_MATCH` vendor strings "TUXEDO" -> "Notebook" (DMI_SYS_VENDOR,
  DMI_BOARD_VENDOR, DMI_CHASSIS_VENDOR). Dead code: the module is not built.

### 5. dkms.conf is fork-side packaging (retained, PACKAGE_VERSION 4.24.0+diamon1)

No upstream counterpart (upstream generates `src/dkms.conf` at install
time). The fork keeps it as its only packaging: `PACKAGE_NAME=clevo-keyboard`,
`PACKAGE_VERSION=4.24.0+diamon1`, three modules (clevo_keyboard, clevo_wmi,
clevo_acpi). DKMS must skip kernels older than 6.6: v4.24.0's clevo_acpi.c uses
`acpi_dev_install_notify_handler()` and `acpi_dev_remove_notify_handler()`,
which first appeared in mainline v6.6, so dkms.conf sets
`BUILD_EXCLUSIVE_KERNEL_MIN="6.6"`. Per-API table: kmin.md on the integration
branch workspace.

### 5a. clevo_acpi probe ordering (carried, fork patch on upstream v4.24.0 code)

`clevo_platform_probe()` in src/clevo_acpi.c now calls `devm_kzalloc()`
before `acpi_dev_install_notify_handler()`. Upstream v4.24.0 installs the
handler first. If the allocation then failed, probe returned -ENOMEM with the
handler still installed. The driver core does not call `remove()` after a failed
probe, so a later ACPI notify could call into an unloaded module. The
handler install is now the last failing step, so no early return leaks it.

### 6. Carried FlexiCharger patches (the fork's functional delta)

- `762b596` — preserve flexicharger state across
  `CLEVO_CMD_SET_EVENTS_ENABLED` (0x46), refactored by `5304cc9` into
  `clevo_flexicharger_set_events_enabled()`: snapshot live cc4 registers
  before each 0x46, re-apply after via the cc4 live-register path only.
  All four 0x46 sites carried (init, resume, wmi/acpi register, the latter
  with assign-first ordering). Design: S2a/S2b sections below.
- `c37e4cf` — cc4 threshold passthrough: sysfs writes of arbitrary percents
  (1..100) reach the EC verbatim; the legacy-table snap applies only on the
  legacy 0x76 path (moved into `clevo_flexicharger_write_unlocked()` by
  `5304cc9`). `5304cc9` also carries: flexicharger EC error propagation,
  `clevo_flexicharger_lock` serialization, `clevo_wmi.c`
  `method_call_pkgbuf` returns -EOPNOTSUPP. Design: S6 section below.
- `158ad9c` — `power_supply_changed()` after charge_type and threshold
  writes.
- Show-path EC re-read is NOT a fork delta: upstream show handlers call
  `clevo_flexicharger_read()` in both bases (v4.24.0 src/clevo_keyboard.h:835,
  :879, :919); nothing carried.
- The kernel-6.11 platform-remove guard (pre-rebase audit item 5) is
  upstream-native since v4.6.3 and present in v4.24.0; no longer a delta.

Verified: the branch's src/clevo_keyboard.h FlexiCharger region is
byte-identical to the shipped 4.6.2+diamon3 source
(`/var/lib/dkms/clevo-keyboard/4.6.2+diamon3/source/`); container build
warning-clean (no new W=1/sparse, vermagic 7.2.6-1-siduction-amd64) and
ephemeral live run 2026-09-16 PASS (hook registered, 85/75 round-trip).

### 7. Kept fork-side, not derived from upstream

`juno/` (harness + docs), `README.md`, `dkms.conf`. The 4.6.2-era
`debian/`, `tuxedo-drivers.spec.in`, `tuxedo_keyboard.conf` and
`99-z-tuxedo-systemd-fix.rules` were removed after the v4.22.3 rebase;
dkms.conf is the only packaging. Not vendored (outside the payload layout):
upstream's `files/`, `pkg-tests/`, `package.yml`, `.editorconfig`,
`.gitlab-ci.yml`.

## Reproduce

```sh
git clone https://github.com/tuxedocomputers/tuxedo-drivers.git
cd tuxedo-drivers
base=$(cat juno/BASE-REF)          # e0e0b8a21252b796aa312b53d00dc0e8a3fd738b (tag v4.24.0)
mkdir /tmp/tux-base && git archive "$base" | tar -x -C /tmp/tux-base

diff /tmp/tux-base/Makefile Makefile                                  # item 2
diff /tmp/tux-base/src/tuxedo_keyboard.c src/clevo_keyboard.c         # items 1, 4
diff /tmp/tux-base/src/tuxedo_keyboard_common.h src/clevo_keyboard_common.h  # item 1
diff -r /tmp/tux-base/src/tuxedo_compatibility_check src/clevo_compatibility_check  # items 1, 4 (+Kbuild, item 3)
diff /tmp/tux-base/src/clevo_keyboard.h src/clevo_keyboard.h          # items 1, 6
diff /tmp/tux-base/src/clevo_wmi.c src/clevo_wmi.c                    # item 6 (-EOPNOTSUPP)
diff /tmp/tux-base/src/uniwill_keyboard.h src/uniwill_keyboard.h      # item 1 only
diff /tmp/tux-base/.gitignore .gitignore                              # expect empty
diff /tmp/tux-base/LICENSE LICENSE                                    # expect empty
diff -r /usr/src/clevo-keyboard-4.6.2+diamon3/src/clevo_keyboard.h \
  src/clevo_keyboard.h                                                # item 6 = zero semantic delta (only upstream drift/+rename)
```

---

# Pre-rebase audit: juno `clevo-keyboard 4.6.2-1` payload vs upstream tuxedo-drivers

Historical record as of the vendored 4.6.2 payload; superseded by the
`rebase-4.22.3` base above. Sections S2a/S2b/S6 remain the design documents
for the carried patches.

Payload under audit: `/usr/share/clevo-keyboard/4.6.2` (17 files, dpkg-owned by
`clevo-keyboard 4.6.2-1`, maintainer "Giovanni Caligaris
<junocomputers@outlook.com>", homepage junocomputers.com). Verified
byte-identical to `/usr/src/clevo-keyboard-4.6.2` (`diff -r` empty), as
expected: the deb's postinst cds into the payload and runs `make clean; make
dkmsinstall`, whose target does `cp -R . /usr/src/$(MODULE_NAME)-$(VER)` with
`MODULE_NAME=clevo-keyboard`, `VER=4.6.2` from the payload's `dkms.conf`.

## Old BASE-REF (pre-rebase)

`93302778c54e432086d1fe609d8550e0de844c77` — commit tagged `v4.6.2`
(annotated tag object d20003c138d1c23c087d7d9a4ea7bea8ef669024), upstream
https://github.com/tuxedocomputers/tuxedo-drivers.git, dated 2024-08-13.

No upstream tree matches the payload exactly; `v4.6.2` is the nearest tag:

- 9 of 17 payload files are byte-identical to their v4.6.2 counterparts
  (LICENSE; src/{clevo_acpi.c, clevo_wmi.c, clevo_leds.h, clevo_interfaces.h,
  uniwill_interfaces.h, uniwill_leds.h, uniwill_wmi.c};
  src/clevo_compatibility_check/clevo_compatibility_check.h, renamed).
- The only two files whose content matches a *later* tag are
  src/clevo_keyboard.h and src/uniwill_keyboard.h, which carry a verbatim
  backport of upstream commit 852b8748f3a94e30c81848b311b58f27484d41ab
  ("Fix compile error for kernel 6.11+", 2024-08-24, first released in
  v4.6.3). `git diff v4.6.2 v4.6.3 -- <the other 14 mapped paths>` is empty,
  so v4.6.3 matches nothing extra; v4.6.2 stays closest.
- dkms.conf declares PACKAGE_VERSION=4.6.2 and the postinst hardcodes
  `cd 4.6.2`, consistent with re-basing on v4.6.2.

## Differences from BASE-REF (all 8 differing payload files + 49 dropped upstream files)

### 1. Module-set rename tuxedo-keyboard/tuxedo-drivers -> clevo-keyboard (packaging + file renames)

- `tuxedo_keyboard.conf` -> `clevo_keyboard.conf`, one content change:
  `options tuxedo-keyboard kbd_backlight_mode=0` ->
  `options clevo-keyboard kbd_backlight_mode=0`.
- `src/tuxedo_keyboard.c` -> `src/clevo_keyboard.c`;
  `src/tuxedo_keyboard_common.h` -> `src/clevo_keyboard_common.h`;
  `src/tuxedo_compatibility_check/` -> `src/clevo_compatibility_check/`
  (both member files renamed; the .h is content-identical).
- String renames inside sources: `DRIVER_NAME "tuxedo_keyboard"` ->
  `"clevo_keyboard"` (clevo_keyboard_common.h); pr_fmt `"tuxedo_keyboard"` ->
  `"clevo_keyboard"`; input-device name `"TUXEDO Keyboard"` -> `"CLEVO
  Keyboard"` (clevo_keyboard.c); `".name = "TUXEDO Flexicharger Extension"`
  -> `"CLEVO ..."` (clevo_keyboard.h); `#include` paths adjusted
  accordingly (5 files).

### 2. Makefile rewritten to build only the Clevo trio

- Adds `obj-m := src/clevo_keyboard.o src/clevo_wmi.o src/clevo_acpi.o` in the
  top-level Makefile. Upstream has no obj-m there; its module list lives in
  src/Kbuild and its dkms template registers 23 modules (clevo_acpi,
  clevo_wmi, tuxedo_keyboard, uniwill_wmi, ite_8291, ite_8291_lb, ite_8297,
  ite_829x, tuxedo_io, tuxedo_compatibility_check, 12 tuxedo_nb04/nb05_*,
  tuxedo_nb02_nvidia_power_ctrl). The extra upstream sources are not
  shipped, so the juno tree cannot build them.
- Name/version are sed-extracted from the shipped `dkms.conf`
  (`VER`/`MODULE_NAME`) instead of upstream's `debian/changelog` grep.
- `dkmsinstall`: `cp -R . /usr/src/$(MODULE_NAME)-$(VER)` + `dkms install
  -m $(MODULE_NAME) -v $(VER)`. Upstream instead renders
  `debian/tuxedo-drivers.dkms` -> `src/dkms.conf` and rsyncs `src/` into
  `/usr/src/tuxedo-drivers-$(VERSION)`.
- The legacy `package-deb`/`package-rpm` targets (deb/module-name +
  src_pkg/rpm_pkg.spec skeleton, referring to `tuxedo_keyboard.conf`) are
  kept from the pre-merge standalone tuxedo-keyboard repo layout; the
  skeleton files they copy are not in the 4.6.2 payload (they are in the
  older on-host payload `/usr/share/clevo-keyboard/3.2.11`, same lineage).
  `clean` drops the Kbuild invocation's `$(MAKEFLAGS)` and all of
  upstream's debian/rpm artifact cleanup.

### 3. dkms.conf is a juno-only new file

No upstream counterpart (upstream generates `src/dkms.conf` at install time).
Payload values: `PACKAGE_NAME=clevo-keyboard`, `PACKAGE_VERSION=4.6.2`,
exactly three modules (clevo_keyboard, clevo_wmi, clevo_acpi, all
`BUILT_MODULE_LOCATION=src/`), explicit
`MAKE[0]="make KDIR=/lib/modules/${kernelver}/build"`, `CLEAN="make clean"`,
`AUTOINSTALL="yes"`. The upstream template debian/tuxedo-drivers.dkms has
`PACKAGE_NAME="tuxedo-drivers"`, 23 module stanzas, and no explicit MAKE[0].

### 4. Functional source deltas (juno-only, never upstream)

Verified juno-only: `git log --all -S 'DMI_SYS_VENDOR, "Notebook"'` finds 0
commits upstream.

- src/clevo_keyboard.c: the DMI compatibility gate is commented out:
  `// if (!tuxedo_is_compatible()) return -ENODEV;` (upstream lines 200-201
  active). The driver therefore probes on any vendor, not just TUXEDO.
- src/clevo_compatibility_check/clevo_compatibility_check.c: three
  `DMI_MATCH` vendor strings changed "TUXEDO" -> "Notebook"
  (DMI_SYS_VENDOR, DMI_BOARD_VENDOR, DMI_CHASSIS_VENDOR). Dead code in
  practice: the gate is disabled and the module is not built (no obj-m or
  dkms.conf entry; the shipped `src/clevo_compatibility_check/` has no
  Kbuild, unlike upstream's `tuxedo_compatibility_check/`).

### 5. Kernel-6.11 backport in the two keyboard headers

src/clevo_keyboard.h and src/uniwill_keyboard.h equal the v4.6.3 versions of
those files (modulo the renames in item 1 and one trailing blank line):
upstream commit 852b8748f3a94e30c81848b311b58f27484d41ab "Fix compile error
for kernel 6.11+" (2024-08-24) is backported verbatim —
`#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)` guards making the
platform remove callback `int`-returning on < 6.11 and `void` on >= 6.11.

### 6. Upstream files not shipped in the payload (49 of 65 in v4.6.2)

- Top level: `.gitignore`, `99-z-tuxedo-systemd-fix.rules`, `Kbuild`,
  `README.md`, `tuxedo-drivers.spec.in`, `debian/` (10 files i.a. the dkms
  template and changelog).
- src/: `Kbuild`, `ite_8291/`, `ite_8291_lb/`, `ite_8297/`, `ite_829x/`,
  `tuxedo_io/`, `tuxedo_nb02_nvidia_power_ctrl/`, `tuxedo_nb04/`,
  `tuxedo_nb05/`, and `tuxedo_compatibility_check/Kbuild`.

### 7. Install wiring (host state, not a tree delta)

`/var/lib/dpkg/info/clevo-keyboard.postinst`:
`cd /usr/share/clevo-keyboard/4.6.2; make clean; make dkmsinstall;
modprobe clevo_acpi; modprobe clevo_wmi; modprobe clevo_keyboard`.
`dkmsinstall` copies the payload into `/usr/src/clevo-keyboard-4.6.2` and
runs `dkms install`. This is why `/usr/share/clevo-keyboard/4.6.2` and
`/usr/src/clevo-keyboard-4.6.2` are byte-identical (verified).

## Reproduce (pre-rebase payload audit)

Run on this host (read-only against system paths):

```sh
git clone https://github.com/tuxedocomputers/tuxedo-drivers.git
cd tuxedo-drivers
base=$(cat juno/BASE-REF)          # 93302778c54e432086d1fe609d8550e0de844c77 (tag v4.6.2)
mkdir /tmp/tux-base && git archive "$base" | tar -x -C /tmp/tux-base

P=/usr/share/clevo-keyboard/4.6.2
diff -r "$P" /usr/src/clevo-keyboard-4.6.2        # expect no output (master == DKMS copy)
diff -r "$P/src" /tmp/tux-base/src                # items 1, 4, 5 + dropped src trees
diff    "$P"/Makefile /tmp/tux-base/Makefile      # item 2
diff    "$P"/clevo_keyboard.conf /tmp/tux-base/tuxedo_keyboard.conf   # item 1
diff    "$P"/dkms.conf /tmp/tux-base/debian/tuxedo-drivers.dkms       # item 3 (juno vs template)
git log --oneline --all -S 'DMI_SYS_VENDOR, "Notebook"'               # item 4: expect 0 hits
git show 852b8748f3a94e30c81848b311b58f27484d41ab -- src/clevo_keyboard.h src/uniwill_keyboard.h  # item 5
```

## Install history

One line per event for `clevo-keyboard` and `juno-drivers-diamon`, from
`zgrep` on `/var/log/apt/history.log*` (retained window starts 2025-09-02):

- `clevo-keyboard 4.6.2-1`: no apt-history event — install predates the
  retained logs (and all rotated dpkg.logs). dpkg evidence:
  `/var/lib/dpkg/info/clevo-keyboard.list` mtime 2025-05-14 (install date);
  postinst content dated 2025-04-23 (deb build date). Still installed.
- 2026-08-30 17:41 `juno-drivers-diamon 0.5.48.1+diamon1` installed
  (replaces `juno-drivers 0.5.48+local1`, upgraded same day 17:39).
- 2026-08-30 19:03 `juno-drivers-diamon` upgraded 0.5.48.1+diamon1 -> 0.5.48.2+diamon1.
- 2026-08-31 03:06 `juno-drivers-diamon` upgraded 0.5.48.2+diamon1 -> 0.5.48.2+diamon2.
- 2026-08-31 08:32 `juno-drivers-diamon` upgraded 0.5.48.2+diamon2 -> 0.5.48.2+diamon3 (current).

## S2a mechanism: threshold reload-revert

**Mechanism (one sentence):** on this Tongfang `V5xTNC_TND_TNE` (cc4
flexicharger), the driver's load-time `CLEVO_CMD_SET_EVENTS_ENABLED` (0x46)
EC write — issued twice at every (re)load, first by
`clevo_keyboard_add_interface()` on interface registration and again by
`clevo_keyboard_init()` at probe — makes the EC restore its live
flexicharger registers from NVRAM, and because cc4 threshold writes
(cmd 0x04, subcommand 0x1f package buffer) update only the live registers
without an NVRAM save, user-set thresholds (95/100) are silently replaced
by the persisted NVRAM values (70/80) at every full stack reload.

### Flexicharger path identification: cc4 (over ACPI); legacy also present

- Legacy probe (0x77 read + 0x76 round-trip) succeeds from both interfaces
  → `legacy=1`.
- cc4 probe (cmd 0x04, arg 0x1e, buffer return) *fails over WMI*
  (`wmi method_call cmd=0x04 -> int 0xffffffff`; WMI cannot return buffers)
  and *succeeds over ACPI* (`cc4_read raw len=256 buf[0..2]=01 46 50`) →
  `cc4=1` whenever `clevo_acpi` is the active interface (the host's boot
  order registers ACPI first, so ACPI is active; see boot journal
  `clevo_acpi: interface initialized` before `clevo_wmi:`).
- `clevo_flexicharger_read/write` re-probes `has_cc4` on *every* call, so
  the effective data path is cc4 (`0x04` read / `0x04` 0x1f pkgbuf write)
  regardless of what `clevo_battery_add` identified at hook time. The hook
  time identification only decides a dmesg string and, on WMI-only
  registration windows, mislabels the EC as legacy.
- The `clevo_battery_add` legacy probe's 0x76 write-back writes the
  thresholds it just read (idempotent) and is exonerated as the cause:
  with 0x46 skipped it write-backs 95/100 and nothing reverts.

### Evidence

Host: siduction, kernel 7.2.2-1-siduction-amd64, Tongfang V5xTNC_TND_TNE,
2026-09-04. Full logs: `/tmp/s2a-capture.log`, `/tmp/s2a-cycle3.log`;
reduced table: `juno/EVIDENCE-s2a.md`. Commands: `sudo -A bash
juno/RELOAD.sh --modules-dir <worktree>/src 95 100` with the S2A-TRACE
build (commit d031483, cycles 1-2) and the ablation build (commit
42ff690, cycle 3) under `sudo -A dmesg -wHL` capture.

Cycle 2 (0x46 active) — user writes reach the EC live registers, are lost
at reload; the only EC write between the last 95/100 read and the first
70/80 read is 0x46:

```
clevo_keyboard: S2A-TRACE cc4_write prev=70/80/1 set=95/80/1 buf[0..3]=1f 01 5f 50
clevo_acpi:     S2A-TRACE acpi pkgbuf cmd=0x04 len=255 arg[0..3]=1f 01 5f 50
clevo_keyboard: S2A-TRACE cc4_write prev=95/80/1 set=95/100/1 buf[0..3]=1f 01 5f 64
clevo_keyboard: S2A-TRACE cc4_read raw len=256 buf[0..2]=01 5f 64 (status=1 start=95 end=100)
# teardown: battery_remove, one read-only cmd=0x27, interface removes; no flexicharger EC write
clevo_keyboard: S2A-TRACE clevo_keyboard remove done
clevo_wmi:      S2A-TRACE wmi interface add
clevo_wmi:      S2A-TRACE wmi method_call cmd=0x46 arg=0x00000000      # <-- only EC write in the gap
clevo_keyboard: S2A-TRACE pre-events-enable 0x77 state start=70 end=80 status=1   # already reverted
clevo_acpi:     S2A-TRACE acpi method_call cmd=0x46 arg=0x00000000     # second load-time 0x46
```

Cycle 3 (ablation build, all three load-time 0x46 writes stubbed out,
zero `cmd=0x46` in the whole cycle) — thresholds survive the identical
teardown+reload:

```
clevo_keyboard: S2A-TRACE events-enable 0x46 SKIPPED (S2a causation experiment)   # x3
clevo_keyboard: S2A-TRACE pre-events-enable 0x77 state start=95 end=100 status=1
# RELOAD.sh output: after_reload_start=95 after_reload_end=100
```

Control observations: thresholds hold 95/100 across 30 s idle with the
stack resident (no self-decay), and hold across full `rmmod` of all four
modules when 0x46 is not issued (EC live registers persist through the
module gap; only the 0x46 write clobbers them).

Answers to the S2a analysis questions: (a) the re-added hook reads 70/80
from the EC, i.e. the revert is complete before the first probe read;
(b) user sysfs writes reach only the live registers — no NVRAM save
happens for cc4 0x1f writes on this EC (post-0x46 reads return the old
NVRAM values 70/80, not the written 95/100); (c) yes — the revert is
tied to `CLEVO_CMD_SET_EVENTS_ENABLED` (0x46), shown by ablation.

### Patch recommendation: arm (driver-patch) EC-state-preserving registration

Snapshot the live flexicharger state immediately before each 0x46 issue
site (`clevo_keyboard_add_interface()` wmi/acpi branches,
`clevo_keyboard_init()`; the interface serving the call is in hand at
each site) and re-apply it immediately after, so the EC's NVRAM-reload
side effect is undone within the same registration call. Rationale: the
driver itself causes the reset at choke points it fully controls, and the
live EC state provably survives module teardown (cycle 3), so a
read→0x46→rewrite pair is exact and race-free; a userspace udev-restore
helper would instead need its own config authority and leaves a window in
which the EC enforces the stale NVRAM limits. Note for implementation:
over WMI the pkgbuf write path does not exist (WMI cannot transport
buffer results), so the re-apply must go through `clevo_flexicharger_write`
which falls back to the legacy 0x76 sequence under WMI.

**Superseded (S2b, commit 762b596):** the re-apply ships cc4-only; the
legacy 0x76 fallback prescribed above is deliberately not used because
its status write triggers an NVRAM save. See "S2b patch" below.

## S2b patch: flexicharger-state preservation across 0x46 (commit 762b596)

`clevo_flexicharger_preserve_begin()` / `_end()` (src/clevo_keyboard.h)
bracket all four `CLEVO_CMD_SET_EVENTS_ENABLED` (0x46) issue sites: the
wmi and acpi branches of `clevo_keyboard_add_interface()`,
`clevo_keyboard_init()`, and `clevo_keyboard_resume()`. begin() snapshots
the live start/end/status (the full sysfs-exposed set) once per 0x46
sequence and holds the snapshot until a restore succeeds; end()
re-applies it at every site whose active interface can carry the cc4
buffer transport, so a snapshot taken before a WMI-served 0x46 is
restored at the first later ACPI-served site, and on WMI-only hosts the
helpers degrade to no-ops beyond a harmless read attempt. Re-applying is
unconditional at transport-capable sites: the cc4 write is an idempotent
read-modify-write of live registers, verified side-effect free for
matching values.

**Persistence contract (decision):** the re-apply goes through the cc4
live-register write path only. The legacy 0x76 path's trailing status
write (`clevo_legacy_flexicharger_write()`, src/clevo_keyboard.h:632; the
`status go last as it also triggers save` comment at :628) makes the EC
persist to NVRAM, so re-applying through it would silently overwrite the
user's NVRAM profile; it is deliberately NOT used. cc4 writes touch live
registers only, so the user's NVRAM profile remains authoritative across
resume and reboot.

Validation: host leg — witnessed reload/resume runs on attempt-1
binaries (reviewer-accepted); container leg —
the S2b container harness run (build, no-new-warnings, vermagic); harness and
run record since removed, see the git history of `juno/`.


## S6: cc4 threshold write passthrough (user requirement)

The stock store handlers snap every write to the legacy-protocol tables
(`array_find_closest_u8` vs start {40,50,60,70,80,95}, end {60,70,80,90,100}),
also on cc4-capable ECs whose pkgbuf carries raw percents. On cc4 the value
now passes through unsnapped (1..100 bounds kept); the legacy snap remains for
0x76 ECs. Verified: 75/85 write→readback exact on the Tongfang V5xTNC_TND_TNE
(cc4). `charge_control_*_available_thresholds` sysfs lists remain the legacy
table (cosmetic; upstream-shaped). Container harness
RESULT PASS (since removed), image debian:sid c37028b7f13d, machine laptop, 2026-09-05.
Module version: dkms.conf PACKAGE_VERSION=4.6.2+diamon2.

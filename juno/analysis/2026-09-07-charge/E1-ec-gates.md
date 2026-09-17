# E1 — charge-gate enumeration: running driver / EC (beyond flexicharger NVRAM)

Date: 2026-09-07. Host: Juno laptop, Tongfang V5xTNC_TND_TNE.
Kernel 7.2.3-1-siduction-amd64, boot 2026-09-07 03:33:16 (`uptime -s`), no PM suspend
entries in this boot's journal (the 24 `suspend` string hits are service/test names only).
Incident state: BAT0 19 %, AC online=1, `status=Not charging`, `current_now=0` after
40 s `Standard` and 40 s `Custom 75/85` phases (`/tmp/opencode/charge-probe2-163216.log`,
verdict `phase1_charged=0 phase2_charged=0`).
Scope: strictly read-only. No sysfs writes, no module control, no ec_sys/acpi_call.
Driver source paths below are `/usr/src/clevo-keyboard-4.6.2+diamon2/src/` (= repo
@c37e4cf byte-identical, see GATE-8) unless noted.

## Gate enumeration

GATE-1	charge_type (flexicharger enable)	channel: sysfs	`Custom` (EC live status byte 1); manager probe held `Standard` 40 s at 19 %/AC and still read `current_now=0`	/sys/class/power_supply/BAT0/charge_type; show/store clevo_keyboard.h:784-824 (read via cc4 dispatch 710-718); probe log /tmp/opencode/charge-probe2-163216.log
GATE-2	charge_control_start_threshold	channel: sysfs	`75`, live cc4-bank raw percent (no legacy snap on cc4 ECs)	/sys/class/power_supply/BAT0/charge_control_start_threshold; clevo_keyboard.h:826-871, passthrough comment 858-859
GATE-3	charge_control_end_threshold	channel: sysfs	`85`, live cc4-bank raw percent	/sys/class/power_supply/BAT0/charge_control_end_threshold; clevo_keyboard.h:873-919, passthrough comment 906
GATE-4	charge_control_*_available_thresholds lists	channel: sysfs	start `40 50 60 70 80 95`, end `60 70 80 90 100` — static legacy tables printed unconditionally; live 75/85 are absent from the lists (RO attrs, cosmetic only)	clevo_keyboard.h:509-510 (tables), 921-957 (unconditional show loops), 965-966 (RO attrs)
GATE-5	BAT0 status / current_now / AC online	channel: sysfs	`Not charging`, `0` uA, AC `online=1`: kernel ACPI battery driver relaying EC `_BST` state=0 (neither charging nor discharging bit set) and `_BST` rate=0 with `_PSR`=1; clevo_keyboard never writes these files, it only registers an attribute group	/sys/class/power_supply/BAT0/{status,current_now}, /sys/class/power_supply/AC/uevent; driver hook only adds attrs: clevo_keyboard.h:1023-1031 (`battery_hook_register`), 982-1012, 970-979
GATE-6	legacy flexicharger bank (0x77) + NVRAM contents	channel: none	live-registers readback goes to the cc4 bank only when has_cc4 (710-718); the 0x77 legacy read (557, 589) is reached only on cc4 failure and is exposed at no user channel; NVRAM content becomes visible only as what the EC restores after a 0x46 write — i.e. at boot or reload. Reload is forbidden now: (a) read-only scope, no module control; (b) any reload fires 0x46 → EC NVRAM-restores live flexicharger registers, destroying the 75/85 live state and the running-build witness	clevo_keyboard.h:557,589,710-718; quirk comment 732-737
GATE-7	NVRAM flexicharger copy	channel: none	boot-time EC NVRAM→live restore happened (03:33, 0x46 quirk; kernel `ACPI: battery: new hook: CLEVO Flexicharger Extension` at 03:33:21.738) and the udev helper's first re-apply of the user's 75/85 Custom profile is logged 03:33:21.799 (pid 729, 8 helper applies within that second) — and it has NOT stopped: /etc/udev/rules.d/60-clevo-flexicharger-restore.rules:4-5 fires the helper on every BAT0 add|change uevent, and the kernel has emitted those in a near-continuous storm all boot. Re-measured live 2026-09-07 17:13-17:15 EDT: 607579 `applied start=75 end=85 type=Custom` entries this boot and still growing (`journalctl -b SYSLOG_IDENTIFIER=flexicharger-restore | wc -l`; identifier cross-check: 0 entries with any other payload); rate 812/min over a 64 s control window (two `journalctl -b -g 'applied start=75 end=85 type=Custom' | wc -l` counts: 607523 @17:13:50 → 608390 @17:14:54); boot-long average ~740/min (607551 entries over 03:33:21.799 → 17:13:52); per-second rates measured 6-38/s; first stamp via `journalctl -b -g 'applied start=75 end=85 type=Custom' --output=short-unix | head -1` → 1788766401.799236; latest entry under 1 s old at measurement end. Journal is persistent and retains this boot from the kernel `Linux version` line (1788766401.439076), so the count is not rotation-truncated. Counting pitfall: `journalctl -g` matches MESSAGE only and the helper's tag is a syslog identifier (`logger -t flexicharger-restore`, helper line 16), so `journalctl -b -g 'flexicharger-restore.*applied'` matches NOTHING on this box (verified: `-- No entries --`) — count on the identifier or the payload text	commands as quoted (17:13-17:15 EDT); rule /etc/udev/rules.d/60-clevo-flexicharger-restore.rules:4-5; helper /usr/lib/clevo-keyboard-dkms/flexicharger-restore:13-16; profile /etc/clevo-flexicharger.conf:3-5; mechanism in logbook ~/repos/memory/juno-drivers-debian/2026-09-05-charging-fix.md
GATE-8	preserve patch in the RUNNING build	channel: none	running-build witness: /proc/kallsyms carries `clevo_flexicharger_preserve_begin.part.0`, `clevo_flexicharger_preserve_end`, `clevo_cc4_flexicharger_read`, `clevo_cc4_flexicharger_write` (+ .cold) under `[clevo_keyboard]`; /sys/module/clevo_keyboard/coresize=73728 (stock build measured 69632, EVIDENCE-s2a/logbook); srcversion absent (CONFIG_MODVERSIONS unset — vacuous by design, not a red flag)	/proc/kallsyms; /sys/module/clevo_keyboard/coresize; all four 0x46 sites bracketed in src: 1047-1049, 1114-1116, 1157-1159, 1169-1171
GATE-9	clevofan 2.4 EC-port contention	channel: none	second uncoordinated EC client: raw `inb`/`outb` on EC ports 0x62/0x66 (EC_DATA/EC_SC) from hwmon `.read`/`.write` ops with no acpi_ec transaction lock; an interleaved byte stream mid-`_DSM` can fail a cc4 read transiently, and on store that failure flips `clevo_has_cc4_flexicharger` false → reroutes to the legacy path whose trailing 0x76 status write NVRAM-saves (`status go last as it also triggers save`). Exposure is far larger than a one-shot profile write: the flexicharger-restore helper (GATE-7) executes the cc4 store path start+end+type, up to ~3 EC transactions per run, at ~8.1e2 runs/min sustained (~2.4e3 store transactions/min; ~6.1e5 runs logged this boot) — each run is a fresh exposure of the cc4-fail→legacy-reroute→NVRAM-persist sequence, the exact corruption class the preserve patch exists to contain. Open observation, not a verdict: the storm's own source is unidentified — the kernel emits BAT0 change uevents at high rate (8 sampled inside a single 0.22 s burst via `timeout 8 udevadm monitor --kernel --subsystem-match=power_supply`), possibly EC state flapping. No readback exists for this hazard: it is a corruption RISK gate, not a charge gate	/usr/src/clevofan-2.4/clevofan.c:17-18 (ports), 114-141 (ec_io_wait/ec_io_do raw IO), 443-447 (hwmon_ops); reroute/save clevo_keyboard.h:903-905, 720-729, 628-633; helper stores /usr/lib/clevo-keyboard-dkms/flexicharger-restore:13-15
GATE-10	EC-internal inhibit candidates	channel: none	not observable through any driver channel on this host: thermal charge-foldback, power-budget charge pause, ship mode, battery calibration mode, pack PF/CUV flags (this EC exposes no such bits via ACPI battery sysfs). The one related driver write that exists — CLEVO_CMD_OPT SUB_SET_PERF_PROF — is DMI-gated to AURA1501/EDUBOOK1502/NL5xRU/NV4XMB,ME,MZ/L140CU/NS50MU/NS50_70MU/PCX0DX/PCx0Dx_GN20/L14xMU; board V5xTNC_TND_TNE is not in the list, so the write never runs on this host	clevo_interfaces.h:71-72; tuxedo_io/tuxedo_io.c:313-314; clevo_keyboard.h:1054-1069 (DMI list, gated write at 1068)
GATE-11	clevo_acpi / clevo_wmi sysfs surface	channel: none	expected empty, confirmed: `grep -c DEVICE_ATTR` → clevo_acpi.c:0, clevo_wmi.c:0 — pure interface shims (method_transport only), no charge or other sysfs controls	grep -c DEVICE_ATTR /usr/src/clevo-keyboard-4.6.2+diamon2/src/clevo_acpi.c /usr/src/clevo-keyboard-4.6.2+diamon2/src/clevo_wmi.c
GATE-12	platform attrs + CLV ACPI devices + ACPI tables	channel: sysfs	expected non-charge surface, confirmed: /sys/devices/platform/clevo_keyboard/ = fn_lock (=0), leds/white:kbd_backlight, driver, modalias, input, power — none charge gates. CLV0001:00 → driver clevo_acpi; CLV0002:00 → no driver bound. BAT0/extensions/ is empty. /sys/firmware/acpi/tables/* are `-r-------- root` (`head DSDT` → Permission denied): DSDT/SSDT body not enumerable without sudo — blocker: root-gated files	/sys/devices/platform/clevo_keyboard/; /sys/bus/acpi/devices/CLV000{1,2}:00; stat /sys/firmware/acpi/tables

## Notes behind the enumeration

### Sysfs-exposed gates (GATE-1..5)

Live values (read 2026-09-07 ~16:50, incident state unchanged since probe 2):

```
charge_type = Custom
charge_control_start_threshold = 75
charge_control_end_threshold = 85
charge_control_start_available_thresholds = 40 50 60 70 80 95
charge_control_end_available_thresholds = 60 70 80 90 100
status = Not charging ; capacity = 19 ; current_now = 0 ; AC online = 1
voltage_now = 15009000 uV ; charge_now = 734000 uAh
```

The available-thresholds lists are the static legacy 0x76 tables printed by unconditional
loops (clevo_keyboard.h:921-957 over arrays :509-510) regardless of `has_cc4`; on this
cc4 EC the store paths pass raw percents verbatim (passthrough comments 858-859, 906), so
the live 75/85 legitimately sit outside the printed lists. The lists are RO documentation
attributes, not enforcement.

### _BST decomposition (GATE-5)

The kernel ACPI battery driver decodes EC `_BST` `state` bitfield: bit1 set → `Charging`,
bit0 set → `Discharging`, neither → `Not charging`; `current_now` is `_BST` rate. AC
`online=1` is `_PSR`. clevo_keyboard's only interaction with `status` is additive: it
registers `battery_hook` (clevo_keyboard.h:1023-1031) which attaches the attribute group
(:970-979) so `charge_type`/`charge_control_*` appear on BAT0; it never overrides the
ACPI-reported status. The kernel is therefore faithfully relaying the EC's `state=0`.

### Enforcement-bank ambiguity (GATE-6, GATE-7)

Every user-visible readback of the flexicharger state goes through `clevo_flexicharger_read`
(clevo_keyboard.h:710-718): on this host `clevo_has_cc4_flexicharger` (:671-679) succeeds,
so all reads hit `clevo_cc4_flexicharger_read` (:638-668, ACPI cmd 0x04 sub 0x1e) — the
live cc4 register bank. The legacy 0x77 bank (:557, :589) and the NVRAM copy have no user
channel in the running driver, and NVRAM content is only manifest as the EC's post-0x46
restore (boot or reload). What is known about NVRAM: at the 03:33 boot the EC restored
NVRAM → live (0x46 quirk); the udev helper's first re-apply of the user's 75/85 Custom
profile is at 03:33:21.799 (8 applies within that second) and the helper has kept firing
on a BAT0 change-uevent storm ever since — 6.08e5 `applied start=75 end=85 type=Custom`
entries this boot as of 17:14 EDT, ~740/min boot average, 812/min in the 17:14 control
window, still growing (commands and the `-g` MESSAGE-only counting pitfall in the GATE-7
row). Probe 2 settles the diagnostic value of unknown banks: even if some other bank held
e.g. 70/80, 19 % < 70 would NOT inhibit charging, and `Standard` mode ignores thresholds
entirely — no known bank content explains `current_now=0` in both phases.

### Running-build witness (GATE-8) and the 16:14 on-disk rebuild

Proven for the running build: the preserve/cc4 symbols are linked into the loaded module
(/proc/kallsyms, addresses under `[clevo_keyboard]`), and coresize 73728 matches the
patched-build witness (stock measured 69632 in the s2a evidence). Payload identity of the
dkms source tree is byte-exact: `cmp` of src/{clevo_keyboard.h,clevo_keyboard.c,
clevo_acpi.c,clevo_wmi.c,clevo_interfaces.h} against repo @c37e4cf → all IDENTICAL;
sha256 of the repo and /usr/src copies of clevo_keyboard.h collapse to 1 unique hash;
both src file mtimes are 2026-09-05 05:24:37 (predate the whole incident day);
dkms.conf mtime is 2026-09-07 09:27 (config-level touch only, content as documented).

The on-disk `/usr/lib/modules/7.2.3-1-siduction-amd64/updates/dkms/clevo_keyboard.ko.zst`
has mtime 2026-09-07 16:14:11 — a dkms in-place rebuild (make.log "Mon Sep 7 04:14:08 PM
EDT 2026", 3 s, exit 0, same gcc-15) that POSTDATES the 03:33:16 boot by 12 h 41 m. That
binary therefore cannot be the running image: the running module was loaded from whatever
.ko existed at boot, and the running-build case rests on kallsyms + coresize, not on the
on-disk file. Since the dkms source was byte-unchanged since 2026-09-05 (repo-c37e4cf
identity, above), the 16:14 rebuild reproduced the same payload; the symbol set in the
running module proves the boot-loaded image already carried it. Remaining unproven:
byte-identity of the loaded image vs the 16:14 .ko (not checkable without the loaded image
or a reload), and anything a same-content rebuild could not change. None of this weakens
the conclusion, because the witness for "running build = patched build" is the running
kernel's own symbol table.

### clevofan as second EC client (GATE-9)

clevofan 2.4 (/var/lib/dkms/clevofan/2.4, module loaded — /sys/module/clevofan present)
talks to the EC with raw port IO: `EC_SC 0x66` / `EC_DATA 0x62` (clevofan.c:17-18),
`inb` polling loops and `outb` command sequences in ec_io_wait/ec_io_do (:114-141),
invoked from hwmon `.read`/`.write` ops (:443-447). It takes no lock shared with
drivers/acpi/ec.c, whose `_DSM`/method transactions carry BAT0 `_BST` reads and the cc4
transport. An unlucky interleave mid-transaction can corrupt a cc4 read transiently; on
the *write* path that failure is caught by `clevo_has_cc4_flexicharger` (clevo_keyboard.h:
903-905) and silently reroutes to `clevo_legacy_flexicharger_write` (:720-729), whose
trailing 0x76 status write persists to NVRAM (:628-633, `status go last as it also
triggers save`) — i.e. a missed cc4 transaction can silently rewrite the user's NVRAM
profile. Exposure arithmetic changed with the storm finding under the GATE-7 row: the
reroute is no longer exposed only on rare user writes — the helper performs start+end+type
stores (up to ~3 EC transactions on this cc4 EC) per run at ~8.1e2 runs/min sustained
this boot (~2.4e3 store-path EC transactions/min; 6.1e5 runs), so the
cc4-fail→legacy-reroute→NVRAM-save sequence gets on the order of 1e3 fresh exposures per
minute, the exact corruption class the preserve patch exists to contain. Unresolved open
observation (not a verdict): why the kernel emits BAT0 change uevents at this rate at all
— 8 sampled inside one 0.22 s burst (`timeout 8 udevadm monitor --kernel
--subsystem-match=power_supply`, 17:11 EDT) — possibly EC state flapping. This is a
standing corruption-risk gate (no readback exists for it), not a charge gate; it cannot
by itself produce a steady `current_now=0` with reading-back 75/85.

## Missing-enumeration statement

Nothing root-gated was opened: /sys/firmware/acpi/tables (DSDT/SSDT AML) is `-r--------
root`, so an AML-level _BST/flexicharger method audit is not enumerable from this shell
(blocker: needs sudo). No ec_sys or acpi_call was used. CLV0001:00/CLV0002:00 carry no
charge-control attributes regardless of binding (GATE-11/12).

## Result

VERDICT: No gate visible in the running driver explains no-charge-at-19%. The sysfs-visible gates all permit charging (Custom 75/85 asserts below-19%-start; Standard ignores thresholds; both phases still read current_now=0); the kernel ACPI battery driver faithfully relays the EC's `_BST state=0` as `Not charging` with AC online; every threshold-bank hypothesis fails by arithmetic (19 < 70 would not inhibit); the clevofan contention gate is a transient-corruption risk, not a steady-state charge hold. The hold is below the driver — in the EC (thermal/power-budget/ship/PF-CUV class inhibits, none observable) or the pack.

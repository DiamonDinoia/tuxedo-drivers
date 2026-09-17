# EVIDENCE-s2a: clevo stack reload reverts flexicharger thresholds (S2a)

- Host: siduction, kernel `7.2.2-1-siduction-amd64`, Tongfang
  `V5xTNC_TND_TNE` barebone, DMI vendor "Notebook"; DKMS clevo-keyboard
  4.6.2 (`clevo_keyboard`, `clevo_acpi`, `clevo_wmi`) + clevofan 2.4.
- Date: 2026-09-04 (~17:02-17:15 EDT).
- Instrumentation: S2A-TRACE pr_info traces in `clevo_acpi.c`,
  `clevo_wmi.c`, `clevo_keyboard.h` (commits d031483 trace build,
  42ff690 0x46-ablation build; both marked `tmp:`).
- Raw logs (kept out of the repo): `/tmp/s2a-capture.log` (cycles 1+2,
  trace build), `/tmp/s2a-cycle3.log` (cycle 3, ablation build), captured
  with `sudo -A dmesg -wHL > /tmp/<file> &` in the background for the
  duration of each run.
- Canoncial reload tool: `juno/RELOAD.sh` (commit f2b1249).

## Commands

```sh
# build (worktree), payload-mirrored src/, kernel headers on host
make KDIR=/lib/modules/$(uname -r)/build

# cycle 1: stock -> trace build; cycle 2: trace build -> trace build
sudo -A dmesg -wHL > /tmp/s2a-capture.log &
sudo -A bash juno/RELOAD.sh --modules-dir "$PWD/src" 95 100   # twice

# cycle 3: ablation build (all load-time 0x46 writes stubbed)
sudo -A dmesg -wHL > /tmp/s2a-cycle3.log &
sudo -A bash juno/RELOAD.sh --modules-dir "$PWD/src" 95 100

# host restore: stock DKMS modules back
sudo -A bash juno/RELOAD.sh 95 100
```

## Reduced capture table

Timeline excerpts, `S2A-TRACE` lines only, dmesg `[ +delta]`/`[Sep 4
HH:MM]` prefixes stripped. EC encodings: 0x46=70, 0x50=80, 0x5f=95,
0x64=100; 0x77 raw word `0x00EESSTT` = end/start/status.

### Cycle 2 (trace build; 0x46 active) — `/tmp/s2a-capture.log`

| step | line |
|---|---|
| sysfs store start=95 | `start_threshold_store requested=95 clamped=95` |
| cc4 write (start) | `cc4_write prev=70/80/1 set=95/80/1 buf[0..3]=1f 01 5f 50` → `acpi pkgbuf cmd=0x04 -> buf 00 00 00 00` |
| live state after | `cc4_read raw len=256 buf[0..2]=01 5f 50 (start=95 end=80)` |
| sysfs store end=100 | `cc4_write prev=95/80/1 set=95/100/1 buf[0..3]=1f 01 5f 64` |
| live state after | `cc4_read ... buf[0..2]=01 5f 64 (start=95 end=100)` (x4, incl. post-write readback) |
| teardown | `acpi interface remove`; `clevo_keyboard remove start`; `battery_remove`; one read-only `cmd=0x27`; `clevo_keyboard remove done`; `wmi interface remove` — **no flexicharger EC write** |
| reload, 1st EC write | `wmi interface add` → `wmi method_call cmd=0x46 arg=0x00000000 -> int 0x00000100` |
| 1st flexicharger read after reload | `pre-events-enable 0x77 state start=70 end=80 status=1` — **already reverted** |
| 2nd load-time 0x46 | `clevo_keyboard_init` → `dispatch method2 cmd=0x46 ... via clevo_wmi` |
| hook probes (via WMI) | `has_legacy verdict=1`; `wmi method_call cmd=0x04 -> int 0xffffffff`; `has_cc4 verdict=0`; `battery_add identified legacy=1 cc4=0` |
| acpi joins, 3rd 0x46 | `acpi interface add` → `acpi method_call cmd=0x46 status=0` → `active now: clevo_acpi` |
| post-reload reads | `cc4_read ... buf[0..2]=01 46 50 (start=70 end=80)` (repeated) |

RELOAD.sh output both cycles: `after_write_start=95 after_write_end=100`
then `after_reload_start=70 after_reload_end=80` (`charge_type=Custom`).

### Cycle 3 (ablation build; zero `cmd=0x46` in capture) — `/tmp/s2a-cycle3.log`

| step | line |
|---|---|
| stores (no-op rewrite) | `cc4_write prev=95/100/1 set=95/100/1 buf[0..3]=1f 01 5f 64` (x2) |
| 0x46 sites | `events-enable 0x46 SKIPPED (S2a causation experiment)` x3 (wmi add, init, acpi add); `grep -c 'cmd=0x46'` after marker = **0** |
| 1st flexicharger read after reload | `pre-events-enable 0x77 state start=95 end=100 status=1` |
| hook probe | `has_legacy 0x77 read raw=0x00645f01 (start=95 end=100 status=1)` |
| RELOAD.sh output | `after_reload_start=95 after_reload_end=100` |

### Controls

| test | result |
|---|---|
| 30 s idle, stack resident, no reload | 95/100 → 95/100 (no self-decay) |
| immediate readback after sysfs write (stock modules, before any reload) | 95/100 (writes land) |
| `strings src/clevo_keyboard.ko \| grep -c SKIPPED` in ablation build | 1 (marker compiled in); zero `cmd=0x46` dispatches in cycle-3 window (positive control that the stub executed) |

## Verdict

The revert is driver-caused and exact: the load-time
`CLEVO_CMD_SET_EVENTS_ENABLED` (0x46) EC write makes this EC restore live
flexicharger registers from NVRAM; cc4 0x1f-pkgbuf threshold writes never
reach NVRAM, so the user's 95/100 live values are replaced by the
persisted 70/80 on every full stack reload. Eliminated by the captures:
the `clevo_battery_add` legacy probe write-back (idempotent in cycle 3),
any unload-time EC write (none but a read-only 0x27), WMI-vs-ACPI serving
order (identical revert either way), and EC autonomous decay (none over
30 s, none across teardown without 0x46).

Flexicharger path on this EC: **cc4** (cmd 0x04 pkgbuf), served via ACPI
once `clevo_acpi` is active (WMI cannot transport the buffer-returning
0x04 probe: `-> int 0xffffffff`); the legacy 0x77/0x76 interface also
exists and round-trips.

Recommended arm: **driver-patch, EC-state-preserving registration**
(snapshot live flexicharger state before each 0x46 issue site, re-apply
immediately after). See `juno/PATCHES.md` section "S2a mechanism" for the
rationale.

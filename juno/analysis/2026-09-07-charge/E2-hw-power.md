# E2-hw-power — hardware-level no-charge cause ranking and ordered decisive experiments

- Machine: Juno (Tongfang V5xTNC_TND_TNE barebone, MTL-H, INSYDE H2O UEFI; barrel inlet + USB-C PD inlet; internal 4S Li-ion BAT0).
- Date of collection: 2026-09-07, ~16:50–16:53 local (EDT), on the incident box. All reads unprivileged; no sudo, no sysfs writes, no module control, nothing physical performed. Experiments below are DESIGNED, not run.
- Collection commands: `cat /sys/class/power_supply/{BAT0,AC,ucsi-source-psy-USBC000:001}/uevent`, `cat /sys/class/power_supply/BAT0/{charge_type,charge_control_*,present,alarm,cycle_count}`, `cat /sys/class/thermal/thermal_zone*/{type,temp}`, hwmon temp reads, `journalctl -k -b`, `dmesg -T`, two `charge_now` samples 67 s apart, `upower -i .../battery_BAT0`, `/var/lib/upower/history-{rate,charge}-BAT-78-0001.dat` (world-readable).
- Incident state confirmed live: BAT0 19%, `Not charging`, `current_now=0`, AC `online=1`, ucsi psy `online=0 voltage_now=0` type `C [PD] PD_PPS` scope `Device`.

## Contamination caveat (recorded, per contract)

The EC state under measurement is NOT the failure-time state. Probe 1 and probe 2 both toggled
`charge_type` Standard→Custom today (probe 1's udev helper re-imposed the user's baseline). Probe 2's
controlled observation still stands: flexicharger held `Standard` for 40 s produced uA=0
(log `/tmp/opencode/charge-probe2-163216.log`), which exonerates the flexicharger register LEVEL as the
gate. Current readback `charge_type=Custom start=75 end=85` (16:50) matches probe 2's final line
(`type=Custom start=75 end=85`, "udev rule restored") — i.e. the user's normal baseline, not a finding:
at 19% SoC this configuration demands charging start (19 < start threshold 75), so it cannot be the gate.
(upower's `-i` prints thresholds as 75/80 vs sysfs 75/85 — cosmetic rounding in upower, ignore.)

## Live evidence collected

| Item | Value | Note |
|---|---|---|
| BAT0 status / current_now / charge_now | Not charging / 0 µA / 734000 µAh (19%) | six prior samples + this dump agree |
| BAT0 voltage_now | 15011000 µV (15.011 V) → 3.753 V/cell on 4S | far above any CUV band |
| BAT0 health | charge_full 3864000 / design 5100000 → 75.76%; capacity_level=Normal | |
| BAT0 identity | model `BAT`, mfr `Notebook`, serial `0001`, cycle_count 0, present 1, alarm 0 | generic; BMS detail (per-cell, PF flags) is NOT observable via /sys on this EC — Windows vendor app or UEFI only |
| AC (ACPI0003-class `AC`) | online=1 | sole klog AC line: `ACPI: AC: AC Adapter [AC] (on-line)` at boot 03:33:21; no plug events since |
| ucsi / Type-C | psy exists, online=0, voltage_now=0, `C [PD] PD_PPS` | klog has ONLY `typec port0: bound usb3-port6/usb2-port1/usb4_port1` at boot; ZERO `ucsi` lines all boot — no PD-controller fault evidence, nothing attached |
| Stay-flat on AC | charge_now 734000→734000, current 0→0, voltage 15008000→15008000 across 16:50:03→16:51:10 (67 s) | battery-stay-flat while AC powered: CONFIRMED. upower energy-rate 0 W, state pending-charge |
| Thermal (live, replaces stale /tmp/opencode/allsensors.txt) | acpitz 87.0, SEN1 87.05, SEN2 67.05, SEN3 72.05, SEN4 68.05, TCPU 85.05, x86_pkg_temp 86.0, cores 78–90, iwlwifi 77, nvme 54.9 °C | machine under heavy S0 load at read time; battery temperature is NOT exposed anywhere on this EC (no battemp hwmon) |
| Charge knobs | charge_type=Custom; start=75 end=85; available start {40 50 60 70 80 95}, end {60 70 80 90 100} | post-contamination baseline, see caveat |

## Evidence from upower history (consumed, not owned — timeline authoring is another expert's)

Shape facts only, feeding the ranks:

1. Last full charge episode (2026-09-05, the manager-referenced 38→100% run): NORMAL CV taper —
   rate 7.7→0.155 W over ~4 min ending `fully-charged` at 99% (03:15:53 local, epoch 1788592553).
2. TODAY'S BOOT window (re-derived against `history-rate-BAT-78-0001.dat`, epochs in parentheses):
   03:33:26 (1788766406) rate 0 `unknown` (upowerd start) → 03:33:28–03:34:21 `charging`
   (53 s) at 66.9–77.9 W, with two dips to 66.9 W at 03:33:45
   and 68.3 W at 03:33:48; SoC 17% at burst start, 19% by 03:34:01 → 03:34:23 (1788766463)
   `pending-charge` 0.000. A HARD CUT, 77 W→0 in ≤2 s, ~62 s after kernel start (03:33:21),
   ~62 s after `ACPI: battery: new hook: CLEVO Flexicharger Extension` (klog 03:33:21.738).
   The charge path was ELECTRICALLY LIVE this morning with this battery and this adapter.
   CORRECTION NOTE: this window was re-derived from the dat file for this revision; the burst
   begins at 03:33:28, 40 s EARLIER than this file's first reading, and lasts 53 s. The corrected
   LONGER acceptance STRENGTHENS the RANK-1/RANK-4 logic below: the path was closed from ~7 s
   into OS bring-up, so the inhibit was (re)armed LATER inside bring-up, not earlier; and a pack
   that sinks 66.9–77.9 W for a full 53 s is even less CUV/PF-plausible than a shorter burst implies.
3. The machine was upower-silent from 2026-09-05 16:44:42 (SoC 14%, discharging) to today's boot (SoC 17%).
   +3 percentage points across a ~34.8 h gap is NOT normal S5-charge behaviour (hours on AC would reach
   full), but the AC state during the gap is unknown — recorded as motivation for EXP-E5 instead of
   evidence for any rank. (The 17→19% rise is INSIDE the observed boot window, not in the gap.)
4. Contrast for the ranks: episode (1) ends in a taper at 99%; episode (2) ends in a hard cut at 19%.
   A hard cut at low SoC from a cold chassis is not what taper-stop, thermal foldback, or BMS lockout look like.

## RANK — hardware-level no-charge causes (format: name | rank | basis | status)

RANK-1 | EC charge-inhibit latch below the flexicharger registers (residual-engine variant ONLY: sticky Windows vendor-app EC state, or an EC state machine that never re-armed; the 0x46/NVRAM quirk is documented elsewhere and deliberately not re-derived) | 1 | Charge burst 66.9–77.9 W at 03:33:28–03:34:21 (53 s, SoC 17%→19%) hard-cut to 0 by 03:34:23, ~62 s into OS bring-up from a cold chassis, while every OS-visible gate reads non-inhibiting (flexicharger held Standard 40 s → uA=0) — the gate sits BELOW register level and is (re)armed during OS bring-up; the corrected LONGER burst pushes the arming event later into bring-up, which strengthens this rank | live
RANK-2 | Adapter-class / weak-adapter inhibit | 2 | Tongfang barrels carry no digital ID, so brick class is unverified (label unread — EXP-E1); 0 µA under present heavy S0 load (pkg 86 °C) is exactly EC load-shedding on an under-class or sagging brick; the 77 W boot burst happened at near-idle, which does not clear this cause | live
RANK-3 | Thermal charge-foldback | 3 | Chassis zones are hot NOW (acpitz 87.0, SEN1 87.05, pkg 86.0, cores 78–90 °C), but battery temp is not exposed on this EC AND the boot cut fired ≤62 s after a ≥10.8 h powered-off cold soak — thermal cannot explain the arming event; survives only as an under-load co-factor that all S0 protocols must control for | demoted
RANK-4 | Pack/BMS inhibit (cell-imbalance CUV / PF flags) | 4 | 15.011 V on 4S = 3.753 V/cell vs the ~2.5–3.0 V lockout band; capacity_level=Normal; worst of all for this cause, the pack ACCEPTED ~66.9–77.9 W for a full 53 s this very boot — a CUV/PF-locked pack cannot; per-cell/PF detail unobservable via /sys here (Windows vendor app or UEFI only) | falsified

## EXP — ordered decisive experiments (increasing info-per-effort)

Universal telltale law for every step: a charge telltale is NEVER a status string or icon flip.
Positive charge evidence is ONLY (a) paired `charge_now` reads with Δ>0 sustained ≥60 s, or
(b) `current_now` > 100000 µA sustained. Plumbing facts (AC online, ucsi contract) are reported
as plumbing, never as charge evidence. Every step: exactly ONE power source attached unless the step
says otherwise; S0 steps state the load (IDLE = no builds/compiles running, x86_pkg_temp back under
70 °C for ≥5 min — today's 86–90 °C load would let RANK-2/RANK-3 masquerade as RANK-1); the EXP-E2
snapshot runs BEFORE anything that mutates state. Machine-local scratch dir throughout:
`~/charge-forensics/` (no sudo anywhere below).

EXP-E1 | (user, physical, no tools) Read the barrel brick label wattage vs board class | TELL: the printed Output line recorded verbatim (V × A = W) into `~/charge-forensics/brick-label.txt`; under-class brick (this MTL-H + dGPU barebone is commonly in the 180–230 W barrel class — the label reading replaces this guess) PROMOTES RANK-2 directly; in-class label leaves RANK-2 alive only as the sag variant | order 1
  - Preconditions: none (machine may keep running; nothing attached/detached; zero mutation).
  - Protocol: read the label on the barrel PSU brick; photo or transcribe the Output rating.
  - No charge telltale attaches to this step — any charge claim afterwards must pass the universal telltale law.

EXP-E2 | Complete state snapshot (one sudo-free command block; freeze current state BEFORE any mutation) | TELL: snapshot file `~/charge-forensics/snapshot-<ts>.txt` exists, contains the BAT0 uevent block and a charge_now value, and is cited by every later EXP as the pre-mutation baseline (its own charge_now is the "before" of the first paired read) | order 2
  - Preconditions: exactly one source attached (barrel, as found); S0 any load is acceptable here (this step is pure reads); runs BEFORE EXP-E3+.
  - Protocol (run as one block):

    ```sh
    mkdir -p ~/charge-forensics; D=~/charge-forensics/snapshot-$(date +%Y%m%d-%H%M%S); {
      date -Is; uname -r
      for f in /sys/class/power_supply/BAT0/uevent /sys/class/power_supply/AC/uevent /sys/class/power_supply/ucsi-source-psy-USBC*/uevent; do echo "== $f"; cat "$f" 2>/dev/null; done
      echo "== BAT0 knobs"
      for k in charge_type charge_control_start_threshold charge_control_end_threshold charge_now current_now voltage_now capacity capacity_level present alarm cycle_count; do printf '%s=%s\n' "$k" "$(cat /sys/class/power_supply/BAT0/$k 2>/dev/null)"; done
      echo "== thermal"; for z in /sys/class/thermal/thermal_zone*; do printf '%s %s\n' "$(cat $z/type)" "$(cat $z/temp)"; done
      echo "== upower"; upower -i /org/freedesktop/UPower/devices/battery_BAT0
      echo "== klog"; journalctl -k -b --no-pager | grep -iE 'ucsi|typec|charger|AC Adapter|battery'
      cp -v /var/lib/upower/history-rate-BAT-*.dat /var/lib/upower/history-charge-BAT-*.dat ~/charge-forensics/
    } > "$D.txt" 2>&1; echo "written $D.txt"
    ```

EXP-E3 | Barrel unplug/replug with 90 s watch | TELL: plumbing — AC `POWER_SUPPLY_ONLINE` reads 0 while unplugged then 1 on replug (proves the ACPI AC device tracks the physical plug at all; if it never flips to 0, the AC plumbing itself is broken and every later AC-side result is suspect); charge — paired charge_now Δ>0 over ≥60 s post-replug OR current_now >100000 sustained | order 3
  - Preconditions: EXP-E2 done; USB-C port EMPTY (exactly one source: barrel); S0 IDLE as defined above (pause builds first — a hot loaded machine confounds RANK-2/RANK-3 with RANK-1).
  - Protocol: start `for i in $(seq 1 8); do printf '%s AC=%s uA=%s charge=%s\n' "$(date +%T)" "$(cat /sys/class/power_supply/AC/online)" "$(cat /sys/class/power_supply/BAT0/current_now)" "$(cat /sys/class/power_supply/BAT0/charge_now)"; sleep 15; done | tee ~/charge-forensics/e3-watch.txt`; unplug the barrel at ~t=20 s for ≥5 s (confirm AC=0 in the transcript); replug firmly; watch to 90 s total.
  - Read: no charge after a clean 0→1 replug at idle keeps RANK-1/RANK-2 live unchanged; charge resumption at idle-but-not-loaded implicates the load-shedding variant of RANK-2.

EXP-E4 | USB-C PD differential: known-good ≥65 W PD brick on the Type-C port (machine lists `C [PD] PD_PPS`) | TELL: plumbing — ucsi psy `online=1` AND `voltage_now` non-zero (a PD contract actually formed; expected to also produce the first `ucsi` klog lines of the boot); charge — paired charge_now Δ>0 over ≥60 s OR current_now >100000 sustained; PD CHARGES while barrel did not → RANK-2 (barrel-path/adapter-class) up, RANK-1 down; PD SILENT too → EC-global inhibit, RANK-1 strongly up | order 4
  - Preconditions: EXP-E2 done; barrel DETACHED (exactly one source: the PD brick); S0 IDLE.
  - Protocol: attach the PD brick to the Type-C port; `for i in $(seq 1 6); do printf '%s ucsi_on=%s uV=%s uA=%s charge=%s\n' "$(date +%T)" "$(cat /sys/class/power_supply/ucsi-source-psy-USBC000:001/online)" "$(cat /sys/class/power_supply/ucsi-source-psy-USBC000:001/voltage_now)" "$(cat /sys/class/power_supply/BAT0/current_now)" "$(cat /sys/class/power_supply/BAT0/charge_now)"; sleep 15; done | tee ~/charge-forensics/e4-watch.txt`.
  - Read caveat: a failed PD contract with a silent ucsi klog is ambiguous (cable/brick/port); only a FORMED contract (voltage_now>0) with zero charge current is a clean vote for EC-global inhibit.

EXP-E5 | S5 shutdown-charge test | TELL: paired charge_now across the off window: (charge_now read at next boot) − (charge_now the user records on paper/photo at shutdown) > 0, with Δ≥200000 µAh after ≥30 min counting as an unambiguous rise; Δ>0 ⇒ the below-OS charge path WORKS and the gate exists only in the OS-imposed state (confirms RANK-1's arming locus and hands a hard fact to the software-lane expert); Δ=0 ⇒ the gate is below the OS (EC latch or adapter) and EXP-E6 is next | order 5
  - Preconditions: EXP-E2 done; exactly one source (barrel first; optionally repeat later with PD if EXP-E4 gave a contract); no suspend — full `poweroff`.
  - Protocol: `cat /sys/class/power_supply/BAT0/charge_now` — user writes the number and wall time DOWN; shut down with AC attached; leave ≥30 min; boot; read charge_now again; compare on paper, then append both numbers to `~/charge-forensics/e5-s5.txt`.
  - Motivation on record: the 14%→17% drift across the 34.8 h upower-silent gap (evidence item 3) says S5 charging is NOT obviously healthy; the 77 W boot burst says the path can still close. This experiment decides between them.

EXP-E6 | EC reset for an internal-battery board: ALL sources off, hold power ≥30 s, then AC + boot | TELL: within 5 min of reaching an IDLE desktop, paired charge_now Δ>0 over ≥60 s OR current_now >100000 sustained ⇒ the latch was the residual-engine kind and a full EC power drain cleared it (document how long the fix persists; recurrence = firmware bug, escalate to vendor); no change ⇒ RANK-1's latch survives even an EC drain or the residual sag/hardware variant of RANK-2 remains — bench-level escalation | order 6 — ONLY after EXP-E1..E5
  - Preconditions: all of EXP-E1..E5 done and their transcripts saved (this step destroys the latched state under study); EXP-E2 re-run for a fresh "before"; exactly one source (barrel) for the first boot after the drain.
  - Protocol: `poweroff`; detach barrel AND anything on USB-C; hold the power button ≥30 s (internal-battery board, no pinhole assumed — long-press is the EC drain); reconnect barrel only; boot; sit IDLE 5 min; run the EXP-E3 watcher loop once (transcript to `~/charge-forensics/e6-watch.txt`).

EXP-E7 | (OPTIONAL, LAST) Windows boot to read the vendor EC app's adapter/battery mode and health | TELL: the vendor app's reported adapter wattage and battery/charge mode strings transcribed verbatim into `~/charge-forensics/e7-windows.txt` (a physical transcription, not a status/icon telltale); back in Linux afterwards, paired charge_now Δ>0 over ≥60 s detects any latch the Windows app set or cleared | order 7 — LAST, because it MUTATES EC state
  - Preconditions: EXP-E1..E6 all done; fresh EXP-E2 snapshot immediately before the Windows boot (this step's reads become uninterpretable otherwise); exactly one source (barrel).
  - Protocol: boot Windows, open the vendor control app, transcribe adapter/battery/health panes; change nothing, or if a change is unavoidable, write down exactly what changed; boot back to Linux; EXP-E2 again plus one watcher loop.

## Verdict

VERDICT: Most probable cause class right now is RANK-1 — an EC charge-inhibit latch below the OS-visible flexicharger registers, (re)armed during OS bring-up (hard fact: 66.9–77.9 W charge burst sustained for 53 s, hard-cut to 0 at 03:34:23, ~62 s after kernel start from a cold chassis, while every register-level gate reads non-inhibiting); the fastest DECISIVE discriminator is EXP-E5 (S5 shutdown-charge, ~35 min), which directly tests the below-OS layer the boot transient incriminates — EXP-E4 (~3 min wall-clock) is the fastest coarse split between EC-global and barrel-path but cannot, in its both-inlets-silent branch, separate RANK-1 from a second dead inlet, so E5 decides what E4 only narrows.

# D1 — consolidated verdict: no charge at 19 %, and why KDE "does nothing"

Date: 2026-09-07, Juno (Tongfang V5xTNC_TND_TNE, siduction, kernel 7.2.3-1, boot 03:33).
Inputs: D3-timeline.tsv/D3-notes.md (timeline), D2-kde-api.md (KDE path),
E1-ec-gates.md (driver/EC gate map), E2-hw-power.md (hardware ranking + experiments).
All facts below cite those documents; the numbers therein were independently re-derived
by reviewers against live sources.

## The one hard fact

The battery charged THIS boot: 03:33:28→03:34:21, 53 s, 66.9–77.9 W (SoC 17→19 %),
then a HARD CUT to 0.000 W `pending-charge` at 03:34:23 — ~62 s after kernel start,
~62 s after the `CLEVO Flexicharger Extension` battery hook registered (klog
03:33:21.738) — and 0 W in every sample since (upower history, both files). The charge path was
electrically live with this battery and this adapter, and something armed the inhibit
during OS bring-up. A pre-echo exists: 2026-09-04 11:46, identical hard-cut signature
at 37→37 % that self-healed after 65 m 52 s (D3; temporal relations measured there — 4 s
after the 7.2.3 package install, ~28 h before the first 7.2.3 boot, ~18 h before the
helper's first install).

VERDICT: RANK-1 (E2) stands — an EC charge-inhibit latch BELOW the OS-visible
flexicharger registers, (re)armed ~62 s into OS bring-up. Every OS-visible gate is
proven non-inhibiting: charge_type=Custom 75/85 reads back (19 < 75 must charge), a
40 s held `Standard` gave uA=0 (probe2), _BST state bits = 0 is the EC's own claim
faithfully relayed (E1), and no threshold bank content can arithmetically inhibit at
19 % (even the NVRAM-era 70/80: 19 < 70). Separately, the KDE slider "does nothing"
is fully explained and independent: the DKMS udev helper re-imposes 75/85/Custom on
every BAT0 change event, ~10–30 events/s, >6e5 runs this boot — any KDE write is
reverted in ≪1 s (D2 §8–9).

CAUSE-1 EC-inhibit latch below registers (Windows vendor-app residue or EC state
machine never re-armed) — live; basis: burst-then-cut under verified non-inhibiting
registers; pre-echo self-healed without driver changes (E2 RANK-1, D3 pre-echo row).
DECISIVE-EXPERIMENT E5 (S5 shutdown-charge, ~35 min) splits below-OS gate from
OS-imposed state; E4 (≥65 W PD brick differential) splits EC-global from barrel-path.

CAUSE-2 Adapter-class/weak-adapter inhibit — live; basis: brick label unread
(EXPERIMENT E1 costs nothing), charge-shed-under-load signature (0 µA at 86 °C pkg,
while the idle boot burst drew 77 W fine). DECISIVE-EXPERIMENT E3 (barrel replug +
90 s watch incl. AC-plumbing flip check), then E4 (PD differential).

CAUSE-3 Helper write-storm as EC-upset cofactor — unproven, flagged: the udev helper
performs up to 3 EC cc4 transactions per run at ~7–8e2 runs/min
(≈2.4e3 store-path transactions/min all day), exercising the cc4-fail→legacy-reroute→
NVRAM-save hazard class at storm frequency (E1 GATE-9); the BAT0 change-uevent storm
itself (~10–30/s) has an UNIDENTIFIED source — possibly the EC's charge state flapping.
Not a verdict; a thread: if the storm originates in EC state oscillation, the helper
converts oscillation into an EC write flood. Removal (D4) eliminates the writer
regardless.

CAUSE-4 Thermal charge-foldback — demoted: boot cut fired ≤62 s after a ≥10.8 h
powered-off cold soak; current 87 °C zones explain at most a co-factor (E2 RANK-3).

CAUSE-5 Pack/BMS inhibit — falsified: 3.753 V/cell (vs ~2.5–3.0 lockout),
capacity_level=Normal, and the pack itself accepted ~67–78 W for 53 s this boot
(E2 RANK-4).

## Exonerations worth keeping on record

- Flexicharger / any threshold content: arithmetic + 40 s Standard hold.
- Kernel/ACPI battery driver: relays EC state; the running clevo_keyboard build is the
  patched one (kallsyms symbols + coresize 73728; /usr/src byte-identical to c37e4cf).
- Package churn today (diamon6→7, juno-kde-fancontrol, purge): postdates the cut by
  ~12.5 h; mid-session dkms rebuild touched only the on-disk image 16:14.
- upower/KDE stack as charge-hold cause: upower re-applies nothing at start (state
  file absent), powerdevil writes nothing at login (D2 §3); they are bystanders to the
  charge hold (and upower's readout is cosmetically wrong: Enabled=false, End=80 from
  the hwdb wildcard — never live values).

## DECISIVE-EXPERIMENT ladder (from E2; increasing effort, user-executed)

1. DECISIVE-EXPERIMENT E1 — read the barrel brick label (no tools; settles RANK-2's
   ceiling).
2. DECISIVE-EXPERIMENT E2 — sudo-free snapshot block (baseline for all telltales).
3. DECISIVE-EXPERIMENT E3 — barrel replug, 90 s watch (AC ONLINE must flip 0→1;
   telltale = paired charge_now Δ>0 over ≥60 s or current_now >1e5 sustained).
4. DECISIVE-EXPERIMENT E4 — ≥65 W USB-C PD differential (charges → barrel-path cause;
   silent too → EC-global, RANK-1 strongly up).
5. DECISIVE-EXPERIMENT E5 — S5 shutdown-charge ≥30 min (Δ charge_now across the off
   window ≥2e5 µAh ⇒ below-OS path works, gate is OS-state; Δ=0 ⇒ EC latch/adapter,
   go E6).
6. DECISIVE-EXPERIMENT E6 — EC reset (all sources off, hold power ≥30 s; recurrence ⇒
   firmware bug, vendor escalation).
7. DECISIVE-EXPERIMENT E7 — Windows vendor-app read (OPTIONAL, LAST — mutates EC
   state; transcribe its adapter/battery mode strings).

Telltale law everywhere: never a status/icon flip — only paired charge_now Δ>0 over
≥60 s, or sustained current_now >1e5 µA.

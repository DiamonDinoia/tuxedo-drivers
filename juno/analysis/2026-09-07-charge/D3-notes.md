# D3 — boot forensics timeline: no-charge incident (2026-09-07)

Scope: reconstruct 2026-09-04 -> now from unprivileged sources only. Incident state
(manager-verified, do not re-litigate): Juno V5xTNC_TND_TNE, kernel
7.2.3-1-siduction-amd64, boot 2026-09-07 03:33; BAT0 Not charging at 19%, AC online,
current_now=0, including with the flexicharger held at Standard for 40 s.

## The onset is a hard cut at 2026-09-07 03:34:23, 62 s after boot

upower `history-rate-BAT-78-0001.dat` shows the machine charging across 22
samples from 03:33:28 to 03:34:21 (17% -> 19%), 66.874-77.895 W: a 75.2-77.9 W
plateau with two sub-70 W dips (66.874 at 03:33:45, 68.251 at 03:33:48), then
0.000 W / pending-charge from 03:34:23 onward. Between 03:34:23 and the upower
restart at 15:59:21 there are exactly two samples in the whole 12.4 h window
(the cut sample and the restart marker): charging never resumed, not even for
one sample interval. This is a switch-off, not a taper: ~77 W at 19% SoC is
full constant-current pace, and the rate drops to zero from one 2-s sample to
the next. No suspend, shutdown, AC-cycle or thermal event appears in the
journal around 03:34:23; only routine userspace noise brackets the cut
(baloo retries, apt-daily finishing); the journal has no power-side entry at
the moment of the cut.

## Onset candidates, ranked by correlation with the 03:34:23 cut

1. **First Linux boot sector of kernel 7.2.3-1-siduction-amd64 with the
   flexicharger stack live.** Kernel installed 2026-09-04 11:38 by a
   user-driven frontend (the apt block carries `Requested-By: marco` and no
   `Commandline` line; blocks written by unattended-upgrade in the same file
   carry `Commandline: /usr/bin/unattended-upgrade`); the only retained
   pre-incident boot on it is boot -1 (Sep 5 16:10, on battery). The failing
   boot is the first recorded AC-charge demand on this kernel with the
   helper/udev stack active, and the failure lands inside the first minute,
   seconds after the boot-time `flexicharger-restore` write storm (75/85
   Custom re-applied 77 times in the first 3 s, 3683 times in the first 99 s)
   and the "ACPI: battery: new hook: CLEVO Flexicharger Extension"
   registration. The EC state left by that intersection (new kernel + hook
   registration + threshold writes at enumeration) is the strongest
   correlate.
   Counter-evidence: the overnight 38→100% charge (Sep 4 12:52 → Sep 5 04:10)
   postdates the kernel install and ran fine, but its kernel/module context is
   not recoverable from retained sources, so it cannot fully exonerate 7.2.3.

2. **The prior hard-cut signature on 2026-09-04 11:46:26 (same machine, old
   running kernel).** Charging stopped at 37% → pending-charge 4 s after the
   7.2.3 kernel-install transaction closed (11:46:22; started 11:38:12), while
   still running the old kernel, then self-healed at 12:52 (AC replug or EC
   recovery — unobserved; journal of that boot rotated away). Two observations
   matter: the failure mode predates the first 7.2.3 boot, and it is
   recoverable in one instance but not in the other. This points at an EC
   latch state that some stimulus (replug, full charge boundary, suspend
   cycle) can clear, rather than at pure kernel regression.

3. **Running module vs mid-session rebuild (provenance murk).** The loaded
   clevo_keyboard was bound at 03:33 from the pre-existing
   `/usr/lib/modules/.../updates/dkms/clevo_keyboard.ko.zst`; the file on disk
   now carries mtime 16:14:11 from the diamon6→diamon7 rebuild (dkms make.log
   exit 0, 3 s build, source still 4.6.2+diamon2). Any earlier rebuild is
   invisible: only one make.log survives per build dir. Meanwhile dkms.conf
   (09:27:40) and the helper (09:27:23) were rewritten on the host mid-incident
   with **no dpkg or sudo journal entry in the whole 09:10-09:35 window** — an
   out-of-package, container-era write (podman debian:sid build churn
   09:16:40-09:25:56 errored on clevo-keyboard packages at 09:20:42).
   So the exact source tree that produced the running module cannot be proven
   from disk artifacts; the running module is older than both the 09:27 edits'
   unknown build products (if any) and the 16:14 rebuild.

Rules themselves out as cause, by dating: the Sep 7 apt full-upgrade
(15:57), `apt purge clevo-keyboard` (16:00), upower/fwupd restarts (15:59:21)
and the diamon7 dkms rebuild (16:13:38–16:14:12) all postdate the onset by
12.5 h. Firmware is also dead-ended: `fwupdmgr get-history` returns "No
history" (nothing ever flashed via fwupd) and DMI reports BIOS 1.07.02
(03/01/2024); the 15:59 fwupd.service cycle is a plain daemon restart.

## Anchor question: did charging break when the helper appeared?

No direct causal line, and an important asymmetry. The helper stack was seeded
by the **first install of clevo-keyboard-dkms 0.5.48.2+diamon5 on 2026-09-05
05:32:46** (dpkg.log: `<none>` → installed), with the rule and conf rewritten
at 05:57 (75/85 Custom). The last **full** charge (38→100%) **ended at
04:10:42 that same morning — 82 min BEFORE the helper existed**. Between the
helper's appearance and the failure there is not a single successful AC
charge: Sep 5 daytime ran on battery (100% → 14% across boots -2/-1), the
machine was dark for 59 h, and the very first charge demand with the helper
live (Sep 7 03:33) is the one that hard-cut after 62 s. So the timeline cannot
exclude the helper stack from the onset; it can only say the helper did not
instantly break a functioning charge path, because no charge path was
exercised in between. The Sep 4 11:46 blip, which pre-dates the helper's
existence by ~18 h, is the strongest single argument that the failure mode is
**not** helper-introduced.

## Boot -2 (Sep 5): shutdown lost the race to the lid switch

The retained journal shows the KDE end-session sequence starting at 14:53:53
(LogoutPrompt, requested by plasmashell; org.kde.Shutdown activated 14:53:55
by ksmserver-logout), then `Lid closed` at 14:53:57.588 while the plasma
session was still tearing down: logind logs `Suspending...` at 14:53:58.378
and sleep.target is reached at 14:53:59. The machine suspends mid-shutdown
instead of powering off. On the 16:05:45 resume (`Restarting tasks: Done`),
the suspend operation finishes at 16:05:47 and the pending poweroff executes
immediately (logind `Powering off...` 16:05:48); the journal ends mid-shutdown
at 16:05:49, at 46% battery. Relevance to the incident: none observed beyond
dating (the dark window starts at the next boot's clean poweroff, 16:44:47) —
but this boot is the origin of the "flexicharger-restore applies 75/85 Custom
from 14:39:55 on" context.

## Mid-session dkms rebuild vs running-module dating

- Running module: loaded at boot 2026-09-07 03:33:21 (journal:
  `clevo_keyboard: module init`, out-of-tree taint).
- On-disk `.ko.zst`: rewritten 16:14:11 by the diamon6→diamon7 dkms rebuild
  (make.log present, exit 0, 3 s build; both package versions ship source
  version 4.6.2+diamon2).
- The pre-16:14 `.ko` was most plausibly built 2026-09-04 ~11:40 by the dkms
  autoinstall wave of the 7.2.3 kernel package (clevofan 11:40:59, nvidia
  11:41:38 survive as witnesses; the clevo witness was overwritten). A 09:27
  rebuild cannot be excluded because make.log does not survive rebuilds.
- Consequence: dmesg/vermagic claims about the running module cannot be
  re-derived from the current on-disk artifact; any fix must reboot or reload
  to take effect, and diagnosis of "what module was running when it broke" is
  bounded by this overwrite.

## Windows-boot determination (gap Sep 5 16:44:47 → Sep 7 03:33:21)

Best determination: **undetermined, with no positive artifact**. Evidence and
its quality:

- `journalctl --list-boots`: 59-h gap between boot -1's clean poweroff and
  boot 0. Weak: a powered-off machine looks identical to a Windows session.
- `efibootmgr -v` (readable unprivileged): BootCurrent=0002 (siduction),
  BootOrder=0002,0001,... (siduction first), **no BootNext**. Weak-moderate
  against a *one-shot* Windows boot via Linux (`--bootnext`), silent on
  firmware-menu or Windows-update-driven boots (BootNext is consumed on use).
- NTFS/BitLocker partitions (nvme0n1p6 BitLocker, p7 ntfs) are **not mounted**;
  no read-only mount exists to inspect hiberfil.sys/$LogFile, and mounting was
  out of scope. ESP is mounted `fmask=0077` (root-only), so bootmgfw.efi
  mtimes are unreadable unprivileged.
- RTC signature: `/etc/adjtime` says UTC; a localtime-RTC Windows boot would
  have produced a multi-hour backward clock jump at the Linux boot; journald
  shows only a small backwards correction at NTP sync ("Time jumped backwards,
  rotating", 03:33:51), no ~4 h signature. Moderate against a
  default-configured Windows boot, null against `RealTimeIsUniversal=1`.
- SoC rose 14% → 17% across the dark window: consistent with ordinary
  EC off-state charging on AC; requires no OS.

Net: nothing found requires or suggests Windows ran; the artifacts that could
settle it (ESP mtimes, NTFS $LogFile) need privileges outside this scope.

## Journal rotation / availability caveats

- journald `SystemMaxUse=500M`; 475.8M in use on this pass, 507.1M on the
  first: a ~32M archived file was vacuumed between the two passes, which is
  also why boot -2's first retained entry moved from 14:24:26 to 14:34:16.
  **Only 3 boots are retained** (Sep 5 mid-afternoon, Sep 5 16:10, Sep 7
  03:33) and the head boundary drifts forward — re-runs of this analysis will
  see later first entries for boot -2.
- Every boot spanning Sep 4 11:14 – Sep 5 14:04 — including the boot that ran
  the successful 38→100% charge and any ~11:46 reboot after the kernel
  install — is rotated away. Statements about those kernels rest on
  upower/dpkg/apt traces only.
- Boot -2's retained journal contains **no kernel ring lines at all** (46 `-k`
  lines, no `Linux version`); its kernel version is unrecoverable.
- The upower 0-rate "unknown" markers at Sep 4 11:14:16, 11:46:26 and Sep 5
  14:04:01 record upowerd (re)starts whose surrounding boots/sessions are
  gone; the 14:04:01 marker has no matching retained boot.
- The helper's own storm inflates the journal beyond "chatty": 606,625
  `applied start=75 end=85 type=Custom` lines in boot 0 alone by 17:11
  (77 firings in the first 3 s, sustained ~590-810/min sampled mid-morning and
  late afternoon), which measurably accelerates the rotation pressure above.
- `/var/log/apt/term.log` is `0640 root:adm`; not readable here, so dkms
  console output of the Sep 4 kernel-install wave is uncaptured.

## Data-quality notes

- upower charge/rate files are append-only event logs (state/`%`-change
  triggered), not uniform samples; "no samples" between 03:34:23 and 15:59:21
  means no state or percent changed, which for a stuck-19% pending-charge
  battery is itself the signal.
- The helper writes sysfs charge thresholds from udev (its shipped purpose);
  observed here as a reader of the journal only — no sysfs writes were made
  for this analysis.

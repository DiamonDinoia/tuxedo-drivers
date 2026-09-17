# D4 — KDE-only 75/85 control: migration plan

Goal: KDE Power Management (KCM Charge Limit spinboxes) is the ONLY writer of
`charge_control_start_threshold`/`charge_control_end_threshold`; the DKMS udev
helper machinery is removed; 75/85 applies. Prerequisite facts (D2, all verified):

- KDE can express 75/85: KCM spinboxes (start 1..stop, stop 50..100) → KAuth
  `org.kde.powerdevil.chargethresholdhelper.setthreshold` → raw sysfs writes,
  0–100, lists not consulted.
- Nothing in the KDE/upower path writes `charge_type` (Dell-style plural-only
  writers vs this driver's singular attr) → `Custom` stands by design; it is
  live now, stable in-session (probe2), and re-applied by the driver's preserve
  logic across 0x46. Nothing must write `Standard`.
- Neither powerdevil (login) nor upowerd (daemon start) re-applies thresholds;
  a cold boot starts from the EC NVRAM profile (outside KDE control).

STEP-1 snapshot (no root): run EXP-E2's snapshot block; record the live triple
(type/start/end) and the helper's journal count (`journalctl -b
SYSLOG_IDENTIFIER=flexicharger-restore | wc -l`).

STEP-2 durable removal in packaging (no root, repo edit): in
~/repos/juno/juno-drivers-debian remove the three payload items from
`clevo-keyboard-dkms`'s install set and the
`debian/` udev/conffile stanzas — `60-clevo-flexicharger-restore.rules`,
`/usr/lib/clevo-keyboard-dkms/flexicharger-restore`, `/etc/clevo-flexicharger.conf`
— bump the version (diamon8), rebuild the deb, and drop the corresponding
test-install.sh asserts. (On-disk deletion alone is re-armed by any package
reinstall/upgrade — D2 §10.)

STEP-3 live removal + reinstall (sudo, user): install the diamon8 deb, then
`sudo rm -f /etc/udev/rules.d/60-clevo-flexicharger-restore.rules
/etc/clevo-flexicharger.conf /usr/lib/clevo-keyboard-dkms/flexicharger-restore &&
sudo udevadm control --reload`. After-upgrade check: `dpkg -L
clevo-keyboard-dkms | grep -iE 'flexicharger|60-clevo'` must print the trio as
GONE. (Do not run `udevadm trigger -s power_supply` as a test — it is a root
action and unnecessary.)

STEP-4 assert sole-writer (no root): `journalctl -b
SYSLOG_IDENTIFIER=flexicharger-restore | wc -l` must equal STEP-1's count
(storm dead); poll the trio for 90 s:
`for i in $(seq 45); do sleep 2; cat /sys/class/power_supply/BAT0/charge_type
/sys/class/power_supply/BAT0/charge_control_{start,end}_threshold; done | sort -u
| sort | uniq -c` — every line must read Custom/75/85.

STEP-5 KDE writes and STICK (user, GUI): System Settings → Power Management →
Charge Limit: set start 74 (or any probe value), Apply, watch sysfs for 60 s —
the new value must survive (the clobber is gone); then set start 75 stop 85,
Apply. Acceptance test (from the team contract): a KDE-written value persists
≥60 s clobber-free. Admin prompt on Apply is expected (`auth_admin_keep`).

STEP-6 accept/document the residual gaps (no action in this plan): a cold boot
takes EC NVRAM's flexicharger profile; nothing in KDE re-applies start/end at
login and nothing anywhere (but the removed helper) re-applied them at boot —
boot NVRAM restore + driver preserve cover in-session reloads only. If a future
boot ever comes up with wrong values or `Standard`, KDE alone cannot fix
`charge_type`; that limitation is the price of KDE-only control and is accepted
unless observed.

STEP-7 hysteresis functional gate (gated on D1 resolution): once charging is
restored by the E-ladder, verify the full contract with the battery: drain
≤75 → charging must resume; charge to 85 → must stop. Telltales only: paired
charge_now deltas over ≥60 s / current_now>1e5 µA. Do not run this before the
charge-hold verdict closes; do not attribute a STEP-7 failure to KDE until the
D1 cause is fixed.

ROLLBACK: one command class, no repo surgery needed: `sudo apt install --reinstall
clevo-keyboard-dkms=<pre-diamon8 version>` (or reinstall the diamon7 deb from the
local repo) re-arms rule+conf+helper; `sudo udevadm control --reload` resumes the
restore behavior; KDE writes will again be clobbered — that is the known old
behavior, not a regression. The pre-diamon8 repo tree carries the trio in
`debian/`, so rebuilding the old version from git history is equally valid.

VERIFY: the plan is verified when ALL hold: (a) STEP-3 dpkg-listing check empty,
(b) STEP-4 storm count frozen and 90 s poll single-valued Custom/75/85, (c)
STEP-5 KDE write persists ≥60 s (acceptance test), and — once D1 is resolved —
(d) STEP-7 drain-charge-stop hysteresis observed with charge_now telltales.
(a)–(c) close this plan's scope; (d) closes the user goal end-to-end.

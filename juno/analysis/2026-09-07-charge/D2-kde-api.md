# D2 — KDE → upower charge-limit API path, breakage mechanism, minimal removal

Machine: juno (siduction, Plasma 6.7.4-1). Date: 2026-09-07. Live state at observation: BAT0
`charge_type=Custom`, `charge_control_start_threshold=75`, `charge_control_end_threshold=85`,
capacity 19%, pending-charge (upower State=5). Available lists print legacy tables only:
start `40 50 60 70 80 95`, end `60 70 80 90 100` (driver prints them unconditionally,
`/usr/src/clevo-keyboard-4.6.2+diamon2/src/clevo_keyboard.h:921-958`).

UPOWER-VERSION: 1.91.3-1 (dpkg), runtime `UPower client version 1.91.3 / UPower daemon version 1.91.3` (`upower --version`)
KDE-START-THRESHOLD: YES — KCM `kcm_powerdevilprofilesconfig` "Charge Limit" group has both spinboxes (`chargeStartThresholdSpin` 1..stop, `chargeStopThresholdSpin` 50..100, powerdevil 6.7.4 `kcm/ui/GlobalConfig.qml:239-280`); Apply → KAuth `org.kde.powerdevil.chargethresholdhelper.setthreshold` → root helper writes `charge_control_start/end_threshold` raw, any 0–100, start≤end, available lists NOT consulted (`daemon/chargethresholdhelper_linux.cpp` `setThresholds`/`setthreshold`); 75/85 fully expressible
CHARGE-TYPE-WRITER: none in KDE path — KDE helper and upower's `EnableChargeThreshold` both write Dell-style `charge_types` (plural) only when that attr exists; clevo_keyboard exposes `charge_type` (singular) → skipped silently (helper `setChargeType`: "ignore those" → continue; upower `up_device_supply_battery_set_battery_charge_types` early-TRUE if attr absent). Nothing in powerdevil/upower can set Custom on this driver
BREAKAGE: `/etc/udev/rules.d/60-clevo-flexicharger-restore.rules` runs `flexicharger-restore` (start=75 end=85 type=Custom from `/etc/clevo-flexicharger.conf`) on every BAT0 add|change event — measured 968 BAT0 change events / 979 helper runs in a natural 92 s window (~10.5/s; 591,354 runs today 03:33:21→16:59:16) — so every KCM/KDE write of other values is overwritten back to 75/85/Custom within well under a second; that is why the KDE charge-limit UI "does nothing"
MINIMAL-REMOVAL: delete (or rename out of rules.d) `/etc/udev/rules.d/60-clevo-flexicharger-restore.rules` and delete `/etc/clevo-flexicharger.conf` + `/usr/lib/clevo-keyboard-dkms/flexicharger-restore` — three files, all owned by the `clevo-keyboard-dkms` package, so the durable route is removing them from the DKMS payload and reinstalling (package reinstall/upgrade re-arms on-disk deletions); no systemd unit or timer exists for this (verified); leaving the conf with the rule gone is inert (helper only runs via the rule), but the conf must not survive a package reinstall if KDE is to be the only writer

## 1. Versions

```
$ dpkg -l powerdevil upower plasma-workspace
ii  plasma-workspace 4:6.7.4-1  amd64
ii  powerdevil       4:6.7.4-1  amd64
ii  upower           1.91.3-1   amd64
$ upower --version
UPower client version 1.91.3 / UPower daemon version 1.91.3
```

upowerd running since 15:59:20 (restarted after boot 03:33:16).

## 2. D-Bus API introspection

`busctl --system introspect org.freedesktop.UPower /org/freedesktop/UPower/devices/battery_BAT0`
(property names exactly as exposed; the prompt's guessed names `ChargeControlSupported`/
`ChargeControlEnabled` do not exist):

- Method: `EnableChargeThreshold(b)` — the ONLY setter on the interface.
- Properties (XML `access="read"` for all five):
  - `ChargeThresholdSupported = true`
  - `ChargeThresholdSettingsSupported = u 3` (bit mask: 1=start + 2=end; both sysfs attrs exist)
  - `ChargeThresholdEnabled = false`
  - `ChargeStartThreshold = u 75`
  - `ChargeEndThreshold = u 80`  ← 80, not the live sysfs 85

Root object `org.freedesktop.UPower`: no charge config — only `GetCriticalAction`,
`DaemonVersion="1.91.3"`, `LidIsClosed/Present`, `OnBattery`. `/etc/UPower/UPower.conf` is the
vendor default and contains no charge-threshold keys (no such keys exist in 1.91.3).

Where 75/80 comes from (source-verified, upower 1.91.3 tarball from snapshot.debian.org):

- upower does NOT read live sysfs thresholds for these properties. `60-upower-battery.rules`
  IMPORTs a hwdb property: `up_device_supply_battery_get_charge_control_limits()` parses the
  udev property `CHARGE_LIMIT` ("start,end"; src/linux/up-device-supply-battery.c:169-204).
- `/usr/lib/udev/hwdb.d/60-upower-battery.hwdb` ends with wildcard `battery:*:*:dmi:* →
  CHARGE_LIMIT=75,80`. md5sum equals the upstream orig tarball (534f704a418287cf4e36c021020a7e2b)
  → shipped by upower itself, not a local edit. `udevadm info --export-db` confirms
  `E: CHARGE_LIMIT=75,80` on BAT0.
- Consequences: D-Bus Start/End are a static hwdb pair (75,80) for every battery, never the live
  values; `End=80 vs sysfs=85` is stale by design. `ChargeThresholdEnabled=false` only means
  `EnableChargeThreshold(true)` was never called (state file absent) — it is not a readout of
  `charge_type`.
- upower 1.91.3 contains NO reference to `charge_control_*_available_thresholds` anywhere in
  `src/` → no clamp/refuse of 75 or 85 exists; the driver's legacy available lists are irrelevant
  to upower.

## 3. Persistence / re-arm

- `/var/lib/upower/`: only `history-*.dat`; no `charging-threshold-status`.
  `EnableChargeThreshold` writes `/var/lib/upower/charging-threshold-status` ("1"/"0", 0640;
  STATE_DIR=/var/lib/upower per meson). At coldplug,
  `up_device_battery_recover_battery_charging_threshold()` re-applies the hwdb thresholds iff that
  file reads "1" (up-device-battery.c:433-475). File absent → **upowerd applies nothing at
  daemon start** (verified listing).
- KDE configs carry NO threshold keys: `~/.config/powerdevilrc` → only
  `[BatteryManagement] BatteryLowLevel=25` under profile groups; `~/.config/powermanagementprofilesrc`
  → profiles + `[Migration]`; `kded6rc` no charge keys.
- powerdevil daemon at login calls `readChargeThreshold()` = KAuth `getthreshold` (READ only;
  powerdevilcore.cpp:75, 1005-1031). It re-reads on battery charge events. Nothing in powerdevil
  writes thresholds at login, profile switch, lid or AC events.
- KCM (`ExternalServiceSettings::load/save`, kcm/ExternalServiceSettings.cpp:78-140) reads live via
  helper `getthreshold` on open and writes via `setthreshold` only on Apply-diff. No KDE-side
  persistence: the EC/driver state is the only store.

## 4. KDE start-threshold verdict

`KDE-START-THRESHOLD: YES` (marker above). Means: KCM spinbox → KAuth `setthreshold` → helper
writes `charge_control_start_threshold` with any 0–100 (validation is 0–100 and start≤end only;
the available-lists are never read — `chargethresholdhelper_linux.cpp`). The KDE path does not
touch upower for this at all; the upower D-Bus `ChargeStartThreshold` exists but is read-only.

## 5. charge_type writer

`CHARGE-TYPE-WRITER: none in KDE path` (marker above). Mechanism: driver `charge_type_store`
(`/usr/src/clevo-keyboard-4.6.2+diamon2/src/clevo_keyboard.h:800-824`) writes the flexicharger
status byte via `clevo_flexicharger_write(NULL, NULL, &status)`; on cc4 this touches live EC
registers only (no NVRAM persist; header comment lines 733-745). EC keeps Custom once set in
session — probe2 A/B: with the udev rule parked, a hand-written `Standard` stayed stable for the
whole 40 s phase window (`/tmp/opencode/charge-probe2-163216.log`, 16:33:16→16:33:56); the only
spontaneous source of `Custom` is the EC NVRAM profile at cold boot / 0x46 reload (driver
preserve logic re-applies live registers around 0x46, lines 747-780).

## 6. Polkit

- `org.freedesktop.UPower.enable-charging-limit` (`org.freedesktop.upower.policy`):
  allow_inactive=no, allow_active=yes → the active local GUI session (`loginctl`: Type=wayland,
  Class=user, **Active=yes**) can call `EnableChargeThreshold` with no prompt. Action id confirmed
  in daemon source (up-device.c:415).
- `org.kde.powerdevil.chargethresholdhelper.*` (KDE policy file): `setthreshold` and
  `setconservationmode` = allow_inactive=no, allow_active=`auth_admin_keep` (admin password,
  cached); `getthreshold`/`getconservationmode` = yes/yes → UI reads are silent.
- Overrides: `/etc/polkit-1/rules.d` is root-only (`ls`: Permission denied) — unverifiable without
  sudo. `/usr/share/polkit-1/rules.d/org.freedesktop.upower.rules` covers only login1
  inhibit actions. Helper runs as root via D-Bus activation (`system-services/…`, User=root).

## 7. Transport proof (sanctioned act 1)

```
$ busctl --system set-property org.freedesktop.UPower /org/freedesktop/UPower/devices/battery_BAT0 \
    org.freedesktop.UPower.Device ChargeStartThreshold u 75
Failed to set property ChargeStartThreshold on interface org.freedesktop.UPower.Device: Property “ChargeStartThreshold” is not writable   # rc=1
$ busctl --system set-property … ChargeEndThreshold u 85
Failed to set property ChargeEndThreshold …: Property “ChargeEndThreshold” is not writable   # rc=1
```

Target values equal the live sysfs state, so success would have been a no-op against the EC; the
daemon rejects before any sysfs access (read-only vtable). `EnableChargeThreshold(true)` was
deliberately NOT called: it takes no values and would write the hwdb pair 75/80 — end 80 ≠ live
85 → a real change, not a no-op (polkit would have allowed it: allow_active=yes). KDE's own KAuth
write was not exercised from this shell: it performs direct sysfs writes (outside the sanctioned
D-Bus read/no-op set) and needs an admin-auth prompt. Conclusion: upower 1.91.3 exposes NO
per-value set interface; the only write is a fixed 75/80 toggle.

## 8. Helper-clobber observation (sanctioned act 2)

Window 16:57:04–16:58:36 (92 s), no `udevadm trigger`, no sysfs writes, only natural activity:

- `udevadm monitor --udev -s power_supply`: 968 BAT0 events (~10.5/s natural).
- `journalctl --since <window> | grep -c flexicharger-restore`: 979 runs (~10.6/s).
- 46 attr polls at 2 s cadence: `type=Custom start=75 end=85` in every sample — the helper
  rewrote identical values each run (expected side-effect-free: driver comment "a rewrite of
  matching values was verified side-effect free", clevo_keyboard.h:768-773).
- Rate over the day: 591,354 runs 03:33:21→16:59:16 (`journalctl --since today | grep -c`);
  1,770 runs in 16:26:00–16:28:00 (matches probe-1 clobber era cited by manager).

## 9. BREAKAGE — ranked candidates

`BREAKAGE:` marker above = the single best mechanism (a). Ranked:

a. PRIMARY — helper re-imposition loop. The DKMS-owned rule fires `flexicharger-restore` on every
   BAT0 add|change; events occur ~10.5/s naturally (§8), so any KDE-written start/end is reverted
   to the conf's 75/85/Custom within ≪1 s. User-visible: KCM Apply "works" (auth, no error),
   values flip back instantly → "does nothing". A/B: rule parked → handwritten `Standard`
   stable 40 s (probe2 log); rule active → 5 runs at 16:26:54 ×5 / 1,770 in 2 min (probe-1 era).
b. upower clamp/refuse vs `charge_control_*_available_thresholds` — DEAD. upower 1.91.3 source
   references those attrs nowhere; D-Bus threshold values come from the
   hwdb `CHARGE_LIMIT=75,80` wildcard and the legacy lists' omission of 75/85 has no effect.
   (KDE's helper likewise never reads the lists; it clamps 0–100 only.)
c. slider bound to a missing upower property / present-but-inert UI — DEAD for the KDE UI: the
   KCM spinboxes are bound to the KAuth helper path, which writes real sysfs files directly
   (source §4). TRUE variant for upower consumers only: clients (e.g. GNOME-style toggles) see a
   false `ChargeThresholdEnabled` and a phantom `End=80`, and calling `EnableChargeThreshold(true)`
   would actively downgrade end 85→80. The KDE UI is not inert — it is overwritten.

## 10. MINIMAL-REMOVAL — details behind the marker line

Remove/disable exactly (dpkg -S attribution in parentheses):

1. `/etc/udev/rules.d/60-clevo-flexicharger-restore.rules` (clevo-keyboard-dkms) — the trigger;
   removing it alone stops all re-application. Reload needs `udevadm control --reload` (root;
   manager's step).
2. `/etc/clevo-flexicharger.conf` (clevo-keyboard-dkms) — the payload values. Inert once the rule
   is gone (helper reads it only when invoked by the rule), but delete it or a reinstall confuses
   future debugging.
3. `/usr/lib/clevo-keyboard-dkms/flexicharger-restore` (clevo-keyboard-dkms) — the writer.

No systemd unit, timer, or other udev rule writes these attrs (verified: `systemctl
list-unit-files|list-timers` grep empty; `/etc/udev/rules.d` scan — only this rule touches
charge attrs; `60-upower-battery.rules` only IMPORTs a hwdb property; `30-clevo_keyboard.rules`
is LED permissions from `clevo-keyboard-controller`). Durable route: remove the three files from
the clevo-keyboard-dkms payload source and reinstall; on-disk deletion is re-armed by any package
reinstall/upgrade.

Implications:

- Boot-time re-arm: with the trio gone, the only writer left at clevo-stack (re)load is the EC
  itself (0x46 NVRAM restore, driver comment clevo_keyboard.h:733-745) plus the driver's
  preserve logic which re-applies the pre-revert live registers on cc4 (lines 747-780). In-session
  reloads therefore keep KDE's values; a cold boot starts from the EC NVRAM profile, which is
  outside KDE control and currently unverified (nothing in the KDE/upower path writes NVRAM on
  cc4 — live registers only).
- charge_type: per §5, KDE never writes `Custom`; the plan keeps `Custom` by design (current live
  state persists in-session — probe2 A/B — and is re-applied by the driver across 0x46). Nothing
  must write `Standard`. If a cold boot ever comes up `Standard`, KDE cannot revive `Custom` —
  that is a residual gap to accept or to cover with a once-per-boot non-udev mechanism (out of
  scope here).
- upower: no change needed — it neither applies nor persists anything on this machine today
  (state file absent); its readout stays cosmetically wrong (Enabled=false, End=80) regardless.

Evidence scratch (not committed): /tmp/opencode/d2-udev-mon.log, /tmp/opencode/d2-attr-poll.log,
/tmp/opencode/upower-src/ (upower-1.91.3 orig), /tmp/opencode/pd-src/ (powerdevil-6.7.4 orig),
both fetched from snapshot.debian.org and hash-matched to the installed packages.

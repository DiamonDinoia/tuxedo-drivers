# clevo-keyboard (Juno fork of tuxedo-drivers)

The `clevo_keyboard`, `clevo_wmi` and `clevo_acpi` modules from
[tuxedo-drivers](https://github.com/tuxedocomputers/tuxedo-drivers) v4.22.3, renamed from
`tuxedo_keyboard` and patched for a Tongfang V5xTNC_TND_TNE (Juno) with a cc4 flexicharger:
the TUXEDO compatibility gate is disabled, charge thresholds pass through as raw percents, the
live thresholds survive `CLEVO_CMD_SET_EVENTS_ENABLED` (module load, resume), and threshold
writes emit a `power_supply` change uevent. The other upstream sources are vendored but not built.
Patch list and provenance: `juno/PATCHES.md`.

```sh
make                # modules in src/, against /lib/modules/$(uname -r)/build
make test           # host-only checks, no EC access (juno/selftest.sh)
sudo dkms add .     # DKMS package clevo-keyboard, version from dkms.conf
```

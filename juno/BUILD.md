# BUILD.md — build / smoke-load / revert runbook (clevo trio)

Host: siduction `laptop`, kernel `7.2.2-1-siduction-amd64`. The build and
`make test` touch no hardware; only the load/revert steps touch the EC,
and always through `juno/RELOAD.sh`.

## 1. Build and host checks

```sh
nice -n19 ionice -c3 taskset -c 0,5-21 make -j4
nice -n19 ionice -c3 taskset -c 0,5-21 make test
```

`make` builds the three modules of the Makefile's `obj-m` list
(`clevo_keyboard.ko`, `clevo_wmi.ko`, `clevo_acpi.ko`) into `src/` against
`/lib/modules/$(uname -r)/build`. `make test` runs `juno/selftest.sh`: the
flexicharger code against a fake EC, and a warning-free rebuild of the trio.

Container build (no host toolchain needed):

```sh
podman run --rm \
  -v /lib/modules/$(uname -r)/build:/kbuild-ro:ro \
  -v "$PWD":/work -w /work debian:sid bash -c '
    apt-get update -qq &&
    apt-get install -y -qq --no-install-recommends make gcc libelf1 &&
    make KDIR=/kbuild-ro'
```

## 2. clevofan is not part of --modules-dir

The Makefile builds exactly three modules, and `RELOAD.sh
--modules-dir DIR` requires only those three load-order `.ko` in DIR
(exit 2 otherwise): `clevofan` comes from the DKMS-installed
juno-drivers package and is never read from DIR. If clevofan is
currently loaded, RELOAD.sh unloads and reloads it via modprobe on its
own; no staging step is needed.

## 3. Smoke-load the patched trio

```sh
sudo -A bash juno/RELOAD.sh --modules-dir "$PWD/src" 95 100
```

Writes 95/100 to the battery's `charge_control_*_threshold` sysfs,
removes `clevo_acpi clevo_wmi clevo_keyboard` (and `clevofan` via
modprobe, if it was loaded), insmods `clevo_keyboard clevo_wmi
clevo_acpi` from the given dir (the patched trio; clevofan, if
reloaded, comes back via modprobe, not from the dir), and prints
`key=value` witness lines (thresholds before/after, `charge_type`,
srcversions). Fan daemons are stopped first and restarted by an EXIT
trap even on failure.

Witness semantics that later rounds rely on:

- exit 2  usage/precondition (missing `.ko` in the dir, no battery
  attrs — i.e. clevo stack not loaded at entry).
- exit 3  **clevo-absent witness violation**: a `^clevo` module survived
  the removal phase, so the swap is partial and nothing new was loaded.
  The surviving modules are listed on stderr; daemons still restart.
- exit 4  `modprobe -r`/`insmod` failure.

## 4. Restore stock

```sh
sudo -A bash juno/RELOAD.sh 95 100
```

Same procedure with plain `modprobe` (DKMS-installed modules), no
`--modules-dir`.

## 5. Which build is resident: lsmod size, not srcversion

The srcversion witness is vacuous on this kernel — it does not
discriminate the stock build from the patched one. Use the `lsmod` size
column on `clevo_keyboard` instead (attempt-1 host runs, this kernel):

| build | `lsmod` size |
|---|---|
| stock | 69632 |
| patched (S2b) | 73728 |

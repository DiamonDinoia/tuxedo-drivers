# BUILD.md — build / smoke-load / revert runbook (clevo trio)

Host: siduction `laptop`, kernel `7.2.2-1-siduction-amd64`. Everything
builds in a container; only the load/revert steps touch the host, and
always through `juno/RELOAD.sh`.

## 1. Build (container, canonical)

```sh
bash juno/container-check.sh
```

Runs `podman` (falls back to `docker`) with image `debian:sid`, the
host's running-kernel headers bind-mounted read-only
(`/lib/modules/$(uname -r)/build` → `/kbuild-ro:ro`), and the compiler
taken from the kernel's `auto.conf` (`CONFIG_CC_VERSION_TEXT`, gcc-15 on
this host). It builds the patched tree (HEAD) and the pre-patch base,
checks no new W=1/sparse warnings, verifies `vermagic`, and copies the
three validated modules (`clevo_keyboard.ko`, `clevo_wmi.ko`,
`clevo_acpi.ko` — the Makefile's whole `obj-m` list) into `src/`. Run
record of the S2b round: `juno/EVIDENCE-s2b-container.md`. If a build
against the read-only headers fails, the script retries against a
writable copy made inside the container.

Bare manual equivalent (no validation, no copy-back):

```sh
podman run --rm \
  -v /lib/modules/$(uname -r)/build:/kbuild-ro:ro \
  -v "$PWD":/work -w /work debian:sid bash -c '
    apt-get update -qq &&
    apt-get install -y -qq --no-install-recommends make gcc libelf1 &&
    make KDIR=/kbuild-ro'
```

## 2. Stage clevofan.ko (prerequisite for --modules-dir)

The Makefile builds exactly three modules, but `RELOAD.sh
--modules-dir DIR` requires all four load-order `.ko` in DIR (exit 2
otherwise): `clevofan` comes from the DKMS-installed juno-drivers
package, not from this tree. Stage it once per kernel:

```sh
zstd -d < /lib/modules/$(uname -r)/updates/dkms/clevofan.ko.zst > src/clevofan.ko
```

(`src/clevofan.ko` inside the tree is worktree-local residue: delete it
after the witnessed run; never commit it. `*.ko` is gitignored.)

## 3. Smoke-load the patched trio

```sh
sudo -A bash juno/RELOAD.sh --modules-dir "$PWD/src" 95 100
```

Writes 95/100 to the battery's `charge_control_*_threshold` sysfs,
removes `clevofan clevo_acpi clevo_wmi clevo_keyboard`, insmods
`clevo_keyboard clevo_wmi clevo_acpi clevofan` from the given dir (the
patched trio plus staged clevofan), and prints
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

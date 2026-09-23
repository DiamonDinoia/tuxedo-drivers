# EVIDENCE-s4: clevo-keyboard 4.6.2+diamon1 DKMS-installed for all headered kernels

Machine: laptop (siduction), 2026-09-05 04:50–04:56 EDT, running kernel
7.2.2-1-siduction-amd64, dkms 3.4.1, gcc-15.3.0-3 host toolchain.

Scope: replace the DKMS-registered `clevo-keyboard/4.6.2` (stock payload
copy) by `clevo-keyboard/4.6.2+diamon1` built from the patched tree
(commit 8d62d12, patch 762b596) on all kernels under /lib/modules that
carry a build/ link. 7.1.10-1-siduction-amd64 is headerless (no build
link) and is skipped; 7.1.12+deb14-amd64, 7.2.2-1-siduction-amd64 and
7.2.3-1-siduction-amd64 are the three headered kernels.

## 1. Source export and provenance check

```
git -C ~/repos/juno/tuxedo-keyboard archive \
    --prefix=clevo-keyboard-4.6.2+diamon1/ \
    -o /tmp/opencode/s4-ck-4.6.2+diamon1.tar 8d62d12
sha256sum: fb922c6d5ca5405027979dd46852e1faf8652a33be63231e4edbc8955e182e6e  (84 entries)
sudo -A tar -x -C /usr/src -f /tmp/opencode/s4-ck-4.6.2+diamon1.tar
test -f /usr/src/clevo-keyboard-4.6.2+diamon1/dkms.conf          -> OK (dkms.conf at root)
sudo -A diff -r /usr/src/clevo-keyboard-4.6.2+diamon1 <user-side re-extract of the same tar>
    -> SOURCE-TREE-IDENTICAL-TO-ARCHIVE
```

The DKMS source tree is bit-identical to `git archive 8d62d12` — the same
archive form the S2b container harness consumed, so the compiled inputs
are exactly what S2b validated.

## 2. dkms add + build (log: /tmp/opencode/s4-dkms-add-build.log)

```
dkms add clevo-keyboard/4.6.2+diamon1
dkms build clevo-keyboard/4.6.2+diamon1 -k {7.1.12+deb14-amd64,7.2.2-1-siduction-amd64,7.2.3-1-siduction-amd64}
```

All three builds `done`; "Building module(s)" succeeded per kernel; the
7.1.12 kernel additionally signs the three modules with the host MOK.
Compiler note from the siduction-kernel make logs (both):

```
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: gcc-15 (Debian 15.3.0-2) 15.3.0
  You are using:           gcc-15 (Debian 15.3.0-3) 15.3.0
```

(7.1.12+deb14 carries the same 15.3.0-2 in its CONFIG_CC_VERSION_TEXT and
prints no warning.) Revision-level difference only; vermagic carries no
compiler field, and D2 below load-proves the artifacts.

## 3. remove-before-install instead of install-before-remove

dkms 3.x ignores DEST_MODULE_LOCATION and always installs to
`updates/dkms/` — the old 4.6.2 and the new 4.6.2+diamon1 target the
identical file paths (`updates/dkms/clevo_{keyboard,wmi,acpi}.ko.*`).
Removing 4.6.2 *after* installing 4.6.2+diamon1 would have deleted the
new files (remove emits plain "Deleting <path>" for those paths; see
transcript below, which shows precisely those paths being removed while
they still belonged to 4.6.2). Order executed:

```
dkms remove clevo-keyboard/4.6.2 --all
dkms install clevo-keyboard/4.6.2+diamon1 -k <each of the 3 kernels>
```

Log: /tmp/opencode/s4-dkms-remove-install.log. Removal deleted the three
old module files per kernel and ran depmod per kernel; install wrote the
three new .ko.zst (7.1.12: .ko.xz) files and ran depmod per kernel.

Invariant check (verbatim from the batch):

```
distinct_clevo-keyboard_versions=1
```

Final `dkms status` (verbatim):

```
clevo-keyboard/4.6.2+diamon1, 7.1.12+deb14-amd64, x86_64: installed
clevo-keyboard/4.6.2+diamon1, 7.2.2-1-siduction-amd64, x86_64: installed
clevo-keyboard/4.6.2+diamon1, 7.2.3-1-siduction-amd64, x86_64: installed
clevofan/2.4, 7.1.12+deb14-amd64, x86_64: installed
clevofan/2.4, 7.2.2-1-siduction-amd64, x86_64: installed
clevofan/2.4, 7.2.3-1-siduction-amd64, x86_64: installed
nvidia/610.57.04, 7.1.12+deb14-amd64, x86_64: installed
nvidia/610.57.04, 7.2.2-1-siduction-amd64, x86_64: installed
nvidia/610.57.04, 7.2.3-1-siduction-amd64, x86_64: installed
```

The orphan /usr/src/clevo-keyboard-4.6.2 is deliberately left in place
(juno deb prerm consumes it later).

## 4. sha256 of the installed artifacts (decompressed)

| kernel | module | sha256 |
|---|---|---|
| 7.1.12+deb14-amd64 | clevo_keyboard | 482dd34a91110414c060d267c62e53988a6595fd4a82c25a7fd5740fa649ac30 |
| 7.1.12+deb14-amd64 | clevo_wmi | 2d9150f2e759953c34b6109f296f7926df0861615edb508de9bf46b733e26f93 |
| 7.1.12+deb14-amd64 | clevo_acpi | d492e851e27d5684041b4ea76d2f076fc9b64015c85b8fb6f0819d5e266cd673 |
| 7.2.2-1-siduction-amd64 | clevo_keyboard | 7297de94afeaa5cbecd5c08f560507c564926db26dd62f2aac13688288ac8ad9 |
| 7.2.2-1-siduction-amd64 | clevo_wmi | 0e59d2f86ed094d99c8327d6dd2d5d2e7c7fee1ac0cdab754997ce25707e0113 |
| 7.2.2-1-siduction-amd64 | clevo_acpi | ebd4ecbfae29ada2b7c78051caddf10e1b64b13135380d2d4f5b692a4303aa9e |
| 7.2.3-1-siduction-amd64 | clevo_keyboard | 2ff2a54905b15d4b0f7b52744a208e729b143d445e32ab10c47bc4035e894395 |
| 7.2.3-1-siduction-amd64 | clevo_wmi | fb835a1a0436953b729a1a7b9300f69353ea885a160ccd4dda7feb76e3d89936 |
| 7.2.3-1-siduction-amd64 | clevo_acpi | 4272c6fc970c51772d9925fdf4044e96b9c1e55f09446bad386f4d7b379031ed |

modinfo of the running kernel's installed clevo_keyboard (decompressed):
`vermagic=7.2.2-1-siduction-amd64 SMP preempt mod_unload`,
`srcversion=(absent)`.

**Gap report, not silent skip:** the brief's step 3 presumes
the S2b container run record carries a reference sha256 of the S2b
container-built artifact. It does not — the file records only the
container image_id; the container workdir was trap-deleted and the
validated .ko copies in the worktree src/ were cleaned after the S2b
witnessed run (BUILD.md convention). The table above therefore
establishes the first on-record sha256 references for the patched line.
Bit-identity between these host builds and the S2b container artifacts
was never testable even in principle: the kernel header config sets
CONFIG_DEBUG_INFO=y (zstd DWARF5 + BTF, both embedding build-directory
paths that differ between /work/patched and the dkms build tree) and the
container used gcc-15 while the kernel/build hosts differ at gcc
revision (15.3.0-2 vs 15.3.0-3), so identical input sources cannot
yield identical .ko bytes across the two environments. Verification is
instead at the levels that can be carried: (a) source-tree bit-identity
to the exact S2b-validated archive (section 1), (b) per-module vermagic
match to each target kernel, (c) the behavioral proof of section 5.

## 5. Witnessed D2 re-run (modules now from modprobe/DKMS)

Pre-state: thresholds 70/80 (NVRAM profile), fancontrol.service active.

```
sudo -A bash juno/RELOAD.sh 95 100        (cwd: main checkout @ 8d62d12)
```

Transcript (verbatim, log /tmp/opencode/s4-d2-reload.log, rc=0):

```
modules_source=modprobe
pre_write_start=70
pre_write_end=80
wrote_start=95
wrote_end=100
after_write_start=95
after_write_end=100
after_reload_start=95
after_reload_end=100
after_reload_charge_type=Custom
module_clevo_keyboard_srcversion=(absent)
module_clevo_wmi_srcversion=(absent)
module_clevo_acpi_srcversion=(absent)
module_clevofan_srcversion=964DB774E8E4FB6D53C102C
```

State-file D2 check (the threshold files, i.e. the check minus its
vacuous srcversion arm):

```
charge_control_start_threshold=95
charge_control_end_threshold=100
charge_type=Custom
```

With stock modules this reload reverts 95/100 to the NVRAM 70/80 before
the first post-reload read (S2a mechanism, cycles 1-2); a kept 95/100
with modules_source=modprobe is the behavioral proof that modprobe
loaded the patched modules. The srcversion arm is vacuous by
construction: patch 762b596 touches only `src/clevo_keyboard.h` and
modpost's srcversion hashes the .c member files only (the in-kernel
srcversion sysfs files are additionally absent on this config).

Non-vacuous build witness (BUILD.md's lsmod-size check):

| | clevo_keyboard size |
|---|---|
| stock 4.6.2 (pre-run lsmod) | 69632 |
| 4.6.2+diamon1 (post-run lsmod) | 73728 |

Daemon restoration (RELOAD.sh internal stop/start): fancontrol.service
`active` after the run; the tray monitor was restarted
(`juno-fan-monitor-reload.service` Started 04:52:50, `tray.py` pid
2724531 running; journal shows the previous transient instance from the
S2b witnessed run being Stopped at 04:52:49 first).

## 6. D4 check — regex defect in the brief's form, fixed form passes

Brief's command, verbatim:

```
v=4.6.2+diamon1; for k in /lib/modules/*/; do [ -e "$k/build" ] || continue; \
  dkms status | grep -qE "clevo-keyboard/$v.*$(basename $k).*installed" || exit 1; done
```

Result: exit 1 on the first headered kernel even in the fully correct
final state (instrumented transcript: FAIL for all three). Defect: in
ERE, `+` is a quantifier, so `4.6.2+diamon1` matches `4.6.22…` with one
or more `2`s and never a literal `+`; the same defect is in the kernel
substitution for `7.1.12+deb14-amd64`. Positive control on one status
line: `grep -F 'clevo-keyboard/4.6.2+diamon1'` and
`grep -E 'clevo-keyboard/4\.6\.2\+diamon1'` match, unescaped
`grep -E …4.6.2+diamon1…` does not (GNU grep 3.12). Content itself is
healthy (the same seconds captured 9 status lines, 3 matching
clevo-keyboard). Fixed form (`esc() { sed 's/[+.]/\\&/g'; }` applied to
both substitutions):

```
D4 PASS 7.1.12+deb14-amd64
D4 PASS 7.2.2-1-siduction-amd64
D4 PASS 7.2.3-1-siduction-amd64
D4-fixed rc=0
```

`dkms status` itself was re-run repeatedly (12 rapid invocations) with
no instability; no dkms processes/locks linger.

## 7. Done condition

- `dkms status`: clevo-keyboard/4.6.2+diamon1 installed for all three
  headered kernels; exactly one clevo-keyboard version registered.
- RELOAD key=value: `after_reload_start=95 after_reload_end=100` with
  `modules_source=modprobe`.
- Deliverable: this file on branch team/4-dkms-deb-packager (only file
  changed); host state (DKMS tree, /usr/src/clevo-keyboard-4.6.2+diamon1)
  is out-of-tree by design.

Raw logs (not committed): /tmp/opencode/s4-dkms-add-build.log,
s4-dkms-remove-install.log, s4-sha256.log, s4-d2-reload.log,
s4-d4-corrected.txt; privileged batch scripts
/tmp/opencode/s4-priv-{1,2}.sh.

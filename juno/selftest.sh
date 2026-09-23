#!/bin/bash
# juno/selftest.sh -- host-only checks, no EC access, no root. Usage: make test
#   flexicharger  src/clevo_keyboard.h flexicharger code against a fake EC
#                 (juno/flexicharger-test.c), built with ASan/UBSan
#   build         make -j2 of the module trio against KDIR (default: running
#                 kernel) exits 0 with no warning line; kbuild's "compiler
#                 differs" note is host state, filtered and printed
set -uo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
RC=0

H=$REPO/src/clevo_keyboard.h
{
	sed -n '/^int clevo_evaluate_method2(/,/^EXPORT_SYMBOL(clevo_evaluate_method);/p' "$H"
	sed -n '/^static u8 clevo_legacy_flexicharger_start_values/,/^static struct acpi_battery_hook battery_hook/p' "$H" | sed '$d'
} >"$T/flexicharger-extract.h"
if gcc -std=gnu11 -Wall -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all \
	-I"$T" -o "$T/flexicharger-test" "$REPO/juno/flexicharger-test.c" &&
	"$T/flexicharger-test"; then
	echo "CHECK flexicharger PASS"
else
	echo "CHECK flexicharger FAIL"
	RC=1
fi

mkdir "$T/build"
cp -r "$REPO/Makefile" "$REPO/src" "$T/build/"
find "$T/build/src" \( -name '*.ko' -o -name '*.o' -o -name '*.mod*' -o -name '.*.cmd' \) -delete
(cd "$T/build" && env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL make -j2) >"$T/build.log" 2>&1
st=$?
for m in clevo_keyboard clevo_wmi clevo_acpi; do
	[ -f "$T/build/src/$m.ko" ] || st="$st, missing $m.ko"
done
grep -A2 'warning: the compiler differs' "$T/build.log" | sed 's/^/  filtered: /'
warn=$(grep 'warning' "$T/build.log" | grep -v 'warning: the compiler differs')
if [ "$st" = 0 ] && [ -z "$warn" ]; then
	echo "CHECK build PASS (exit 0, clevo_keyboard/clevo_wmi/clevo_acpi .ko, no warnings)"
else
	echo "CHECK build FAIL (exit $st)"
	cat "$T/build.log"
	RC=1
fi

exit $RC

#!/bin/bash
# juno/RELOAD.sh -- canonical full-stack clevo module reload
# (team round 2 S2a)
#
# Usage: sudo -A bash juno/RELOAD.sh [--modules-dir DIR] [START END]
#
#   --modules-dir DIR   insmod clevo_keyboard.ko clevo_wmi.ko clevo_acpi.ko
#                       from DIR (all three must exist there). clevofan is
#                       a separate package (clevofan-dkms); if it is
#                       currently loaded it is removed/reloaded via
#                       modprobe regardless of this flag.
#                       Default: plain modprobe (DKMS-installed modules).
#   START END           charge thresholds written to sysfs before the
#                       reload (default: restore the values read before
#                       unload, i.e. a no-op).
#
# Steps: stop fan daemons, write START/END, remove (if loaded) clevofan then
# clevo_acpi/clevo_wmi/clevo_keyboard, verify no ^clevo module remains
# (exit 3 otherwise), load clevo_keyboard first then clevo_wmi, clevo_acpi,
# then clevofan if it was loaded, settle, print the resulting thresholds,
# charge_type and loaded-module srcversions as key=value lines, then
# restart the daemons. Daemons are also restarted on failure (trap).
#
# Exit codes: 2 usage/precondition, 3 clevo module survived removal,
# 4 module remove/load failure, 5 sysfs write/read failure,
# 6 daemon restart failure, 7 lock held.

set -euo pipefail
export LANG=C

MODULES_DIR=""
START=""
END=""
THRESHOLDS_GIVEN=0

while [ $# -gt 0 ]; do
	case "$1" in
	--modules-dir)
		[ $# -ge 2 ] || { echo "usage: --modules-dir DIR" >&2; exit 2; }
		MODULES_DIR=$2
		shift 2
		;;
	-h | --help)
		sed -n '2,20p' "$0"
		exit 0
		;;
	*)
		if [ $# -eq 2 ]; then
			START=$1
			END=$2
			THRESHOLDS_GIVEN=1
			shift 2
		else
			echo "usage: $0 [--modules-dir DIR] [START END]" >&2
			exit 2
		fi
		;;
	esac
done

if [ "$THRESHOLDS_GIVEN" = 1 ]; then
	case "$START" in
	'' | *[!0-9]*)
		echo "START must be an integer 1..100 (got '$START')" >&2
		exit 2
		;;
	esac
	case "$END" in
	'' | *[!0-9]*)
		echo "END must be an integer 1..100 (got '$END')" >&2
		exit 2
		;;
	esac
	if [ "$START" -lt 1 ] || [ "$START" -gt 100 ] || [ "$END" -lt 1 ] || [ "$END" -gt 100 ]; then
		echo "START/END must be 1..100 (got '$START' '$END')" >&2
		exit 2
	fi
	if [ "$START" -ge "$END" ]; then
		echo "START must be less than END (got '$START' '$END')" >&2
		exit 2
	fi
fi

if [ "$(id -u)" -ne 0 ]; then
	echo "must run as root (sudo -A bash juno/RELOAD.sh ...)" >&2
	exit 2
fi

exec 9>/run/lock/juno-reload.lock
flock -n 9 || {
	echo "another RELOAD.sh instance holds /run/lock/juno-reload.lock" >&2
	exit 7
}

REMOVE_ORDER="clevo_acpi clevo_wmi clevo_keyboard"
LOAD_ORDER="clevo_keyboard clevo_wmi clevo_acpi"

if [ -n "$MODULES_DIR" ]; then
	for m in $LOAD_ORDER; do
		[ -f "$MODULES_DIR/$m.ko" ] || {
			echo "missing $MODULES_DIR/$m.ko" >&2
			exit 2
		}
	done
fi

# clevofan is a separate DKMS package, never built in this tree; it is
# only touched if it is currently loaded, and always via modprobe.
CLEVOFAN_LOADED=0
if lsmod | grep '^clevofan' >/dev/null; then
	CLEVOFAN_LOADED=1
fi

BAT=""
for b in /sys/class/power_supply/BAT*; do
	if [ -e "$b/charge_control_start_threshold" ] && [ -e "$b/charge_control_end_threshold" ]; then
		BAT=$b
		break
	fi
done
[ -n "$BAT" ] || {
	echo "no battery with charge_control_* found (is the clevo stack loaded?)" >&2
	exit 2
}

# --- daemon stop / restart ---------------------------------------------

TRAY_PROC_RE='(^|/)juno-fan-monitor( |$)|/usr/lib/juno-kde-fancontrol/tray\.py'
TRAY_USER=""
TRAY_STOPPED=0
FANCONTROL_STOPPED=0

stop_daemons()
{
	if systemctl is-active --quiet fancontrol.service; then
		systemctl stop fancontrol.service
		FANCONTROL_STOPPED=1
	fi
	systemctl stop tccd.service 2>/dev/null || true

	local pid uid
	pid=$(pgrep -f "$TRAY_PROC_RE" | head -1 || true)
	if [ -n "$pid" ]; then
		uid=$(ps -o uid= -p "$pid" | tr -d ' ')
		TRAY_USER=$(id -nu "$uid")
		local unit
		unit=$(systemctl --user -M "$TRAY_USER"@ list-units --no-legend --type=service 2>/dev/null |
			grep -i 'juno.*fan' | awk '{print $1}' | head -1 || true)
		if [ -n "$unit" ]; then
			systemctl --user -M "$TRAY_USER"@ stop "$unit" || true
		fi
		# unit stop or not, the process must be gone before rmmod clevofan
		pkill -f "$TRAY_PROC_RE" 2>/dev/null || true
		sleep 0.5
		TRAY_STOPPED=1
	fi
}

start_daemons()
{
	local rc=0
	if [ "$FANCONTROL_STOPPED" = 1 ]; then
		systemctl start fancontrol.service || rc=6
		FANCONTROL_STOPPED=0
	fi
	if [ "$TRAY_STOPPED" = 1 ]; then
		systemd-run --user -M "$TRAY_USER"@ --unit=juno-fan-monitor-reload \
			--collect /usr/bin/juno-fan-monitor >/dev/null 2>&1 || rc=6
		TRAY_STOPPED=0
	fi
	return $rc
}

# cooling daemons come back even when a step fails
trap 'start_daemons || echo "warning: daemon restart failed" >&2' EXIT

srcversion_of()
{
	local v
	v=$(cat "/sys/module/$1/srcversion" 2>/dev/null || true)
	echo "${v:-(absent)}"
}

# --- run ---------------------------------------------------------------

stop_daemons

pre_write_start=$(cat "$BAT/charge_control_start_threshold")
pre_write_end=$(cat "$BAT/charge_control_end_threshold")

if [ "$THRESHOLDS_GIVEN" = 0 ]; then
	START=$pre_write_start
	END=$pre_write_end
fi

echo "$START" >"$BAT/charge_control_start_threshold" || exit 5
echo "$END" >"$BAT/charge_control_end_threshold" || exit 5
after_write_start=$(cat "$BAT/charge_control_start_threshold")
after_write_end=$(cat "$BAT/charge_control_end_threshold")

if [ "$CLEVOFAN_LOADED" = 1 ]; then
	modprobe -r clevofan || {
		echo "failed to remove clevofan" >&2
		exit 4
	}
fi

for m in $REMOVE_ORDER; do
	modprobe -r "$m" || {
		echo "failed to remove $m" >&2
		exit 4
	}
done

if lsmod | grep '^clevo' >/dev/null; then
	echo "clevo module(s) survived removal:" >&2
	lsmod | grep '^clevo' >&2
	exit 3
fi

for m in $LOAD_ORDER; do
	if [ -n "$MODULES_DIR" ]; then
		insmod "$MODULES_DIR/$m.ko" || {
			echo "failed to insmod $MODULES_DIR/$m.ko" >&2
			exit 4
		}
	else
		modprobe "$m" || {
			echo "failed to modprobe $m" >&2
			exit 4
		}
	fi
done

if [ "$CLEVOFAN_LOADED" = 1 ]; then
	modprobe clevofan || {
		echo "failed to modprobe clevofan" >&2
		exit 4
	}
fi

# a settle timeout is not fatal here: the retry loop below independently
# checks that charge_control_start_threshold reappeared, which is what
# actually gates continuation.
udevadm settle -t 15 || echo "warning: udevadm settle timed out" >&2

# the battery hook re-adds the sysfs attrs; wait briefly if needed
for _ in 1 2 3 4 5 6 7 8 9 10; do
	[ -e "$BAT/charge_control_start_threshold" ] && break
	sleep 0.2
done
[ -e "$BAT/charge_control_start_threshold" ] || {
	echo "charge_control_start_threshold did not reappear" >&2
	exit 5
}

echo "modules_source=${MODULES_DIR:-modprobe}"
echo "pre_write_start=$pre_write_start"
echo "pre_write_end=$pre_write_end"
echo "wrote_start=$START"
echo "wrote_end=$END"
echo "after_write_start=$after_write_start"
echo "after_write_end=$after_write_end"
echo "after_reload_start=$(cat "$BAT/charge_control_start_threshold")"
echo "after_reload_end=$(cat "$BAT/charge_control_end_threshold")"
echo "after_reload_charge_type=$(cat "$BAT/charge_type")"
for m in clevo_keyboard clevo_wmi clevo_acpi clevofan; do
	echo "module_${m}_srcversion=$(srcversion_of "$m")"
done

start_daemons || exit 6

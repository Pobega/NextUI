#!/bin/sh
# Phase 0 recon dump for the Powkiddy V90S NextUI port.
# Invoked by the hook wrappers (postshare.sh, custom.sh) with the hook's
# name as $1. Writes everything to /userdata/recon/ and always exits 0 —
# it must never block or fail the stock boot.
#
# RECON_ROOT can be overridden for dry-running on a desktop host:
#   RECON_ROOT=/tmp/recon-test sh recon.sh test-hook

HOOK="${1:-unknown}"
D="${RECON_ROOT:-/userdata/recon}"

mkdir -p "$D/markers" 2>/dev/null || exit 0

# Marker: prove this hook fired, when, and what was mounted at the time.
{
	date
	cat /proc/uptime 2>/dev/null
	echo "script: $0 (hook: $HOOK)"
	echo "--- mounts at time of hook ---"
	cat /proc/mounts 2>/dev/null
} > "$D/markers/$HOOK" 2>/dev/null

# Collect any early markers boot-custom.sh left in /tmp before /userdata
# was mounted.
for M in /tmp/recon-marker-*; do
	[ -f "$M" ] && cp "$M" "$D/markers/" 2>/dev/null
done

# The heavy dump only needs to run once per boot; later hooks just add
# their marker above.
GUARD=/tmp/.recon-dump-done
[ -e "$GUARD" ] && exit 0
touch "$GUARD" 2>/dev/null

# --- identity ---------------------------------------------------------------
cp /etc/os-release "$D/" 2>/dev/null
cp /usr/share/batocera/batocera.version "$D/" 2>/dev/null
batocera-version > "$D/version.txt" 2>&1
{
	cat /proc/version 2>/dev/null
	echo
	cat /proc/device-tree/model 2>/dev/null
	echo
	cat /proc/cpuinfo 2>/dev/null
} > "$D/kernel.txt" 2>/dev/null
cat /proc/cmdline > "$D/cmdline.txt" 2>/dev/null
dmesg > "$D/dmesg.txt" 2>/dev/null
lsmod > "$D/lsmod.txt" 2>/dev/null

# --- input ------------------------------------------------------------------
ls -laR /dev/input > "$D/input-nodes.txt" 2>/dev/null
cat /proc/bus/input/devices > "$D/input-devices.txt" 2>/dev/null
if command -v udevadm >/dev/null 2>&1; then
	for E in /dev/input/event*; do
		[ -e "$E" ] || continue
		echo "===== $E ====="
		udevadm info "$E" 2>&1
	done > "$D/udev.txt"
fi

# --- display / backlight ----------------------------------------------------
{
	for B in /sys/class/backlight/*; do
		[ -e "$B" ] || continue
		echo "===== $B ====="
		ls -la "$B/" 2>/dev/null
		echo "brightness:     $(cat "$B/brightness" 2>/dev/null)"
		echo "max_brightness: $(cat "$B/max_brightness" 2>/dev/null)"
	done
} > "$D/backlight.txt" 2>/dev/null
# Allwinner disp2 attrs (colortemp/enhance knobs, as used on tg5040)
{
	ls -la /sys/class/disp/disp/attr/ 2>/dev/null
	for A in /sys/class/disp/disp/attr/*; do
		[ -f "$A" ] || continue
		echo "$A: $(cat "$A" 2>/dev/null)"
	done
} > "$D/disp-attr.txt" 2>/dev/null
ls -la /dev/fb* /dev/dri /dev/dri/* > "$D/gfx-nodes.txt" 2>/dev/null

# --- power ------------------------------------------------------------------
{
	ls -laR /sys/class/power_supply/ 2>/dev/null
	for P in /sys/class/power_supply/*; do
		[ -e "$P" ] || continue
		echo "===== $P ====="
		for F in type capacity status voltage_now current_now online present; do
			[ -f "$P/$F" ] && echo "$F: $(cat "$P/$F" 2>/dev/null)"
		done
	done
} > "$D/battery.txt" 2>/dev/null
{
	ls /sys/devices/system/cpu/cpu0/cpufreq/ 2>/dev/null
	echo "available_frequencies: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies 2>/dev/null)"
	echo "available_governors:   $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors 2>/dev/null)"
	echo "governor:              $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
} > "$D/cpufreq.txt" 2>/dev/null

# --- audio ------------------------------------------------------------------
{
	aplay -l 2>&1
	echo
	amixer scontrols 2>&1
	echo
	amixer contents 2>&1
} > "$D/alsa.txt"

# --- graphics / system libraries (Gates A and B) ----------------------------
{
	for L in /lib /usr/lib /usr/lib64 /usr/lib/aarch64-linux-gnu; do
		[ -d "$L" ] || continue
		echo "===== $L ====="
		ls -la "$L"/libSDL2* "$L"/libGLES* "$L"/libEGL* "$L"/libgbm* \
			"$L"/libdrm* "$L"/libmali* "$L"/libpvr* 2>/dev/null
	done
} > "$D/gfx-libs.txt"
# Gate B: which video drivers is stock SDL2 built with?
if command -v strings >/dev/null 2>&1; then
	for S in /usr/lib/libSDL2-2.0.so.0 /usr/lib/libSDL2.so /lib/libSDL2-2.0.so.0; do
		[ -e "$S" ] || continue
		echo "===== $S ====="
		strings "$S" | grep -i -E 'kmsdrm|fbdev|wayland|x11|mali|opengl' | sort -u
	done > "$D/sdl2-drivers.txt" 2>/dev/null
	# Gate A: glibc version ceiling
	for C in /usr/lib/libc.so.6 /lib/libc.so.6; do
		[ -e "$C" ] || continue
		strings "$C" | grep '^GLIBC_' | sort -u
		break
	done > "$D/glibc.txt" 2>/dev/null
fi

# --- boot / init ------------------------------------------------------------
ls -la /etc/init.d/ > "$D/initd.txt" 2>/dev/null
for S in S00bootcustom S12populateshare S31emulationstation S99userservices; do
	cp "/etc/init.d/$S" "$D/" 2>/dev/null
done
grep -rl "postshare\|custom.sh" /etc/init.d/ > "$D/hookpoints.txt" 2>/dev/null
ls -la /boot/ > "$D/boot-dir.txt" 2>/dev/null
cat /proc/mounts > "$D/mounts.txt" 2>/dev/null
blkid > "$D/blkid.txt" 2>&1
df -h > "$D/df.txt" 2>&1

# --- runtime state ----------------------------------------------------------
ps > "$D/ps.txt" 2>&1
free > "$D/free.txt" 2>&1
ls -la /userdata/ /userdata/system/ > "$D/userdata-dir.txt" 2>/dev/null

# --- full /etc for offline inspection ---------------------------------------
tar czf "$D/etc.tgz" /etc 2>/dev/null

sync 2>/dev/null
exit 0

#!/bin/sh
# MinUI.pak for Powkiddy V90S / Allwinner A133P (Batocera-splice stock OS)

export PLATFORM="v90s"
SDCARD_PATH="/mnt/SDCARD"

# /boot/postshare.sh normally bind-mounts the NextUI card here already;
# this is a defensive fallback (eg. when launched by hand over serial)
mkdir -p "$SDCARD_PATH"
if ! mountpoint -q "$SDCARD_PATH"; then
	mount -t vfat -o rw,utf8,noatime /dev/mmcblk1p1 "$SDCARD_PATH" 2>/dev/null || true
fi

export SDCARD_PATH
export BIOS_PATH="$SDCARD_PATH/Bios"
export ROMS_PATH="$SDCARD_PATH/Roms"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export HOOKS_PATH="$USERDATA_PATH/.hooks"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"
export HOME="$USERDATA_PATH"
LAUNCH_LOG="$LOGS_PATH/launch.txt"

export PATH="$SYSTEM_PATH/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
export LD_LIBRARY_PATH="$SYSTEM_PATH/lib:/usr/lib:/lib:$LD_LIBRARY_PATH"
# stock SDL2's custom EGL-framebuffer video driver (PowerVR NULL WSEGL)
export SDL_VIDEODRIVER="fbgl"
export SDL_AUDIODRIVER="alsa"
# Do NOT set SDL_JOYSTICK_DISABLE_UDEV here (h700 habit): with udev disabled,
# stock SDL falls back to an mtime/inotify /dev/input scan that races boot
# and can miss the adc_gamepad entirely (SDL_NumJoysticks()==0). udevd runs
# on stock and its enumeration is deterministic — stock ES relies on it.

for egl_path in /usr/lib/libEGL.so.1 /usr/lib/libEGL.so; do
	if [ -e "$egl_path" ]; then
		export SDL_VIDEO_EGL_DRIVER="$egl_path"
		break
	fi
done
for gles_path in /usr/lib/libGLESv2.so.2 /usr/lib/libGLESv2.so; do
	if [ -e "$gles_path" ]; then
		export SDL_VIDEO_GL_DRIVER="$gles_path"
		break
	fi
done

log_runtime_state() {
	echo "launch: runtime state $(date)" >> "$LAUNCH_LOG"
	echo "launch: LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> "$LAUNCH_LOG"
	ldd "$SYSTEM_PATH/bin/nextui.elf" >> "$LAUNCH_LOG" 2>&1 || true
	ls -l /usr/lib/libEGL.so* /usr/lib/libGLESv2.so* /usr/lib/libSDL2* >> "$LAUNCH_LOG" 2>&1 || true
}

if [ -f "/tmp/poweroff" ]; then
	poweroff
	exit 0
fi
if [ -f "/tmp/reboot" ]; then
	reboot
	exit 0
fi

mkdir -p "$BIOS_PATH" "$ROMS_PATH" "$SAVES_PATH" "$CHEATS_PATH"
mkdir -p "$USERDATA_PATH" "$LOGS_PATH" "$HOOKS_PATH" "$SHARED_USERDATA_PATH/.minui"
echo "launch: starting $(date)" > "$LAUNCH_LOG"

# Stock daemons that fight NextUI: triggerhappy (multimedia keys), hotkeygen
# (SELECT-combo hotkeys), battery-saver (dims then suspends/powers off on
# input inactivity). postshare.sh prevents them from ever starting; this
# covers boots where TF1 still carries an older hook. The delayed sweep
# catches any that init started after the overwrite.
stop_stock_daemons() {
	for SVC in S50triggerhappy S90hotkeygen S96battery-saver-daemon; do
		printf '#!/bin/sh\nexit 0\n' > "/etc/init.d/$SVC" 2>/dev/null || true
	done
	pkill -f battery-saver 2>/dev/null
	pkill -f hotkeygen 2>/dev/null
	pkill thd 2>/dev/null
	return 0
}
stop_stock_daemons
( sleep 20 && stop_stock_daemons ) &

export IS_NEXT="yes"
log_runtime_state

rm -rf "$SDCARD_PATH/.shadercache"

sh "$SYSTEM_PATH/bin/governor.sh" "auto"

keymon.elf > "$LOGS_PATH/keymon.txt" 2>&1 &
batmon.elf > "$LOGS_PATH/batmon.txt" 2>&1 &

rm -f "$USERDATA_PATH/.asoundrc"
audiomon.elf > "$LOGS_PATH/audiomon.txt" 2>&1 &

AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi
"$SYSTEM_PATH/bin/run_hooks.sh" boot.d

cd "$(dirname "$0")"
killall -9 show2.elf > /dev/null 2>&1 || true

parse_hook_cmd() {
	HOOK_CMD="$1"
	HOOK_EMU_PATH=$(echo "$HOOK_CMD" | sed "s/^'\\([^']*\\)'.*/\\1/")
	_remainder=$(echo "$HOOK_CMD" | sed "s/^'[^']*'//")
	if echo "$_remainder" | grep -q "'"; then
		HOOK_TYPE="rom"
		HOOK_ROM_PATH=$(echo "$_remainder" | sed "s/.*'\\([^']*\\)'.*/\\1/")
	else
		HOOK_TYPE="pak"
		HOOK_ROM_PATH=""
	fi
	[ -f /tmp/last.txt ] && HOOK_LAST=$(cat /tmp/last.txt) || HOOK_LAST=""
	export HOOK_CMD HOOK_EMU_PATH HOOK_TYPE HOOK_ROM_PATH HOOK_LAST
}

EXEC_PATH="/tmp/nextui_exec"
NEXT_PATH="/tmp/next"
CRASH_COUNT=0
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	echo "launch: starting nextui.elf $(date)" >> "$LAUNCH_LOG"
	nextui.elf > "$LOGS_PATH/nextui.txt" 2>&1
	EXIT_CODE=$?
	echo "launch: nextui.elf exited $EXIT_CODE $(date)" >> "$LAUNCH_LOG"
	if [ "$EXIT_CODE" != "0" ]; then
		CRASH_COUNT=$((CRASH_COUNT + 1))
		if [ "$CRASH_COUNT" -ge 5 ]; then
			echo "launch: crash limit reached; powering off" >> "$LAUNCH_LOG"
			rm -f "$EXEC_PATH"
			continue
		fi
	else
		CRASH_COUNT=0
	fi
	sh "$SYSTEM_PATH/bin/governor.sh" "performance"

	if [ -f "$NEXT_PATH" ]; then
		CMD="$(cat "$NEXT_PATH")"
		parse_hook_cmd "$CMD"
		"$SYSTEM_PATH/bin/run_hooks.sh" pre-launch.d
		eval "$CMD"
		"$SYSTEM_PATH/bin/run_hooks.sh" post-launch.d
		rm -f "$NEXT_PATH"
		sh "$SYSTEM_PATH/bin/governor.sh" "performance"
	fi

	if [ -f "/tmp/poweroff" ]; then
		poweroff
		exit 0
	fi
	if [ -f "/tmp/reboot" ]; then
		reboot
		exit 0
	fi
done

poweroff

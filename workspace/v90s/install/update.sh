#!/bin/sh
# Self-heal the TF1 hook: keep /boot/postshare.sh in sync with the copy
# shipped in the NextUI payload. /boot is mounted ro, so remount around the
# copy; only touch it when content actually differs.

SDCARD_PATH="/mnt/SDCARD"
SYSTEM_PATH="$SDCARD_PATH/.system/v90s"
TF1_HOOK="/boot/postshare.sh"
TF2_HOOK="$SYSTEM_PATH/dat/postshare.sh"

if [ -f "$TF2_HOOK" ] && [ -f "$TF1_HOOK" ]; then
	if ! cmp -s "$TF2_HOOK" "$TF1_HOOK" 2>/dev/null; then
		if mount -o remount,rw /boot 2>/dev/null; then
			cp "$TF2_HOOK" "$TF1_HOOK" 2>/dev/null || true
			sync
			mount -o remount,ro /boot 2>/dev/null || true
		fi
	fi
fi

exit 0

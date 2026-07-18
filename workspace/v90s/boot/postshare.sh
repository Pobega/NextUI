#!/bin/sh
# NextUI boot hijack for the Powkiddy V90S stock OS (Batocera splice).
# Installed on TF1's ext4 BATOCERA partition as /boot/postshare.sh, with
# stock's own postshare.sh renamed to postshare-stock.sh (chained below so
# its first-boot config seeding still happens).
#
# Invoked by /etc/init.d/S12populateshare with "start" after /userdata is
# mounted, before EmulationStation (S31). The stock rootfs is an overlay
# with a tmpfs upper layer, so replacing emulationstation-standalone here
# lasts only until reboot — stock on disk is never modified. All later init
# services (audio config at S27 etc.) still run; S31 then launches NextUI
# thinking it is ES. Without a NextUI card present, stock boots untouched.

[ -e /boot/postshare-stock.sh ] && sh /boot/postshare-stock.sh "$1"
[ "$1" = "start" ] || exit 0

# Find the NextUI card: stock auto-mounts TF2's first partition under /media,
# with TF1's own SHARE partition (/userdata) as the fallback
SDCARD=
for D in /media/* /userdata; do
	[ -f "$D/.tmp_update/updater" ] && SDCARD="$D" && break
done
# first install: a card holding only MinUI.zip — pull the bootstrap out of it
# with stock's unzip so "copy MinUI.zip to the card" is the whole install
if [ -z "$SDCARD" ]; then
	for D in /media/* /userdata; do
		if [ -f "$D/MinUI.zip" ]; then
			unzip -o "$D/MinUI.zip" '.tmp_update/*' -d "$D" >/dev/null 2>&1
			[ -f "$D/.tmp_update/updater" ] && SDCARD="$D" && break
		fi
	done
fi
[ -z "$SDCARD" ] && exit 0

# shared code and scripts all assume /mnt/SDCARD
mkdir -p /mnt/SDCARD
mount --bind "$SDCARD" /mnt/SDCARD 2>/dev/null

cat > /usr/bin/emulationstation-standalone <<'EOF'
#!/bin/sh
# volatile NextUI shim (tmpfs overlay) — see /boot/postshare.sh
[ "$1" = "--stop-rebooting" ] && exit 0
exec sh /mnt/SDCARD/.tmp_update/updater
EOF
chmod +x /usr/bin/emulationstation-standalone

# Neuter stock daemons that fight NextUI's own input/power handling. We run
# at S12, before any of them start, and the rootfs upper is tmpfs so stock
# on disk is untouched:
# - S50 triggerhappy: stock multimedia-key daemon (keymon handles volume)
# - S90 hotkeygen: synthesizes hotkey events from SELECT combos, which
#   NextUI uses as a normal button
# - S96 battery-saver: input-inactivity daemon that dims, then suspends or
#   powers off the device behind NextUI's back (defaults: dim + suspend)
for SVC in S50triggerhappy S90hotkeygen S96battery-saver-daemon; do
	printf '#!/bin/sh\nexit 0\n' > "/etc/init.d/$SVC"
done
exit 0

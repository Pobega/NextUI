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
exit 0

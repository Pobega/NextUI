#!/bin/sh
# Goes on TF1's ext4 BATOCERA boot partition as /boot/postshare.sh.
# Batocera/Knulli's S12populateshare runs this with "start" after /userdata
# is mounted (before EmulationStation), and with "stop" at shutdown.
#
# The stock V90S image ships its own /boot/postshare.sh (first-boot seeding
# of batocera.conf / es_settings.cfg into /userdata). Install ours by
# renaming stock to postshare-stock.sh first — we chain to it here so that
# behavior is preserved.
[ -e /boot/postshare-stock.sh ] && sh /boot/postshare-stock.sh "$1"

[ "$1" = "start" ] || exit 0

for R in /boot/recon.sh /userdata/system/recon.sh; do
	if [ -e "$R" ]; then
		sh "$R" postshare
		break
	fi
done
exit 0

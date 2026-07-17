#!/bin/sh
# Goes on TF2 (the /userdata SHARE partition) as /userdata/system/custom.sh.
# Batocera/Knulli's S99userservices runs this in the background with
# "start"/"stop" after EmulationStation has started. Fallback hook: works
# without touching TF1 at all.
[ "$1" = "start" ] || exit 0

for R in /userdata/system/recon.sh /boot/recon.sh; do
	if [ -e "$R" ]; then
		sh "$R" custom
		break
	fi
done
exit 0

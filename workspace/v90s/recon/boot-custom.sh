#!/bin/sh
# Goes on TF1's FAT32 boot partition as /boot/boot-custom.sh.
# Batocera/Knulli's S00bootcustom runs this very early, before /userdata is
# mounted — so all we can do is leave a marker in /tmp for recon.sh to
# collect later. Proves whether the earliest hook survives in the V90S's
# spliced stock OS, and captures the pre-share mount state (useful for
# understanding the dual-SD boot requirement).
[ "$1" = "start" ] || exit 0

{
	date
	cat /proc/uptime 2>/dev/null
	echo "script: $0 (hook: boot-custom)"
	echo "--- mounts at time of hook ---"
	cat /proc/mounts 2>/dev/null
} > /tmp/recon-marker-bootcustom 2>/dev/null
exit 0

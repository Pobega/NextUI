# 05 — Recon: How the Device Was Reverse-Engineered

Two passes, both before writing any platform code. Everything in 00 traces
back to one of these.

## Pass 1: offline image dissection (no hardware needed)

The stock image is a plain download (see 00), so most questions were
answered on a PC:

1. `7z x V90S.7z.001` → 16 GB `V90S.img`; `fdisk -l` for the GPT (backup
   table — the primary is nonstandard, see the eGON warning in 00).
2. Partitions carved with `dd skip=<start> count=<sectors>`; **7z 25+ opens
   everything without root**: the zstd squashfs rootfs, the ext4 BATOCERA
   and SHARE partitions, FAT.
3. Rootfs inspection: init scripts (`S00bootcustom`/`S12populateshare`/
   `S99userservices` — hook invocations verbatim-identical to upstream
   Knulli), glibc version (`strings libc.so.6 | grep GLIBC_`), SDL2 video
   drivers (`strings libSDL2* | grep -i <driver names>` — found `fbgl`),
   the PowerVR blob inventory, `emulationstation` link deps (`readelf -d`),
   stock `postshare.sh` and `batocera-boot.conf` from the BATOCERA
   partition, ES `es_input.cfg`.
4. Kernel version straight from the bootimg partition:
   `dd ... | grep -a "Linux version"`.

The local Knulli checkout (`../knulli`) provided cross-checks: the a133
board dir (`board/batocera/allwinner/a133/` + `powkiddy-v90s/`) supplied
the asound.state mixer names, the brightness mechanism, and the SDL2 PVR
patch lineage.

## Pass 2: on-device dump

`workspace/v90s/recon/` — hook wrappers + a dump script, installed per its
README (TF1: `postshare.sh` chain + `recon.sh`; optional TF2 `custom.sh`).
One boot writes `/userdata/recon/`: timestamped markers proving which hooks
fire and when, plus input devices, udev properties, sysfs trees (backlight,
power_supply, cpufreq, disp2 attrs), ALSA layout, mounts/blkid, dmesg, and
an `/etc` tarball.

Key things only the hardware could tell us:

- the real input topology (`adc_gamepad` + `sunxi-keyboard` +
  `axp2202-pek`, exact key bitmaps — the ES `es_input.cfg` in the image had
  suggested a different, generic `sunxi-joypad` layout);
- `mmcblk0` = TF1 (settling the dual-SD `sharedevice` mystery);
- `/userdata` = TF1's own SHARE partition; TF2 auto-mounts under `/media`;
- hook timing (S00 at ~3 s, S12 at ~14 s, both firing on the splice);
- axp2202 PMIC, backlight node names, thermal zones;
- absence of `SW_LID` (with owner confirmation: no lid sensor exists).

## Retrieval on a PC

The card's partitions don't get device nodes on desktop Linux (nonstandard
GPT) — use the offset mounts documented in 00 and pull
`/userdata/recon/` from the SHARE partition (p7).

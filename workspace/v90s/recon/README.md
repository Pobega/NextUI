# V90S Phase 0 recon

Scripts that dump the Powkiddy V90S stock OS's hardware and software details
on one boot, using Batocera/Knulli's built-in user boot hooks (the stock OS is
a Batocera/Knulli splice). See `docs/v90s-port/` for the port documentation
(this recon is covered in `05-recon.md`; the roadmap is `09-roadmap.md`).

Nothing here modifies the stock OS beyond one rename — every script ends with
`exit 0` so the device always continues booting into stock EmulationStation.

## What offline analysis already established

The stock image (`V90S.img` from
<https://github.com/game-de-it/testrepo/releases/tag/V90S_stockOS>) was
dissected offline, so several roadmap questions are already answered:

- **All three hooks exist in the stock rootfs, verbatim-identical to
  upstream Knulli** (`S00bootcustom`, `S12populateshare` line 156,
  `S99userservices` line 52). The postshare hijack point is real.
- **Stock ships its own `/boot/postshare.sh`** (first-boot seeding of
  `batocera.conf`/`es_settings.cfg` from `/boot/preinstall/`). Our wrapper
  chains to it — see install steps below.
- **Gate A (glibc) passes:** device rootfs has glibc 2.39; the
  tg5040-toolchain (Debian buster) targets 2.28, so tg5040-built binaries
  will run.
- **Gate B (video) mostly resolved:** stock ships SDL2 2.30.12 with a custom
  `fbgl` EGL-framebuffer video driver (no kmsdrm, no gbm/drm anywhere) atop
  the PowerVR GE8300 blobs (NULL WSEGL), plus SDL2_image 2.8.5 and
  SDL2_ttf 2.24.0. Same architectural shape as TrimUI stock. Remaining
  on-device question: does `fbgl` accept our fullscreen GLES window.
- **Kernel:** Allwinner BSP Linux 4.9.191 (same 4.9 BSP family as tg5040).
- **TF1 partition layout (GPT):** p1 32M FAT (Allwinner boot resources),
  p4 Android bootimg (kernel), p5 2.5G squashfs rootfs, p6 1G ext4
  `BATOCERA` (= `/boot`, **ext4 — needs a Linux PC to edit**), p7 2G ext4
  `SHARE`. `batocera-boot.conf` maps roms/screenshots/themes to
  `/dev/mmcblk0p8`, a partition not present in the image — created/expected
  at first boot, probably the source of the dual-SD boot requirement.

## Hook points

| Hook | Installed at | Invoked by | When |
|---|---|---|---|
| `boot-custom.sh` | TF1 `/boot` | `/etc/init.d/S00bootcustom` | very early, **before** `/userdata` mounts |
| `postshare.sh` | TF1 `/boot` | `/etc/init.d/S12populateshare` (end) | after `/userdata` mounts, **before** EmulationStation (S31) |
| `custom.sh` | TF2 `/userdata/system/` | `/etc/init.d/S99userservices` | after EmulationStation starts (backgrounded) |

All three are executed via `bash <script>`, so no executable bit is needed.
Each receives `start` at boot and `stop` at shutdown; the wrappers only act
on `start`. Each hook writes a timestamped marker, so one boot reveals which
hooks fire on real hardware and in what order.

## Install

Starting state: TF1 holds the **stock OS** (recon targets the stock splice,
not Knulli). Flash `V90S.img` (link above) to a spare microSD with dd/Etcher.

**TF1** (the OS card — mount partition 6, the ext4 `BATOCERA` partition, on a
Linux PC):

1. rename the existing `postshare.sh` to `postshare-stock.sh` (our wrapper
   chains to it, preserving stock's first-boot config seeding)
2. copy our `postshare.sh`, `boot-custom.sh`, and `recon.sh` to the partition
   root (they become `/boot/postshare.sh` etc. on the device)

**TF2** (the games/user-data card — FAT32/exFAT with `roms` and `bios`
folders at the root, per stock convention):

- copy `custom.sh` and `recon.sh` into a `system/` directory at the card root
  (`/userdata/system/` on the device)

Installing on both cards is the belt-and-braces option: whichever hook fires
first triggers the dump. The heavy dump runs at most once per boot (guarded
by `/tmp/.recon-dump-done`); later hooks only add their marker.

## Run + retrieve

1. Boot the V90S once, let it reach the stock menu, shut down.
2. Pull the `recon/` output directory from wherever `/userdata` landed —
   check TF2's root first, then TF1's `SHARE` partition (p7). Which one it
   is answers the dual-SD mount question.
3. To uninstall, delete the added files and rename `postshare-stock.sh` back.

## What to look at (remaining on-device questions)

- **`markers/`** — which hooks fired on real hardware, and when (uptime).
- **`mounts.txt`** / **`blkid.txt`** — which card/partition became
  `/userdata`, and what `mmcblk0p8` turns out to be (the dual-SD quirk).
- **`input-devices.txt`** / **`udev.txt`** — gpio-keys vs. joystick device,
  keycodes for the button map, and whether a `SW_LID` lid switch exists
  (sleep-on-close support).
- **`backlight.txt`**, **`battery.txt`**, **`cpufreq.txt`**, **`alsa.txt`** —
  sysfs/ALSA paths for `libmsettings`, `batmon`, and `governor.sh`.
- **`disp-attr.txt`** — whether the Allwinner disp2 color-temperature knob
  from tg5040 exists here too.
- **`glibc.txt`**, **`sdl2-drivers.txt`**, **`gfx-libs.txt`** — confirm the
  offline Gate A/B findings match the flashed firmware revision.
- **`dmesg.txt`** — panel driver, resolution, and refresh rate hints.

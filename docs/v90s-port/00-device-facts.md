# 00 — Device Facts

Everything below is verified, with the source noted: **[image]** = offline
dissection of the stock `V90S.img`, **[recon]** = the on-device dump from
`workspace/v90s/recon/` run on real hardware (2026-07-17), **[owner]** =
confirmed by device owner.

## Core hardware

| Component | Fact | Source |
|---|---|---|
| SoC | Allwinner A133P (`sun50iw10p1`), 4× Cortex-A53, aarch64 — same as TrimUI Smart Pro/Brick (`tg5040`) | [recon] cmdline |
| GPU | PowerVR GE8300 ("rogue"), blob DDK 33.15.11.3, firmware in `/lib/firmware/powervr/` | [image] |
| RAM | 1 GB (≈470 MB free after boot per `free`) | [recon] |
| Panel | 3.5" 640×480 IPS; `disp_reserve=1228800` = 640×480×4 confirms | [recon] cmdline |
| Battery | 3000 mAh via **axp2202** PMIC | [recon] |
| CPU freq | 408/600/816/1008/1200/1416/1608/1800 MHz; governors incl. `performance`, `ondemand`, `userspace` | [recon] |
| Storage | 2× microSD (TF1 = OS, TF2 = user); **no internal OS storage** — device boots entirely from TF1 | [image]+[recon] |
| USB | 2× USB-C | reviews |
| Wifi/BT | none | reviews+[recon] (no interfaces) |
| Rumble | none (driver advertises FF but no motor) | [owner-implied], TODO confirm |
| Lid | clamshell, **no close-detection hardware** — no `SW_LID`, no PMIC hallkey | [owner]+[recon] |
| Serial | UART0 console at 115200 (`console=ttyS0`) | [recon] cmdline |

## Kernel & stock OS

- Linux **4.9.191** Allwinner BSP, built 2025-05-13, gcc 6.4.1 (OpenWrt/Linaro), Android-style bootimg. [image]
- Stock OS: **Batocera 42-dev splice** (buildroot 2024.11), version string
  `42-dev-65bb0c1d50 2025/05/13`. Derived from Knulli/Batocera by game-de-it;
  board name marker `rgbxx` (`/boot/boot/batocera.board`). [image]+[recon]
- glibc **2.39** on the device. [image]
- Rootfs is an **overlay with a tmpfs upper** (`lowerdir=vendor:base` squashfs,
  `upperdir` tmpfs) → runtime rootfs changes vanish on reboot. This is
  load-bearing for the boot hijack (02). [recon] mounts
- Stock image download: <https://github.com/game-de-it/testrepo/releases/tag/V90S_stockOS>

## TF1 partition layout (GPT)

`mmcblk0` = TF1. Partition names from the kernel cmdline. [image]+[recon]

| # | Name | Size | Contents |
|---|---|---|---|
| p1 | boot-resource | 32M | FAT, Allwinner boot resources (bootlogo) — mounted `/media/Volumn` |
| p2/p3 | env / env-redund | 256K | u-boot env |
| p4 | boot | 64M | Android bootimg (kernel 4.9.191) |
| p5 | batocera | 2.5G | squashfs (zstd) rootfs base |
| p6 | rootfs (`BATOCERA`) | 1G | **ext4** — mounted `/boot` (ro): `batocera-boot.conf`, `postshare.sh`, `preinstall/`, vendor squashfs |
| p7 | rootfs_data (`SHARE`) | 2G | ext4 — mounted `/userdata` |

TF2 (`mmcblk1p1`, FAT32) is auto-mounted by stock at `/media/<label>`.

### ⚠️ GPT / eGON warning

The image's **primary GPT deliberately points its partition-entry array at
sector 41982** (not the standard LBA 2) because the Allwinner **eGON boot0
blob lives at offset 8 KB**, where a standard entry array would sit. Desktop
Linux calls the table corrupt and creates **no partition nodes** for the
card. **Never run a GPT "repair"** (gdisk/parted) — rewriting standard
entries at LBA 2 overwrites boot0 and makes the card unbootable. Mount
partitions with explicit offsets instead:

```sh
# BATOCERA /boot partition (p6):
sudo mount -o loop,offset=2807037952,sizelimit=1073741824 /dev/sdX /mnt
# SHARE /userdata partition (p7):
sudo mount -o loop,offset=3880779776,sizelimit=2147483648 /dev/sdX /mnt
```

## Input devices [recon]

| event | Name | What it is |
|---|---|---|
| event0 | `sunxi-keyboard` | volume keys: KEY_VOLUMEDOWN (114), KEY_VOLUMEUP (115) |
| event1 | `axp2202-pek` | power key: KEY_POWER (116) |
| event2 | `sunxi-gpadc0` | ADC (no keys) |
| event3 | `audiocodec sunxi Audio Jack` | headphone jack switch (SW bits 2,4) |
| event4 | `adc_gamepad` (rocknix-singleadc-joypad driver) | the built-in pad |

`adc_gamepad` key bitmap decodes to BTN codes **304, 305, 307, 308** (ABXY),
**310–315** (L1, R1, L2, R2, Select, Start), and **316 = BTN_MODE — the Menu
button that stock leaves unmapped**. Dpad is hat0 (ABS_HAT0X=16 / ABS_HAT0Y=17).
The driver declares ABS_X/Y/RX/RY but the device has no analog sticks.
Physical A/B/X/Y label orientation still needs eyes-on confirmation.

## Confirmed control paths [recon]

| Subsystem | Path / control |
|---|---|
| Brightness | `/dev/disp` ioctl `DISP_LCD_SET_BRIGHTNESS` (0x102), range 0–255 (also `/sys/class/backlight/sunxi_backlight/`) |
| Color temp | `/sys/class/disp/disp/attr/color_temperature` |
| Contrast/saturation/exposure | `/sys/class/disp/disp/attr/enhance_{contrast,saturation,bright}` |
| Audio | ALSA card 0 `audiocodec` (sun50iw10codec); controls: `digital volume` (attenuation, reversed), `LINEOUT volume`, `Headphone Volume`, `HpSpeaker Switch`, `Headphone Switch`, `DAC volume` |
| Battery | `/sys/class/power_supply/axp2202-battery/capacity`, `.../axp2202-usb/online` |
| CPU freq | standard cpufreq sysfs on cpu0 |
| CPU temp | `/sys/devices/virtual/thermal/thermal_zone0/temp` |
| Graphics nodes | `/dev/fb0`, `/dev/dri/card0` + `renderD128` (PowerVR render node; no KMS scanout — display is Allwinner disp2) |

## Boot behavior quirks

- Stock `batocera-boot.conf` sets `sharedevice=DEVICES` pointing roms at
  `/dev/mmcblk0p8` — a partition that does not exist in the image. With no
  games found, stock EmulationStation error-loops and reboots. This is the
  infamous "needs two cards" behavior. NextUI doesn't care: our hook runs
  before ES ever starts.
- `bootreason=button` appears in the cmdline; boot takes ~14 s to reach the
  postshare hook (S12), ~3 s to S00.

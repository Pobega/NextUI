# 02 — Boot Hijack, Installer & SD Layout

## The hook: Batocera's own `postshare.sh`

The stock OS is a Batocera/Knulli splice and keeps Batocera's first-class
user boot hooks, verified both in the rootfs and by firing them on hardware:

| Hook | Invoked by | When | Uptime (measured) |
|---|---|---|---|
| `/boot/boot-custom.sh` | `S00bootcustom` | before `/userdata` mounts | ~2.9 s |
| `/boot/postshare.sh` | `S12populateshare` (line 156) | after `/userdata` mounts, **before ES (S31)** | ~14.1 s |
| `/userdata/system/custom.sh` | `S99userservices` | after ES starts (bg) | fallback only |

All are run via `bash <script>` (no exec bit needed) with `start`/`stop` as
`$1`. **Stock ships its own `/boot/postshare.sh`** (first-boot seeding of
`batocera.conf`/`es_settings.cfg` from `/boot/preinstall/`); ours chains to
it, renamed `postshare-stock.sh`.

## The hijack: a volatile ES shim

`workspace/v90s/boot/postshare.sh` (installed as `/boot/postshare.sh` on
TF1's ext4 BATOCERA partition):

1. chains `postshare-stock.sh` (preserves stock first-boot behavior);
2. finds the NextUI card — stock has already auto-mounted TF2 under
   `/media/*` by S12; falls back to `/userdata` (TF1's SHARE partition).
   On a fresh card holding only `MinUI.zip`, it first extracts
   `.tmp_update/*` out of the zip with stock's `/usr/bin/unzip`
   (self-bootstrap: "copy MinUI.zip to the card" is the whole install).
   If no `.tmp_update/updater` found anywhere → `exit 0`, **pure stock
   boot**;
3. bind-mounts the card to `/mnt/SDCARD` (so every shared-code path
   convention holds);
4. overwrites `/usr/bin/emulationstation-standalone` with a two-line shim
   that execs `.tmp_update/updater`.

Because the rootfs upper layer is **tmpfs**, step 4 is volatile: it lasts
exactly one boot and stock-on-disk is never modified. All later init
services (audio config at S27, governor at S18, …) still run normally —
then S31 "starts EmulationStation" and gets NextUI. The shim honors ES's
`--stop-rebooting` stop invocation as a no-op.

Chain: `postshare.sh` → (init continues) → S31 → fake
`emulationstation-standalone` → `.tmp_update/updater` (platform detection:
device-tree model contains `sun50iw10` → `v90s`) → `.tmp_update/v90s.sh`
(installer: unzips `MinUI.zip`/`*.pakz`, runs `bin/install.sh`) →
`MinUI.pak/launch.sh` (env, daemons, the nextui.elf loop).

## SD card layout

Policy copied from the h700 port: **TF1 gets exactly one changed file**
(`postshare.sh`, plus the one-time rename of stock's), everything else
lives on TF2.

- **TF1**: stock OS flashed from the stock image. `/boot` partition edits
  require a Linux PC (ext4) and the offset-mount trick from 00.
- **TF2**: FAT32 card, NextUI's home: `MinUI.zip` (payload), then
  `.system/`, `.tmp_update/`, `.userdata/`, `Bios/`, `Roms/`, `Saves/`.
  Stock auto-mounts it under `/media`.
- No TF2 → the hook finds no updater and stock boots untouched.

Uninstall: delete `postshare.sh` from TF1 (rename `postshare-stock.sh`
back), remove/repurpose the TF2 card.

## Self-heal

`install/update.sh` (runs as `bin/install.sh` during updates) keeps the TF1
hook in sync with the payload copy (`.system/v90s/dat/postshare.sh`),
remounting `/boot` rw only when content actually differs.

## Known consequence of stock's config

Stock's `sharedevice=DEVICES` expects roms on a nonexistent `mmcblk0p8`,
so *stock* ES error-loops without a specially-prepared card. Irrelevant to
NextUI (we intercept before ES), but useful to know when testing the
stock-fallback path: a "can't find game" reboot loop is stock's normal
no-roms behavior, not a hijack failure.

# Porting NextUI to the Powkiddy V90S

A phased roadmap for bringing NextUI to the Powkiddy V90S clamshell using the
**stock-OS hijack** approach — the same philosophy as the TrimUI port: keep the
vendor kernel, drivers, and GPU blobs; replace the frontend entirely.

## Why this is feasible

The V90S uses the **same Allwinner A133P** (quad Cortex-A53, aarch64) and
**PowerVR GE8300** GPU as the TrimUI Brick / Smart Pro (`tg5040`). That means:

- CPU flags carry over unchanged (`-mcpu=cortex-a53 -flto`).
- NextUI's SDL2 + GLES render/shader pipeline is already proven on this GPU.
- The aarch64 libretro core binaries built for `tg5040` will likely run as-is.

Hardware deltas vs. the Brick:

| | TrimUI Brick | Powkiddy V90S |
|---|---|---|
| Screen | 3.2" 1024×768 | 3.5" 640×480 IPS |
| Form factor | slab | clamshell (lid switch?) |
| Analog sticks | — | none |
| Wi-Fi / Bluetooth | yes | none (USB dongles possible) |
| Rumble / RGB LEDs | yes | none |
| RAM | 1GB | 1GB |
| SD | 1× microSD | 2× microSD (TF1 = OS, TF2 = user data) |
| Stock OS | TrimUI Linux | Batocera/Knulli splice (buildroot) |

The stock OS being a Batocera/Knulli derivative is a gift: Batocera ships
first-class user boot hooks (`/boot/postshare.sh`, `/userdata/system/custom.sh`,
`/userdata/system/scripts/*.sh`), so the hijack likely requires **zero rootfs
modification** — closer to a "drop files on the SD card" install than even the
TrimUI port. The infamous "won't boot without TF2" quirk exists because TF2
*is* the `/userdata` (SHARE) partition — which is exactly where NextUI's
payload wants to live.

Two facts about this repo that make the port cheaper than it looks:

- `workspace/all/common/api.c` provides **76 weak `FALLBACK_IMPLEMENTATION`
  no-ops** covering all Wi-Fi (~20 fns), Bluetooth (~25 fns), LED, and turbo
  functions. The v90s platform simply *omits* the `generic_wifi.c` /
  `generic_bt.c` includes and defines `MAX_LIGHTS 0`. No stub-writing marathon.
- NextUI/MinUI heritage supports 640×480 natively:
  `workspace/desktop/platform/platform.h:117` retains the reference geometry
  block (`FIXED_WIDTH 640`, `FIXED_HEIGHT 480`, `MAIN_ROW_COUNT 6`) and
  `skeleton/SYSTEM/res/charging-640-480.png` still exists.

**Effort estimate: ~5–8 weeks of hobby time to a daily-drivable beta.**
Phases 0–2 carry nearly all the uncertainty; after video + input work, the
rest is well-trodden NextUI porting mechanics.

---

## Phase 0 findings from offline image analysis (2026-07-17)

The stock image (`V90S.img`, from
<https://github.com/game-de-it/testrepo/releases/tag/V90S_stockOS>, kept at
`~/Code/v90s-stock/`) was dissected without hardware. Results:

- **Hooks confirmed.** `S00bootcustom`, `S12populateshare` (runs
  `bash /boot/postshare.sh start` at line 156), and `S99userservices`
  (`bash /userdata/system/custom.sh start &`) are all present in the stock
  rootfs, byte-identical in behavior to upstream Knulli. The hijack point is
  real.
- **Stock ships its own `/boot/postshare.sh`** doing first-boot config
  seeding from `/boot/preinstall/`. Any injected postshare.sh must chain to
  it (rename stock → `postshare-stock.sh`; our scripts do this).
- **Gate A (glibc): PASS.** Device rootfs glibc is 2.39; the
  tg5040-toolchain is Debian buster (glibc 2.28), so tg5040-built binaries
  run without a new toolchain.
- **Gate B (video): mostly resolved.** Stock SDL2 is 2.30.12 built with a
  single custom video driver, `fbgl` ("Fbgl EGL Video Driver") — an
  EGL-on-framebuffer driver in the lineage of Knulli's
  `sdl2_add_video_mali_gles2.patch` (see
  `../knulli/board/batocera/allwinner/a133/patches/sdl2/`). No kmsdrm, and
  no libgbm/libdrm anywhere in the rootfs; EGL goes through the PowerVR DDK
  blobs (`libIMGegl`, `libpvrNULL_WSEGL`, rogue 33.15.11.3 firmware).
  SDL2_image 2.8.5 and SDL2_ttf 2.24.0 are also shipped. EmulationStation
  links SDL2 + EGL + GLESv2 directly, proving the fbgl driver drives the
  panel with GLES2 — the same architectural shape as TrimUI stock. Remaining
  on-device check: our fullscreen GLES window through fbgl.
- **Kernel:** Allwinner BSP Linux 4.9.191 (aarch64), same 4.9 BSP family as
  tg5040. Board name marker: `rgbxx` (`/boot/boot/batocera.board`).
- **TF1 layout (GPT):** p1 32M FAT (Allwinner boot resources), p2/p3 256K
  env, p4 Android bootimg (kernel), p5 2.5G squashfs (zstd) rootfs, p6 1G
  ext4 **`BATOCERA` = `/boot` — ext4, not FAT32**, so TF1 hook installs need
  a Linux PC (correcting the FAT32 claims below), p7 2G ext4 `SHARE`.
- **Dual-SD quirk lead:** `batocera-boot.conf` uses `sharedevice=DEVICES`
  mapping roms/screenshots/themes to `/dev/mmcblk0p8` — a partition that
  does not exist in the image. It's created/expected at first boot and is
  the likely cause of the "won't boot without TF2" behavior (recon's
  `blkid.txt`/`mounts.txt` will settle which card is `mmcblk0`).
- **No internal OS storage.** Unlike the TrimUI Brick (stock OS in internal
  flash, NextUI = files dropped on the user's card), the V90S boots entirely
  from TF1 — that's why stock/Knulli/PlumOS are all full flashable card
  images. The NextUI install story is therefore "flash the stock image to
  TF1 once, then add our files to it," not "add files to your existing
  card." Still zero rootfs modification, still reversible.
- **Single-card setup looks achievable.** The two-card requirement is stock
  configuration, not hardware: `sharedevice=DEVICES` points the share at
  `/dev/mmcblk0p8` (external). Batocera also supports
  `sharedevice=INTERNAL`, which would use TF1's own ext4 `SHARE` partition
  (p7, growable to fill the card) — and `batocera-boot.conf` lives on the
  same `/boot` partition we already edit. Verify on hardware during recon:
  boot once two-card stock-style, once single-card with
  `sharedevice=INTERNAL`.
- **No lid sensor (owner-confirmed, 2026-07-17).** The clamshell has no
  close-detection hardware — recon shows no `SW_LID` input device and no
  PMIC hallkey. Sleep/wake is power-button only; every lid mention in the
  phases below is moot, and the h700 lid code was stripped from the port.
- **Recon tooling implemented** at `workspace/v90s/recon/` (recon.sh + three
  hook wrappers + README with install steps).

Sections below predate these findings; where they say the boot partition is
FAT32 or that hook survival is unknown, the list above supersedes them.

## Primary template: the h700 port (pvaibhav/NextUI, branch `h700`)

<https://github.com/pvaibhav/NextUI> carries a mature NextUI port for the
Anbernic RG35XX H700 family (RG40XXV, RG34XXSP clamshell, RG28XX, RGcubexx)
with the same philosophy — stock TF1 untouched bar one file, NextUI on TF2 —
plus a full `docs/h700-port/00–10` write-up. Fetched into this repo as
`FETCH_HEAD` (re-fetch with `git fetch https://github.com/pvaibhav/NextUI
h700`). **Base `workspace/v90s/` on `workspace/h700/`, not `workspace/tg5040/`.**

What transfers directly:
- **Toolchain**: h700 builds inside the tg5040-toolchain container (no new
  image; `makefile.toolchain` maps the platform to the tg5040 image). Copy
  the wiring; our glibc margin (2.28 vs 2.39) is wider than theirs.
- **640×480 UI**: validated on RG40XXV hardware — geometry, shaders,
  overlays, keyboard all good at 480p. Lift their platform.h geometry
  pattern.
- **Clamshell**: RG34XXSP lid handling incl. re-suspend-on-wake-if-closed.
- **msettings**: h700 is also a 4.9 Allwinner BSP with disp2
  (`/dev/disp` + fbdev, no DRM) — closer to the V90S than tg5040's code;
  see their `sunxi_display2_min.h`.
- **Boot shim body**: self-extracting shim + `fbsplash` (zero-dep fb text
  renderer), TF2 mount/fsck-repair/fallback-to-stock, MinUI.zip/`*.pakz`
  install trigger, `shim-common.sh` helpers.
- **SD policy**: TF1's only write is the hook file; NextUI wholly on TF2;
  no TF2 → splash then stock. Adopt it; drop the single-card stretch goal.
- **Docs/CI structure**: `docs/h700-port/*` layout, CI matrix additions.

What stays V90S-specific:
- **Hijack trigger**: their `dmenu.bin` is an Anbernic stock quirk; ours is
  the `/boot/postshare.sh` chain (already implemented in
  `workspace/v90s/recon/`).
- **SDL2/GPU**: they build in-tree SDL2 (JohnnyonFlame mali-fbdev, pinned)
  because Anbernic stock ships none; we try stock's `fbgl` SDL2 2.30 first.
  Fallback = their in-tree `early:` build pattern with Knulli's a133 PVR
  GE8300 SDL2 patch (`../knulli/board/batocera/allwinner/a133/patches/sdl2/`).
- Input maps, keymon keycodes, ALSA mixer names — recon territory.

---

## Phase 0 — Reconnaissance & stock OS dump (2–4 evenings)

Goal: know exactly what the stock rootfs provides before writing any code.
**No Wi-Fi means no easy SSH** — plan around that.

### Getting a shell / getting data off the device (in order of preference)

1. **Blind recon script** (no hardware mods): place the script below at
   `/userdata/system/custom.sh` (TF2, ext4 — mount on a Linux PC) or
   `/boot/postshare.sh` (TF1's FAT32 boot partition — editable on any PC).
   It dumps everything to `/userdata/recon/` and exits so boot continues.
2. **USB-Ethernet dongle** on USB-C + SSH (Batocera/Knulli default:
   `root` / `linux`).
3. **UART** (solder pads; A133 console is UART0 @ 115200) — only if the
   device won't run scripts at all.

Additionally, `dd` the TF1 card on a PC (`dd if=/dev/sdX of=v90s-stock.img`)
and `unsquashfs` the rootfs for offline inspection. archive.org hosts stock
V90-family firmware if a pristine copy is needed.

### Recon script

```sh
#!/bin/sh
D=/userdata/recon; mkdir -p $D
cp /etc/os-release /usr/share/batocera/batocera.version $D/ 2>/dev/null
batocera-version > $D/version.txt 2>&1
cat /proc/version /proc/cpuinfo /proc/device-tree/model > $D/kernel.txt 2>/dev/null
ls -laR /dev/input > $D/input-nodes.txt
for e in /dev/input/event*; do udevadm info $e; done > $D/udev.txt 2>&1
cat /proc/bus/input/devices > $D/input-devices.txt
ls /sys/class/backlight/*/ > $D/backlight.txt
cat /sys/class/backlight/*/max_brightness >> $D/backlight.txt
ls -laR /sys/class/power_supply/ > $D/battery.txt
ls /sys/devices/system/cpu/cpu0/cpufreq/ > $D/cpufreq.txt
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies >> $D/cpufreq.txt 2>/dev/null
ls /sys/class/disp/disp/attr/ > $D/disp-attr.txt 2>/dev/null  # Allwinner disp2: colortemp knobs, same as tg5040?
amixer scontrols > $D/alsa.txt 2>&1; aplay -l >> $D/alsa.txt 2>&1
ls -la /usr/lib/libSDL2* /usr/lib/libGLES* /usr/lib/libEGL* /usr/lib/libgbm* /usr/lib/libdrm* > $D/gfx-libs.txt 2>&1
strings /usr/lib/libc.so.6 | grep -E '^GLIBC_[0-9.]+$' | sort -V | tail -1 > $D/glibc.txt
ls /etc/init.d/ > $D/initd.txt; cp /etc/init.d/S31emulationstation $D/ 2>/dev/null
grep -rl "postshare\|custom.sh" /etc/init.d/ > $D/hookpoints.txt 2>/dev/null
cat /proc/mounts > $D/mounts.txt; blkid > $D/blkid.txt 2>&1
tar czf $D/etc.tgz /etc 2>/dev/null
```

### Decision gates coming out of Phase 0

- **Which hook fires and when** (`postshare.sh` vs `custom.sh` vs `scripts/`)
  — verify with timestamped marker files.
- **Gate A (glibc):** device glibc version vs. the `tg5040-toolchain`
  container's target → decides whether the toolchain is reusable as-is.
- **Gate B (SDL2):** does stock SDL2 exist and which video drivers does it
  have (`strings libSDL2*.so | grep -i 'kmsdrm\|fbdev'`) → decides
  link-against-stock vs. ship-our-own strategy.
- **Input:** joystick device vs. gpio-keys keyboard device, plus keycodes.
- **Lid switch:** look for `SW_LID` in `/proc/bus/input/devices` — NextUI
  already has `PLAT_initLid` / `PLAT_lidChanged` in
  `workspace/all/common/api.h`.

---

## Phase 1 — Boot hijack + hello world (2–4 evenings)

Goal: NextUI-owned code runs at boot from TF2, EmulationStation never appears,
and the device still boots stock when the hijack files are absent.

The design mirrors the TrimUI chain
(`skeleton/BOOT/trimui/app/runtrimui.sh` → `.tmp_update/updater` → `tg5040.sh`):

- **Primary hook: `/boot/postshare.sh`** on TF1's FAT32 partition (runs after
  `/userdata` is mounted, before ES). Logic mirrors `runtrimui.sh`: if
  `/userdata/.tmp_update/updater` exists, stop/never-start ES
  (`/etc/init.d/S31emulationstation stop` or
  `batocera-es-swissknife --emukill`) and chain to the updater; otherwise
  return 0 and boot stock untouched. Verify in the unpacked squashfs exactly
  where `postshare.sh` is invoked and whether it blocks init — if it blocks,
  run our launch loop directly in it; if not, use it to disable ES and chain
  from `custom.sh`.
- **Fallback (zero-TF1-touch): `/userdata/system/custom.sh`** — ES flashes
  briefly, then we kill it and take over. Keep this working as the
  "no-TF1-edit" install path.
- **Path unification:** early in the boot script,
  `mkdir -p /mnt/SDCARD && mount --bind /userdata /mnt/SDCARD`, so
  `SDCARD_PATH "/mnt/SDCARD"` in `platform.h` and every shell-script
  convention carries over verbatim from `tg5040`.
- **Updater detection:** `skeleton/BOOT/common/updater` keys off
  `/proc/cpuinfo` strings that won't match here — add a v90s case using
  `/proc/device-tree/model` (or presence of `/usr/bin/batocera-version` plus a
  board marker) to select `v90s.sh`.
- **Hello world:** cross-compile a static test binary (write to backlight
  sysfs, log uptime) with the tg5040 toolchain and run it from the hook →
  proves **Gate A**. Then a dynamically linked SDL2 test using stock
  `/usr/lib` via `LD_LIBRARY_PATH` → first probe of **Gate B**.

Repo work: none yet beyond a scratch branch; iterate with scripts on-card.

---

## Phase 2 — Video + input on device (1–2 weeks; the riskiest phase)

Goal: NextUI's SDL2+GLES render path (`PLAT_initVideo` etc.) draws on the
panel and buttons register.

### Video — try in order

1. **Link against stock SDL2** (same soname) with
   `SDL_VIDEODRIVER=KMSDRM`; PowerVR `libGLESv2`/`libEGL`/`libgbm` come from
   stock. Same GPU as tg5040, so NextUI's GLES shader pipeline
   (`generic_video.c`, included by `workspace/tg5040/platform/platform.c`)
   should be compatible.
2. **Ship our own SDL2** if stock's is missing features — NextUI also needs
   SDL2_image and SDL2_ttf, which Batocera may not ship. Build them in the
   toolchain with kmsdrm enabled against the dumped sysroot and ship in
   `.system/v90s/lib/`, keeping the GPU/EGL blobs from stock. This is the
   existing tg5040 pattern (the root `makefile` `system:` target already
   ships libsamplerate/libzip/libchdr per-platform) extended to SDL itself.

Confirm real panel refresh with a frame-time probe and set `SCREEN_FPS`
accordingly (tg5040 uses 60.235).

### Input

- Build `evtest`/`jstest` (already wired in `workspace/tg5040/makefile`'s
  `early:` target) and enumerate devices. Expect a gpio-keys keyboard-style
  device (typical for Batocera A133 boards) rather than TrimUI's
  `trimui_inputd` joystick.
- Fill `platform.h` `CODE_*` (and/or `JOY_*` if a joystick node exists);
  all `AXIS_*` = `*_NA` (no sticks).
- Map the stock-unused **Menu button** as `BTN_MENU` — NextUI's core UX
  button, and a genuine UX win over stock (which leaves it dead and abuses
  Select as hotkey).
- Check for `SW_LID`.

### Geometry (640×480)

Per the reference block in `workspace/desktop/platform/platform.h`:

```c
#define FIXED_WIDTH   640
#define FIXED_HEIGHT  480
#define FIXED_SCALE   2
#define FIXED_BPP     2
#define MAIN_ROW_COUNT 6
#define QUICK_SWITCHER_COUNT 3
#define PADDING 10
```

**Repo work begins:** create `workspace/v90s/` as a stripped copy of
`tg5040` (see file inventory at the end).

---

## Phase 3 — Launcher boots (3–5 evenings)

Goal: `nextui.elf` runs in the standard launch loop.

- Port `skeleton/SYSTEM/tg5040/paks/MinUI.pak/launch.sh` →
  `skeleton/SYSTEM/v90s/paks/MinUI.pak/launch.sh`: keep the env exports,
  directory scaffolding, `/tmp/nextui_exec` + `/tmp/next` eval loop, hooks,
  and governor calls. **Delete** the TrimUI-isms: GPIO exports,
  `trimui_inputd`, `led_anim`, BT/Wi-Fi init blocks, and the
  `strings /usr/trimui/bin/MainUI` model detection (replace with
  `/proc/device-tree/model`).
- Port `skeleton/SYSTEM/tg5040/bin/`: `governor.sh` (cpufreq paths from
  recon; A133 likely uses the same `scaling_governor`/`scaling_setspeed`
  sysfs as tg5040), `run_hooks.sh` (unchanged), `suspend`, `setterm`, and
  `reboot_next` (plain `poweroff` unless the tg5040 wake-limbo bug appears).
- Build `libmsettings` first — the launcher links it. Phase-5 features can
  return sane constants at this point.

---

## Phase 4 — minarch + one core (3–5 evenings)

- Build `minarch.elf` for v90s; start with **gambatte** (simplest, no GL
  quirks), then `fceumm`, `snes9x`, `pcsx_rearmed`.
- **Shortcut:** same CPU/arch/GPU as tg5040 — try tg5040-built cores as-is
  on device first (they link mostly libc/libm/libstdc++); if Gate A passed,
  they should run.
- Then create `workspace/v90s/cores/makefile` as a trimmed copy of
  `workspace/tg5040/cores/makefile` (28 cores listed). The generic engine
  (`workspace/all/cores/makefile`) builds with `platform=$(PLATFORM)`; some
  core makefiles may need a `v90s` case aliased to the tg5040/unix-aarch64
  paths — or simply build them with `platform=tg5040`.
- In-game hotkeys: with no dedicated function button beyond Menu, mirror
  tg5040's `BTN_MOD_*` combos on `BTN_MENU`; keep Select available as a
  fallback hotkey to match user muscle memory from stock.

---

## Phase 5 — Audio, brightness, battery (1 week)

`workspace/v90s/libmsettings/msettings.c`, adapted from tg5040's (1431
lines, heavily TrimUI-specific):

- **Brightness:** `/sys/class/backlight/<name>/brightness` (Batocera
  standard; recon gives the node) instead of `/dev/disp` ioctls.
- **Volume:** ALSA on the Allwinner `audiocodec` card — tg5040's
  tinyalsa/amixer code (`digital volume`, `DAC volume` controls, around
  msettings.c lines 1319–1389) may work nearly unchanged since it's the same
  Allwinner codec family; recon's `amixer scontrols` confirms names. Keep
  the USB-DAC path (USB-C audio is plausible); drop the BT/A2DP path.
- **Color temperature:** keep **iff**
  `/sys/class/disp/disp/attr/color_temperature` exists on the stock A133
  BSP kernel (recon item); otherwise compile the setters as no-ops.
- **Mute/jack detect:** find the headphone-jack switch event or ALSA jack
  control; drop tg5040's GPIO243 DIP-switch code.
- **Battery:** `PLAT_getBatteryStatus*` from
  `/sys/class/power_supply/battery/capacity` + `/status` (Batocera
  standard); `batmon.elf`/`libbatmondb` are platform-agnostic
  (`workspace/all/`).
- **Sample rate:** test `PLAT_pickSampleRate` at 48000 vs 44100 against the
  codec.

---

## Phase 6 — Sleep, power, keymon (3–5 evenings)

- `workspace/v90s/keymon/keymon.c`, adapted from tg5040's 229-line daemon:
  fix the event-node paths, replace TrimUI codes (`CODE_MENU0..2` 314–316,
  mute GPIO243, rumble feedback) with V90S keycodes. If the V90S has no
  hardware volume keys, volume moves entirely to Menu+dpad combos
  (`BTN_MOD_*` in platform.h).
- **Sleep:** try `echo mem > /sys/power/state` (Batocera suspend). A133 BSP
  suspend is often unreliable → implement `PLAT_supportsDeepSleep()`
  honestly and fall back to NextUI's soft-sleep (backlight off + minimum CPU
  clock).
- **Lid:** wire `PLAT_initLid` / `PLAT_lidChanged` to `SW_LID` if present —
  sleep-on-lid-close is the flagship feature for a clamshell.
- **Power off:** plain `poweroff`; keep the `tg5040.sh`-style "force
  poweroff so stock never touches the card" tail in the boot script.

---

## Phase 7 — Packaging & installer (3–5 evenings)

- `workspace/v90s/install/boot.sh` (becomes `.tmp_update/v90s.sh`): copy
  tg5040's flow (splash via `show2.elf`, `.pakz` handling, `MinUI.zip`
  unzip, `install.sh` chain) minus LED/TrimUI blocks;
  `workspace/v90s/install/update.sh` likewise.
- Root `makefile`: add `v90s` to `PLATFORMS`; extend the
  `system:`/`cores:` conditionals (add v90s to the lib-shipping filter list
  alongside `tg5040 tg5050 my355`; exclude LED/bootlogo/BT paks); the
  `special:` target gains a `build/BOOT/v90s/` family containing
  `postshare.sh`, the `custom.sh` fallback, and an install README.
- `skeleton/BOOT/common/updater`: add the v90s platform-detection case
  (device-tree model string).
- **User-facing install story:**
  - **TF2:** extract the NextUI release zip onto the ext4 SHARE partition
    (payload + Roms/Bios/Saves skeleton).
  - **TF1:** copy `postshare.sh` to the small FAT32 boot partition — a
    one-line file drop on any PC.
  - **Uninstall** = delete those files.
- Open item: the ext4 SHARE partition is hostile to Windows/macOS users →
  evaluate reformatting SHARE as exFAT if the stock kernel supports it
  (recon item), or lean on NextCommander (already in the build) + USB
  gadget MTP if the kernel has configfs gadget support.

---

## Phase 8 — Polish & upstream (ongoing)

- **Toolchain:** fork `tg5040-toolchain` → `LoveRetro/v90s-toolchain` (same
  crosstool, sysroot swapped for libs/headers extracted from the V90S stock
  dump), publish `ghcr.io/loveretro/v90s-toolchain`. Until then, build with
  the tg5040 container by symlinking `toolchains/v90s-toolchain`
  (`makefile.toolchain` clones/pulls by name).
- Update README and PAKS.md for the new platform (v90s likely needs no
  `DEVICE` variants).
- Questions to raise with LoveRetro before any PR: platform naming (`v90s`
  vs. an A133-Powkiddy family name — the Powkiddy V20 shares the SoC),
  whether to also support Knulli-as-base officially, CI matrix for a third
  platform.
- Quality-of-life passes: overlays/shaders at 640×480, `settings.elf` /
  `minput.elf` paks, game-time/battery trackers (all in `workspace/all/`,
  should Just Work), performance tuning of the governor script for
  FBNeo/PCSX.

---

## Risks & fallback ladder

| Risk | Likelihood | Mitigation / fallback |
|---|---|---|
| Stock hooks (`postshare.sh`/`custom.sh`) absent or broken in the splice | Low–Med | Hooks are core Batocera machinery and the stock is Knulli-derived. Fallback: `/boot/overlay` edit (`knulli-save-overlay`) to patch `S31emulationstation`; last resort: repack the TF1 squashfs. |
| glibc mismatch: tg5040-toolchain binaries won't run on stock rootfs (Gate A) | Med | Build a v90s-toolchain against the dumped sysroot — a known process (it's how tg5040-toolchain exists). |
| Stock SDL2/KMSDRM can't drive the panel for the GLES pipeline (Gate B) | Med | Ship our own SDL2 (kmsdrm+GBM) built against stock's PowerVR/gbm blobs (same GPU as tg5040 = proven shader path). Worst case: fbdev/DRM dumb-buffer software path at reduced eye candy. |
| Stock OS too unstable as a base (boot loops, kernel bugs) | Med | **Pivot to Knulli alpha as the host OS** — the hook mechanism is identical, Knulli is maintained. Design the boot scripts to work on both stock and Knulli from day one so the pivot is free. |
| A133 BSP suspend broken | High | Soft-sleep fallback (backlight + governor); lid-close = soft sleep; document it. |
| ext4 SHARE partition hostile to Windows/macOS users | High | exFAT reformat if the kernel supports it; NextCommander + USB gadget MTP otherwise. |

---

## Appendix — New repo files (complete inventory)

```
workspace/v90s/
  makefile                     # from tg5040: keep evtest/jstest/unzip60/NextCommander
                               # early: targets; drop btmanager/rfkill/poweroff_next
  platform/platform.h          # 640x480 geometry, V90S CODE_*/JOY_* maps,
                               # AXIS_*=NA, MAX_LIGHTS 0, SDCARD_PATH /mnt/SDCARD
  platform/platform.c          # from tg5040 minus wifi/BT includes;
                               # keep generic_video.c include; no-op rumble
  platform/makefile.env        # identical to tg5040 (-mcpu=cortex-a53 -flto, SDL2, GLES)
  platform/makefile.copy       # trimmed: installer, show2, unzip, NextCommander
  libmsettings/                # backlight sysfs + ALSA audiocodec + optional disp colortemp
  keymon/                      # V90S event nodes/keycodes; power/menu combos
  install/                     # boot.sh, update.sh, logo.png
  cores/makefile               # trimmed copy of tg5040's core list

skeleton/SYSTEM/v90s/
  bin/                         # governor.sh, run_hooks.sh, suspend, setterm, reboot_next
  paks/MinUI.pak/launch.sh     # runtime boot loop (TrimUI-isms stripped)
  paks/Emus/                   # per-console .pak launchers (copy tg5040 set)

skeleton/BOOT/v90s/
  postshare.sh                 # TF1 FAT32 hijack (primary)
  custom.sh                    # TF2 /userdata/system fallback hijack

skeleton/BOOT/common/updater   # + v90s detection case (device-tree model)
makefile                       # + v90s in PLATFORMS, system:/cores:/special: conditionals
makefile.toolchain             # no change (name-driven); new LoveRetro/v90s-toolchain repo
```

### Key template files in this repo

- `workspace/tg5040/platform/platform.h` / `platform.c` — the `PLAT_*`
  implementation templates.
- `workspace/all/common/api.c` — weak fallbacks that make Wi-Fi/BT/LED
  omission free.
- `skeleton/SYSTEM/tg5040/paks/MinUI.pak/launch.sh` — the runtime boot loop.
- `workspace/tg5040/install/boot.sh` + `skeleton/BOOT/common/updater` — the
  install/hijack chain to adapt to Batocera's hooks.
- root `makefile` — platform registration, lib shipping, `special:` packaging.

## Sources

- [Batocera wiki: launch a script](https://wiki.batocera.org/launch_a_script)
- [Knulli: Patches and Overlays](https://knulli.org/configure/patches-and-overlays/)
- [Knulli: Partitioning](https://knulli.org/guides/partitioning/)
- [Retro Handhelds: Can KNULLI Save the Powkiddy V90S?](https://retrohandhelds.gg/can-knulli-save-the-powkiddy-v90s/)
- [Retro Handhelds: PlumOS on the V90S](https://retrohandhelds.gg/how-to-install-plumos-powkiddy-v90s/)
- [knulli-cfw/distribution releases](https://github.com/knulli-cfw/distribution/releases)
- [Powkiddy V90 stock firmware archive](https://archive.org/details/powkiddy-v90-stock-firmware)

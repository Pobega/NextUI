# 03 — Platform Layer

`workspace/v90s/` is derived from the h700 port's `workspace/h700/` (not
tg5040): same kernel generation (4.9 Allwinner BSP + disp2), same
evdev-first input philosophy, similar screen class. Deltas below.

## platform.h

- Geometry: `FIXED_WIDTH 640`, `FIXED_HEIGHT 480`, `FIXED_SCALE 2`,
  `MAIN_ROW_COUNT 6`, `QUICK_SWITCHER_COUNT 3`, `PADDING 5` — the 480p UI
  was validated on hardware by the h700 port (RG40XXV).
- Single device: no `is_*` flags, no `DEVICE` env, no HDMI block (defines.h
  fallbacks cover it).
- Buttons (see 00 for the full evdev derivation): `CODE_A..Y` =
  304/305/307/308, `CODE_L1/R1/L2/R2` = 310/311/312/313, `CODE_SELECT/START`
  = 314/315, **`CODE_MENU` = 316 (BTN_MODE)** — the button stock leaves dead
  becomes NextUI's Menu. `CODE_PLUS/MINUS` = 115/114 (real volume keys),
  `CODE_POWER` = 116. No sticks: all `AXIS_*` = NA. `MAX_LIGHTS 0`.
- Modifier convention (tg5040-style): Menu+Vol = brightness,
  Select+Vol = color temperature.
- `SCREEN_FPS 60.0` — TODO: measure the real panel refresh on hardware.

## platform.c

- **Input:** raw evdev on `/dev/input/event0..7`; the built-in
  `adc_gamepad` reports clean BTN codes + hat0 dpad. SDL joystick handling
  is reserved for external USB pads, and `adc_gamepad` is explicitly
  skipped there to avoid double events (h700 pattern).
- **Battery:** axp2202 paths (identical PMIC to h700 devices).
- **Power off:** `/tmp/poweroff` / `/tmp/reboot` markers consumed by
  launch.sh, as everywhere else.
- **Sleep:** `PLAT_supportsDeepSleep() = 1` optimistically; A133 BSP
  suspend reliability is a first-boot test item. Wake = power-key release
  (drained-fd pattern from h700 to avoid instant re-wake).
- **No lid:** the V90S clamshell has no close sensor (owner-confirmed);
  `PLAT_initLid` reports no lid and the h700 lid/hallkey code is gone.
- **Stubs by hardware absence:** rumble no-op; network status hardcoded
  offline; wifi/BT/LED/turbo left to `api.c`'s weak `FALLBACK_IMPLEMENTATION`
  no-ops (`generic_wifi.c`/`generic_bt.c` are *not* included).
- **Timezones:** Batocera has no `timedatectl`; get/set go through the
  `/etc/localtime` symlink into `/usr/share/zoneinfo`. No NTP (no network).
- Video is entirely `generic_video.c` (see 04).

## libmsettings

h700's msettings adapted — most of it applied unchanged because both
platforms are sunxi disp2 + tinyalsa:

- Brightness: `/dev/disp` ioctl `DISP_LCD_SET_BRIGHTNESS` (the same
  mechanism Knulli's own V90S `brightness` tool uses).
- Color temp / contrast / saturation / exposure: disp2 attr files, all
  confirmed present on device.
- Volume: tinyalsa on the `audiocodec` card — `digital volume`
  (attenuation, reversed percent mapping, same as h700/tg5040),
  `LINEOUT volume`, `HpSpeaker Switch` (names from the device's
  asound.state; note the case difference from h700's `lineout volume`).
- Init routing: `HpSpeaker Switch` + `Headphone Switch` on (replaces
  h700's `SPK`/`LINEOUT`/`Output* Mixer` set).
- Display calibration: single `DISPLAYCAL_PRESET_DEFAULT` (no per-panel
  preset measured yet); the h700 RGXX preset table is removed.
- HDMI code is inert by construction (no `extcon/hdmi` node → `GetHDMI()`
  is 0) and was left in place to minimize the diff from h700.

## keymon

h700's daemon with V90S constants: watches event0–4; Menu = 316,
Select = 314, volume keys 115/114. Menu+Vol → brightness, Select+Vol →
color temp, bare Vol → volume.

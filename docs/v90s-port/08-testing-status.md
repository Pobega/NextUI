# 08 — Testing Status

## Verified

| Item | How |
|---|---|
| Boot hooks fire on the splice (S00 + S12 postshare, pre-ES) | on-device recon markers |
| Stock postshare chaining preserves first-boot seeding | code review of stock script; chain implemented |
| glibc compatibility (Gate A): buster 2.28 toolchain vs device 2.39 | image dissection; same argument h700 validated empirically on its target |
| Full workspace compiles for `PLATFORM=v90s` | tg5040 container build, exit 0: nextui.elf, minarch.elf (+RA, +shipped libs incl. liblz4), settings.elf, keymon.elf, libmsettings.so, batmon, all tools, show2 |
| Input topology & keycodes | recon evdev dump (00) |
| Brightness/colortemp/audio/battery/cpufreq control paths | recon sysfs/ALSA dump + Knulli V90S board files |
| No lid sensor | owner + recon |
| Recon tooling itself | ran on hardware, produced the full dump |
| ES shim handoff end-to-end: postshare self-bootstrap from MinUI.zip, updater detection, installer, launch loop | first boot on hardware (2026-07-18): install log clean, launch loop reached |
| `fbgl` drives an SDL window (show2 splash rendered) | same first boot |
| Stock rootfs lacks tinyalsa → ship `libtinyalsa.so.1` in `.system/v90s/lib` | first boot: nextui.elf exited 127 (`libtinyalsa.so.1` not found), crash-limit poweroff; fixed |

## Pending first boot (in rough order of risk)

1. **`fbgl` accepts NextUI's fullscreen GLES window** — the one real
   unknown left in the video chain (04); the splash proved plain-SDL
   windows work, GLES still unproven. Everything downstream (shaders,
   overlays) is proven on this GPU by tg5040.
3. **Audio** — tinyalsa control names live (`digital volume`,
   `HpSpeaker Switch`), sample rate pick (48000 vs 44100), jack switching.
4. **Cores** — try tg5040-built cores first (same arch/CPU); else build
   with `platform=tg5040` aliasing per the roadmap.
5. **Suspend** — `echo mem` resume reliability on this BSP kernel; fall
   back to soft sleep (`PLAT_supportsDeepSleep() → 0`) if flaky.
6. **Physical A/B/X/Y orientation** — verify with the Input pak; swap
   `platform.h` codes if mirrored.
7. **Panel refresh** — measure and correct `SCREEN_FPS`.
8. **Rumble** — driver advertises FF; confirm whether a motor exists at
   all (currently no-op).
9. **NextCommander (Files.pak)** — the patch assumes stock SDL2 assigns
   adc_gamepad buttons joystick indices in ascending keycode order
   (304→0 … 316→10) and delivers the keyboard-style dpad as SDLK arrows;
   verify both on device.
10. Battery curve sanity, charge detection, power-off/reboot paths,
    volume-key repeat feel, 480p UI polish pass.

## Not applicable on this device

Wifi, Bluetooth, HDMI, LEDs, lid sleep, per-device variants (`DEVICE` env).

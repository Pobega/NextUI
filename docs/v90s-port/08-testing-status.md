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
| **Gate B closed: `fbgl` drives NextUI's fullscreen GLES window** — launcher UI renders | on-device (2026-07-18), second boot after the tinyalsa fix |
| Games run with sound: GBA (tg5040-built gpsp core), button mapping correct in menu and in game | on-device (2026-07-18) |
| Audio + volume controls, brightness/settings UI, battery display | on-device (2026-07-18) |
| Deep sleep broken as predicted: resume renders but backlight stays off and input fds die → `PLAT_supportsDeepSleep()` now returns 0 (soft sleep + 2-min poweroff) | on-device (2026-07-18) |

## Pending (in rough order of risk)

1. **NextCommander (Files.pak)** — first test: app rendered but all
   input dead. Suspected cause: it only opened SDL joystick index 0,
   which on this device may not be the adc_gamepad (other event nodes
   can enumerate as joysticks first). Patch v2 opens every joystick and
   Files.pak now logs to `logs/commander.txt` — retest, and check the
   log's joystick list if buttons are still dead (next suspect: the
   ascending-keycode button-index assumption, 304→0 … 316→10).
2. **Soft sleep** — verify the fallback: power button sleeps/wakes
   instantly, 2 minutes asleep powers off cleanly (quicksave present).
3. **Remaining cores** — GBA works; spot-check pcsx_rearmed, snes9x,
   fceumm, picodrive; else build with `platform=tg5040` aliasing.
4. **Panel refresh** — measure and correct `SCREEN_FPS`.
5. **Rumble** — driver advertises FF; confirm whether a motor exists at
   all (currently no-op).
6. Battery curve sanity over a full charge, charge detection,
   power-off/reboot paths, volume-key repeat feel, 480p UI polish pass.

## Not applicable on this device

Wifi, Bluetooth, HDMI, LEDs, lid sleep, per-device variants (`DEVICE` env).

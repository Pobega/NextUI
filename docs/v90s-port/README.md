# NextUI on the Powkiddy V90S (`v90s` platform)

A port of NextUI to the Powkiddy V90S clamshell, following the approach of
the [h700 port](https://github.com/pvaibhav/NextUI) (Anbernic RG XX): the
stock OS stays on disk untouched, NextUI hijacks the boot via a sanctioned
hook and lives on its own SD card.

**Status: shelved (2026-07-18) after four on-device test rounds.**
The port itself works on hardware: boot hijack, installer, launcher
(fullscreen GLES through stock's `fbgl` SDL driver), games with sound
(tg5040-built cores run as-is), buttons, volume, brightness, battery. What
stopped the effort is the stock splice's install story — its ext4 `/boot`
on a nonstandard GPT means installing the one hook file needs a Linux PC
and offset-mounts, judged not viable for release. The identified (verified,
unexecuted) path forward is rebasing TF1 on **official Knulli**, where the
same hook is a one-file drop onto a FAT32 partition — see the post-mortem
in [02](02-boot-and-installer.md) and the final state in
[08](08-testing-status.md). Every hardware constant in the port comes from
either offline dissection of the stock OS image or an on-device recon dump —
not guesswork.

| Doc | Contents |
|---|---|
| [00-device-facts](00-device-facts.md) | Hardware, kernel, stock OS, partitions, input devices, every confirmed sysfs/ALSA path |
| [01-toolchain-and-build](01-toolchain-and-build.md) | Building with the tg5040 container, gotchas |
| [02-boot-and-installer](02-boot-and-installer.md) | The postshare hijack, volatile ES shim, SD card layout, install/uninstall |
| [03-platform-layer](03-platform-layer.md) | platform.h/c, input, keymon, msettings decisions |
| [04-video-display](04-video-display.md) | The `fbgl` SDL2 driver, PowerVR stack, open questions |
| [05-recon](05-recon.md) | How the device was reverse-engineered; recon tooling usage |
| [08-testing-status](08-testing-status.md) | What's verified vs. pending first boot |
| [09-roadmap](09-roadmap.md) | The original porting roadmap (phases, history, findings log) |

Numbering mirrors `docs/h700-port/` in the h700 branch; gaps are sections
that don't apply here (the V90S has no wifi, no Bluetooth, no HDMI, no
rumble, no lid sensor, no per-device variants).

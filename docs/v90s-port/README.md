# NextUI on the Powkiddy V90S (`v90s` platform)

> [!IMPORTANT]
> **Provided As-Is:** `NextUI-v90s` and [`BaseOS-v90s`](https://github.com/Pobega/BaseOS-v90s) are provided **as-is**. They are **not** intended to be fully maintained forks of [NextUI](https://github.com/LoveRetro/NextUI) or [BaseOS](https://github.com/pvaibhav/BaseOS), but rather a standalone port meant to give Powkiddy V90S users a good experience. There may be bugs or missing features—feel free to report issues, though it is not extremely likely that all of them will be fixed.

A port of NextUI to the Powkiddy V90S clamshell (`sun50iw10p1` / Allwinner A133P), paired with **[`BaseOS-v90s`](https://github.com/Pobega/BaseOS-v90s)**.

**Status: Working & released on [`BaseOS-v90s`](https://github.com/Pobega/BaseOS-v90s).**
Initially tested using a stock Batocera/PlumOS `postshare.sh` boot hijack (recorded in the historical design docs below), the V90S port now runs natively on **[`BaseOS-v90s`](https://github.com/Pobega/BaseOS-v90s)**—a minimal BusyBox-based OS on TF1 that boots the V90S in ~5.9 seconds (`~3.6s` before Linux + `~2.3s` Linux to NextUI) and automatically installs `MinUI.zip` and `*.pakz` from either TF1's `BASEOS` partition (single-card setup) or a FAT32/exFAT TF2 card (dual-card setup). Every hardware constant in the port comes from either offline dissection of the stock OS image or an on-device recon dump — not guesswork.

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

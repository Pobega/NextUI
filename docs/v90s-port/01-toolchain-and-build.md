# 01 — Toolchain & Build

## No dedicated toolchain image

Like the h700 port, v90s builds inside the existing
**`ghcr.io/loveretro/tg5040-toolchain`** container: same SoC family
(A133P), same arch/tuning (aarch64, `-mcpu=cortex-a53 -flto`), and the
image's glibc (Debian buster, 2.28) is far older than the device's (2.39) —
forward-compatible by construction.

`makefile.toolchain` maps the platform to the image via `TOOLCHAIN_NAME`
(`v90s` → `tg5040`) and **forces `PLATFORM`/`UNION_PLATFORM` through into
the container** — without that, the image's `setup-env.sh` default
(`UNION_PLATFORM=tg5040`) silently builds the wrong platform.

## Building

```sh
make PLATFORM=v90s build          # workspace build (no cores)
make PLATFORM=v90s build-cores    # cores (COMPILE_CORES gate applies)
```

On hosts without `make` (e.g. Fedora Atomic), run the container directly:

```sh
podman run --rm --security-opt label=disable \
  -v "$PWD/workspace:/root/workspace" \
  -e PLATFORM=v90s -e UNION_PLATFORM=v90s \
  ghcr.io/loveretro/tg5040-toolchain:latest \
  /bin/bash -c '. ~/.bashrc && export PLATFORM=v90s UNION_PLATFORM=v90s && cd /root/workspace && make PLATFORM=v90s'
```

(`--security-opt label=disable` is required on SELinux-enforcing hosts.)

## SDL2: link toolchain, run stock

Unlike h700 (which builds an in-tree SDL2 because Anbernic stock has none
usable), the V90S stock OS ships SDL2 **2.30.12 + SDL2_image 2.8.5 +
SDL2_ttf 2.24.0**. So v90s follows the tg5040 pattern instead: binaries
link against the container's SDL2 dev files and run against stock's
libraries at runtime (`SDL_VIDEODRIVER=fbgl`, see 04). There is no
`other/sdl2` build in `workspace/v90s/makefile` — its `early:` target builds
evtest, jstest, unzip60, and NextCommander (patched for v90s via
`patches/NextCommander-v90s.patch`: 640×480 at PPU 2, adc_gamepad joystick
button indices in ascending keycode order, dpad as SDLK arrows).

## Platform filters to know about

Several shared makefiles gate work behind explicit platform lists; `v90s`
was added to:

- root `makefile`: the lib-shipping block (`tg5040 tg5050 my355 v90s`) —
  libsamplerate/libzip/libchdr/libcrypto/etc. are bundled into
  `.system/v90s/lib` because the stock OS doesn't provide them — and the
  liblz4 rewind block.
- `workspace/all/minarch/makefile`: all three filters (RA sources, RA
  linking, and the `all:` target). **Gotcha:** for platforms not in the
  `all:` filter, minarch "builds" successfully while producing nothing.
- `workspace/makefile`: `ledcontrol` and `bootlogo` are gated to
  tg5040/tg5050 (no LEDs; bootlogo pak not wired for v90s yet).

## Shared-code delta

None. The port needed one shared API (`PWR_requestSleep`, an h700-branch
addition) only for lid handling, which the V90S doesn't have — so the
scaffold currently touches `workspace/all/` not at all beyond the minarch
makefile filters.

# 04 — Video & Display

## The stack (from stock image dissection)

- Kernel 4.9 BSP. **No KMS/DRM display path**: scanout is Allwinner disp2
  (`/dev/disp` + fbdev). A PowerVR DRM *render* node exists
  (`/dev/dri/renderD128`) for the GPU only.
- PowerVR GE8300 blob stack: `libIMGegl`, `libGLESv2.so.2`, `libEGL.so.1`,
  `libpvrNULL_WSEGL.so` (NULL window system = EGL direct to framebuffer),
  rogue 33.15.11.3 firmware. **No libgbm, no libdrm** anywhere in the rootfs.
- Stock SDL2 2.30.12 is built with exactly one real video driver:
  **`fbgl` ("Fbgl EGL Video Driver")** — the splice's evolution of the
  mali-fbdev driver lineage (Knulli's a133 tree carries the ancestor patch:
  `sdl2_add_video_mali_gles2.patch`, with a PVR GE8300 variant under
  `board/batocera/allwinner/a133/powkiddy-v90s/patches/sdl2/`).
- Proof the stack drives the panel: stock EmulationStation links
  SDL2 + EGL + GLESv2 directly and renders through it.

## NextUI's strategy

`generic_video.c` unchanged: SDL2 window → `SDL_GL_CreateContext` → GLES
shader pipeline → `SDL_GL_SwapWindow`. launch.sh exports
`SDL_VIDEODRIVER=fbgl` plus `SDL_VIDEO_EGL_DRIVER`/`SDL_VIDEO_GL_DRIVER`
pointing at stock's PowerVR libs. Binaries link the toolchain's SDL2 and
resolve stock's at runtime (same soname; the tg5040 pattern).

The GPU being identical to tg5040's means the shader pipeline itself is
proven; the **untested link in the chain is `fbgl` accepting our fullscreen
GL window** — the single biggest first-boot question.

## Fallback ladder if fbgl disappoints

1. Ship our own SDL2 built for the PVR NULL WSEGL, using the h700 port's
   in-tree SDL2 `early:` build pattern with Knulli's
   `001-add-pvr-ge8300-mali-driver.patch` as the driver source.
2. Worst case: fbdev software blit through NextUI's CPU scalers (loses
   shaders/overlays; not a destination, a debugging waypoint).

## Notes

- 640×480 UI at scale 2 is already hardware-validated by the h700 port on
  the RG40XXV (same class of panel).
- Panel refresh: assumed 60.0; measure with a frame-time probe on hardware
  and correct `SCREEN_FPS` (tg5040's true value is 60.235 — these BSP
  panels are rarely exactly 60).
- Brightness/colortemp/enhance knobs live on the disp2 side and are
  independent of the GL path (see 03).

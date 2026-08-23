# Changelog

Notable changes to DS1Edit. Releases before 2.1.1 are described on the
[GitHub releases page](https://github.com/bethington/d2-ds1-edit/releases).

## 2.1.1

### Fixed

- **Rendering could fall off the GPU entirely, costing a thousandfold in frame
  rate while still drawing a correct picture.** Reported on a GTX 1060 at
  0.26 fps. Four causes, addressed together:
  - The executable now asks for the discrete GPU (`NvOptimusEnablement`,
    `AmdPowerXpressRequestHighPerformance`). A laptop with two of them hands
    OpenGL to the integrated chip otherwise.
  - The offscreen render targets no longer inherit whatever bitmap flags
    happened to be set, so a window resize can no longer move the editor's
    compositing surface into system memory.
  - The DCC and DC6 sprite decoders take one bitmap lock per frame instead of
    one per pixel. Against a video bitmap the old path cost a texture lock for
    every pixel written.

### Added

- Startup now reports the renderer the graphics context actually bound, not
  just the driver name, and warns when it is a software rasterizer:

  ```text
  display: 2560x1440, OpenGL driver
  display: renderer "NVIDIA GeForce RTX 5060 Ti/PCIe/SSE2", vendor "NVIDIA Corporation"
  ```

- A one-time warning when a memory bitmap is drawn into the map, or when the
  render target itself is one. Both are silent thousandfold slowdowns
  otherwise.
- The `[perf]` block reports `present` separately from `render`, which
  separates slow compositing from a stalled `al_flip_display`.
- Troubleshooting notes in `README.md` for an editor that draws correctly but
  at well under one frame per second.

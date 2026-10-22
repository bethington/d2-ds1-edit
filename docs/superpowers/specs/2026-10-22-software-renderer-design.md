# Software 8-bit Renderer Design

Status: approved in conversation 2026-10-22, pending written-spec review.

## Goal

DS1Edit must run with excellent performance on machines without usable 3D
acceleration (virtual machines, Remote Desktop, absent or broken GPU drivers,
basic display adapters, the ARM handheld target) by rendering the way Diablo II
does: compose the whole window in 8-bit palette indices in software and present
one finished frame per display refresh.

## Decisions

| Topic | Decision |
| --- | --- |
| Minimum hardware | No usable 3D at all. |
| Scope | Whole window: map, panels, menus, text. Native OS message boxes stay native. |
| Present layer | Keep Allegro 5 for window, input and dialogs; one full-window upload per frame. |
| Pixel format | Everything 8-bit palette indices; RGB exists only in the present step. |
| Compositor | Full redraw per frame (game-faithful). Pre-composited chunks only if zoomed-out measurements later require them. |
| Performance target | 60 fps median at 1920x1080 windowed, 1:1 zoom, one core of a mid-range x86 CPU, no GPU acceleration; at least 30 fps at 1:16. |
| Migration | Side by side behind `renderer=`; software becomes default after parity and performance gates; GPU per-tile path then deleted. |

## Architecture

Five units, each testable on its own:

- **`src/render/canvas.c/.h`** — `DS1_CANVAS { uint8_t *pixels; int w, h, stride; DS1_RECT clip; }`.
  Primitives: fill, hline/vline/line, rect, rectfill, RLE sprite blit (opaque,
  horizontal flip), RLE sprite blit through a 256x256 colormap, RLE blit
  through a darkness level. All primitives clip against `clip`; sprite offsets
  are never trusted. No Allegro dependency.
- **`src/render/spritecache.c/.h`** — converts the raw index-frame seam
  (`DS1EDIT_INDEX_FRAME` from `dcc_decode_raw`, `dc6_decode_indices`, DT1 index
  decode) into game-style RLE (`skip n` / `copy n` runs per row), one copy per
  asset per zoom level, built on first use, freed when a map closes.
  Replaces per-tile Allegro bitmaps on the software path.
- **`src/render/map_sw.c/.h`** — draws the map into a canvas. Reuses
  `wpreview_get_visible_tile_bounds` and `wpreview_sort_order_data` so draw order
  and depth sorting are identical to the GPU path.
- **`src/render/ui_sw.c/.h`** — `ui_text`, `ui_rect`, `ui_fill`, `ui_line`,
  `ui_blit` in palette indices; bitmap fonts; named UI colours.
- **`src/render/present.c/.h`** — the only unit that touches Allegro on the
  software path: palette-to-RGB lookup, one write-only lock of a single video
  bitmap, expand, unlock, flip.

Selection: INI `renderer = software | gpu | auto`. During transition `auto`
selects software when the bound renderer string is a known software
rasterizer (WARP, llvmpipe, GDI Generic), otherwise the GPU path.

## Frame data flow

Animation advances on the existing 25 Hz tick; presents are capped at 60 Hz
and do not depend on vsync (no busy loop on devices without it).

1. Visible bounds and sort order from the shared helpers.
2. Layer passes in the current order: floor -> shadows -> walls/objects merged
   by sort order -> roofs -> editor overlays (selection, paths, grid, labels).
3. Draw-operation mapping:

| Current GPU call | Software primitive |
| --- | --- |
| `wpreview_draw_bitmap` | opaque RLE blit (transparent runs skipped) |
| `wpreview_draw_trans_bitmap`, faded hidden layers | RLE blit through 50% cmap |
| `wpreview_draw_blended_bitmap(trans_b)` | RLE blit through COF blend tables, modes 0-6 (D2CMP formulas) |
| `wpreview_draw_gouraud_bitmap` | RLE blit through darkness colormap at the cell light level |
| `wpreview_draw_line/rect/rectfill` | canvas primitives in palette indices |

4. Zoom 2:1, 1:1, 1:2, 1:4, 1:8, 1:16: per-zoom RLE copies made by nearest-pixel
   decimation (2:1 by pixel doubling). No per-frame scaling.
5. Palette and gamma: the canvas holds indices only. The 256-entry
   index-to-RGB table is rebuilt only on act palette or gamma change.
6. Present: write-only lock of one video bitmap, expand ~2M indices via the
   table, unlock, flip. Fallback when no usable video bitmap: one memory-bitmap
   draw (a single software upload), with a one-time warning.

Editing state (selection flags, hidden layers, `IS_SELECTED` tint) is read from
the same cell data as the GPU path; editing code does not change.

## UI port

- Inventory at design time: ~91 text calls, 15 files using `al_draw_*`.
  Ported window by window behind `ui_sw`.
- Fonts: rasterized once at startup from the current font into 1-bit glyph
  masks; drawn as single palette indices, no antialiasing. Measurement uses
  the same glyph metrics.
- About 16 named UI colours resolved to nearest indices in each act palette
  (production `palette_find_closest`, live-verified against D2CMP), recomputed
  on palette or gamma change.

## Error handling

- Present resource unavailable: log reason, fall back to memory-bitmap present,
  warn once in stderr and the status bar. Never fall into a silent per-pixel
  path.
- Asset decode failure: magenta-index placeholder box, logged once per asset.
- Out-of-range sprite offsets are clipped, never written.

## Testing

- **Canvas unit tests** (portable, no display): clipping on all four edges,
  flips, colormap and darkness blits, every primitive; poisoned borders detect
  out-of-bounds writes.
- **Sprite cache**: index frame -> RLE -> blit reproduces the index frame for
  authored DC6/DCC/DT1 fixtures; each zoom level matches a reference
  decimation.
- **Blend and darkness tables**: compared with D2CMP formulas and, through the
  package B live-capture harness, with the tables read from the original game
  on an idle fleet member.
- **Map parity**: six core maps plus one per act rendered headless and
  windowed (never fullscreen) through both renderers. Index frames compared
  with game-faithful expectations; RGB compared with the GPU path; known
  differences (GPU bilinear zoom-out) are named contract rules, not
  tolerances. Goldens regenerated once, in a reviewed commit, after the tile
  rarity selection change is settled.
- **UI colours**: for each of the five act palettes, every named UI
  foreground/background pair resolves to indices meeting a minimum contrast
  ratio (WCAG 4.5:1 for text); glyph rendering matches authored 1-bit masks.
- **Software-device lane**: selftest and the performance gate run under
  Windows WARP / Mesa llvmpipe `opengl32.dll`, and Mesa llvmpipe on Linux CI.
- **Performance**: `scripts/equivalence/benchmarks.py`; act1_town and
  act5_town at 1920x1080 windowed. Record frame-time histogram, compose vs
  present time, peak working set. CI thresholds only after a reviewed baseline.

## Phasing

1. Canvas, sprite cache, present, map view under `renderer=software`
   (Allegro UI drawn on top temporarily). Gate: six-map parity; fps target on
   WARP.
2. UI port onto `ui_sw` with bitmap fonts and palette contrast test.
3. `auto` defaults to software on software rasterizers, then for everyone once
   parity and performance hold on Windows, Linux and macOS.
4. Delete the GPU per-tile path and bitmap promotion code.

## Out of scope

- Dirty rectangles and pre-composited chunks (revisit only if measurements
  miss the target).
- Multithreaded or SIMD compositing (revisit for the ARM handheld target).
- Replacing Allegro with SDL or native 2D blit backends; `present` is the
  seam if that becomes necessary.

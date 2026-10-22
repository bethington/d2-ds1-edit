# Software 8-bit Renderer, Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compose the DS1 map view in 8-bit palette indices on the CPU and present it with one write-only upload per frame, selectable with `renderer = software | gpu | auto`, proven against the GPU path on the six core maps and measured against the spec's frame-rate target on a software rasterizer.

**Architecture:** Four Allegro-free units (`canvas`, `spritecache`, `sw_tables`, `map_sw`) build an index frame from the same cell data, visible-tile bounds, layer order and editing state as the GPU path in `src/render/preview.c`; `present` is the only new unit that touches Allegro on the software path (palette-to-RGB table, one write-only lock of `screen_buff`, a memory-bitmap fallback with a one-time warning). `wpreview_draw_tiles()` branches to the software compositor for passes 1-6 and then lets the existing Allegro code draw everything after pass 6 (walkable info, grid over walls, cursor, paths, labels, HUD) on top, as the spec's phase 1 prescribes.

**Tech Stack:** C99, Allegro 5.2 (vcpkg x86-windows / distro packages / Homebrew), CMake + CTest, Unity (`test/unity`), Python 3 `unittest` (Pillow, numpy optional) under `scripts/equivalence` and `test/python`.

**Spec:** `docs/superpowers/specs/2026-10-22-software-renderer-design.md` (Phasing item 1). Read it before starting; this plan argues from it.

## Global Constraints

Every task's requirements implicitly include all of these.

- **Language and portability:** C99 that builds unchanged with MSVC (Win32 x86), GCC and Clang (LP64: Linux x86_64, macOS arm64). Pixel and on-disk data use fixed-width types (`uint8_t`, `int32_t`, `uint32_t`); never `long` for a pixel count or a file field. Windows-only code is behind `#if defined(_WIN32)` with a working portable fallback.
- **Ship all platforms together:** Windows x86 + Linux x86_64 + macOS arm64 go out as one unit. Nothing in this plan is "Windows first"; every new test is registered for all three.
- **Windowed-only testing:** every INI a test, harness or lane writes contains `full_screen = NO`. No test ever creates a fullscreen display. `scripts/equivalence/sw_runtime.py` refuses to write an INI without it (Task 11).
- **Windows launchers:** every Python harness that starts the editor or a probe calls `SetErrorMode(0x8003)` (`0x0001 | 0x0002 | 0x8000`) before the first launch, so a missing DLL or a fault becomes an exit code, never a dialog.
- **Test data:** portable tests (CTest label `portable`) use authored bytes only and need no game data. Game-data lanes use the vanilla install `F:\D2VersionChanger\VersionChanger\LoD\1.13c`, are local-only, and are never wired into CI.
- **No Allegro in the compositor:** `src/render/canvas.c`, `spritecache.c`, `sw_tables.c`, `map_sw.c`, `frame_stats.c`, `render_select.c` and `view_geom.c` contain no Allegro calls (`map_sw.c` and `view_geom.c` include `structs.h` for the editor's data types only). `present.c` and `display_device.c` are the only new units that call Allegro.
- **Transparency key:** palette index 0 is transparent in every sprite (the `DS1EDIT_INDEX_FRAME` seam rule, `src/core/index_frame.h`). The present step maps index 0 to opaque black, as `misc_draw_screen()` does for the GPU path.
- **Default unchanged:** `renderer = gpu` is the default in phase 1, and the GPU path's output does not change. `auto` selects software only when the bound device is a known software rasterizer (spec: "WARP, llvmpipe, GDI Generic").
- **Spec values, verbatim:** performance target "60 fps median at 1920x1080 windowed, 1:1 zoom, one core of a mid-range x86 CPU, no GPU acceleration; at least 30 fps at 1:16". Zoom levels "2:1, 1:1, 1:2, 1:4, 1:8, 1:16" with "per-zoom RLE copies made by nearest-pixel decimation (2:1 by pixel doubling). No per-frame scaling." Error handling: "Present resource unavailable: log reason, fall back to memory-bitmap present, warn once in stderr and the status bar. Never fall into a silent per-pixel path." "Asset decode failure: magenta-index placeholder box, logged once per asset." "Out-of-range sprite offsets are clipped, never written." Performance: "CI thresholds only after a reviewed baseline."
- **Build commands (Git Bash):**
  ```bash
  CMAKE="/c/Program Files/JetBrains/CLion 2026.2.3.1/bin/cmake/win/x64/bin/cmake.exe"
  CTEST="/c/Program Files/JetBrains/CLion 2026.2.3.1/bin/cmake/win/x64/bin/ctest.exe"
  "$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
  ```
  Build one target: `"$CMAKE" --build tmp/build-sw --config Release --target <target>`. Run one test: `"$CTEST" --test-dir tmp/build-sw -C Release -R "^<name>$" --output-on-failure`. Building the `ds1edit` target writes `bin/ds1edit.exe` (its `RUNTIME_OUTPUT_DIRECTORY` is the repository's `bin/`), which other work in the tree also uses.
- **Commits:** the maintainer currently keeps changes uncommitted for review. Every "Commit" step reads "Commit (only when the maintainer has asked for commits)" and lists the exact paths to `git add`; never `git add -A` or `git add .`.
- **Concurrent work:** another agent is fixing a sub-1 FPS regression in the GPU path (memory bitmaps drawn into the map). This plan does not depend on its outcome. It touches some of the same code: `src/render/preview.c` (`wpreview_draw_tiles`, the palette block that calls `dt1_rebuild_bitmaps_from_cache`, and the helpers moved in Task 8), `src/main.c` (`ds1edit_report_display_driver`, `ds1edit_try_create_display`, the headless block), `src/misc.c` (`misc_draw_screen`), `src/core/dt1.c` (`dt1_free`), `src/core/cof.c` (`anim_destroy_cof`) and `src/ui/interface.c` (the `--selftest` exit block). Before editing any of those, re-read the current file; if an anchor quoted below has changed, apply the same edit at the equivalent place and say so in the task's report.

---

## File Structure

Created (all new code lives under `src/render/` beside the GPU path it will replace):

| File | Responsibility |
| --- | --- |
| `src/render/canvas.h/.c` | `DS1_CANVAS` (8-bit indices, optional tag plane, clip rect), `DS1_RLE` sprite type, fill/putpixel/hline/vline/line/rect/rectfill, RLE blits (opaque, h-flip, 256x256 colormap, 256-entry source table, 256-entry destination table), index dump writer. |
| `src/render/spritecache.h/.c` | Index frame -> game-style RLE (`skip n` / `copy n` per row); per-zoom copies (2:1 doubling, 1:N nearest decimation through `index_buf_scale_down`); lazy cache keyed by (source tile, zoom); magenta placeholder; once-per-asset failure log; `spritecache_clear()`. |
| `src/render/sw_tables.h/.c` | D2CMP colour tables for COF `trans_b` 0-6 and 33 darkness levels: copied byte-for-byte from a full `Pal.PL2`, else generated from the documented formulas. |
| `src/render/present.h/.c` | Palette -> packed RGB table per pixel layout, canvas expansion, one write-only lock of the target bitmap, memory-bitmap fallback with a one-time warning, 60 Hz pacing arithmetic. |
| `src/render/render_select.h/.c` | `renderer =` parsing, software-rasterizer device names, `auto` resolution, the active renderer. |
| `src/render/display_device.h/.c` | Name of the bound device: `GL_RENDERER` on OpenGL, the D3D9 adapter description on Direct3D. |
| `src/render/view_geom.h/.c` | The view geometry both paths share, moved verbatim out of `preview.c`: `wpreview_light_dist`, `wpreview_sort_order_data`, `wpreview_get_visible_tile_bounds`, `coord_to_tile`. |
| `src/render/map_sw.h/.c` | The software map compositor: passes 1-6 of `wpreview_draw_tiles()` into a canvas; full-map bounds for headless; queued labels for the Allegro overlay. |
| `src/render/frame_stats.h/.c` | Frame-time samples, nearest-rank percentiles, the histogram and the `selftest:` report lines. |
| `test/cmake/software_renderer.cmake` | CTest registration for every new C test. |
| `test/test_render_canvas.c`, `test_render_rle.c`, `test_render_blit.c`, `test_render_spritecache.c`, `test_render_spritecache_fixtures.c`, `test_render_sw_tables.c`, `test_render_present.c`, `test_render_select.c`, `test_render_view_geom.c`, `test_render_map_sw.c`, `test_render_map_sw_objects.c`, `test_render_frame_stats.c`, `test_render_present_display.c` | Unity tests, one per unit. |
| `scripts/equivalence/sw_runtime.py` | Private, windowed-only runtime directories (exe + DLLs + assets + owned `Ds1edit.ini`), software-device setup, `renderer:` line parsing, `smoke` CLI. |
| `scripts/equivalence/renderer_parity.py` | Six-map parity: headless and windowed renders through both renderers, RGB compared exactly outside named contract rules. |
| `test/python/test_sw_runtime.py`, `test/python/test_renderer_parity.py` | Python unit tests (fake trees, synthetic images). |
| `test/equivalence/profiles/performance-software.json` | Performance-gate workloads for `benchmarks.py`. |

Modified:

| File | Change |
| --- | --- |
| `CMakeLists.txt` | New sources in `SOURCES`. |
| `test/CMakeLists.txt` | `software_renderer` added to the package list. |
| `test/cmake/visuals.cmake`, `test/cmake/robustness.cmake` | `spritecache.c` linked wherever `dt1.c` is (Task 4). |
| `src/structs.h` | `CONFIG_S.renderer`; `CMD_LINE_S.headless_indices`, `selftest_indices`, `uncapped`, `deterministic`. |
| `src/config.c`, `Ds1edit.ini.sample` | `renderer` key, default text. |
| `src/main.c` | Renderer resolution, device report, headless software branch, vsync option, new command-line defaults. |
| `src/misc.c` | New command-line flags; 60 Hz pacing in `misc_draw_screen()`. |
| `src/render/preview.c/.h` | Shared helpers moved out (Task 8); software branch, labels, status-bar warning, headless software render (Task 11). |
| `src/core/dt1.c`, `src/core/cof.c` | `spritecache_clear()` before tile/sprite buffers are freed. |
| `src/ui/interface.c` | `--deterministic`, frame-time collection and the `selftest:` report lines. |
| `scripts/equivalence/benchmarks.py`, `test/python/test_benchmarks.py` | `editor-selftest` workload kind and frame-time records. |
| `.github/workflows/build.yml` | Software-device lane steps (Linux llvmpipe under Xvfb, Windows hosted runner). |

Task order is dependency order: 1-5 are pure units, 6-8 the Allegro seam and the shared geometry, 9-10 the compositor, 11-12 wiring and instrumentation, 13-14 the phase gates.

---
### Task 1: Canvas primitives and the test package

**Files:**
- Create: `src/render/canvas.h`, `src/render/canvas.c`
- Create: `test/cmake/software_renderer.cmake`, `test/test_render_canvas.c`
- Modify: `test/CMakeLists.txt` (package list), `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: nothing.
- Produces (`src/render/canvas.h`):
  ```c
  typedef struct DS1_RECT { int32_t x0, y0, x1, y1; } DS1_RECT;        /* half-open */
  typedef struct DS1_CANVAS { uint8_t *pixels; uint8_t *tags; int32_t w, h, stride; DS1_RECT clip; uint8_t *owned; } DS1_CANVAS;
  typedef struct DS1_RLE { int32_t w, h, origin_x, origin_y; uint32_t *row; uint8_t *data; size_t data_len; int32_t opaque_count; } DS1_RLE;
  #define CANVAS_TAG_EXACT 0
  #define CANVAS_TAG_TABLE 1
  #define CANVAS_TAG_LINE  2
  #define CANVAS_MAX_LINE_SPAN (1 << 20)
  void canvas_init_view(DS1_CANVAS *cv, uint8_t *pixels, uint8_t *tags, int32_t w, int32_t h, int32_t stride);
  int  canvas_create(DS1_CANVAS *cv, int32_t w, int32_t h, int with_tags);   /* 1 ok */
  void canvas_destroy(DS1_CANVAS *cv);
  void canvas_set_clip(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
  void canvas_reset_clip(DS1_CANVAS *cv);
  void canvas_fill(DS1_CANVAS *cv, uint8_t c);
  void canvas_putpixel(DS1_CANVAS *cv, int32_t x, int32_t y, uint8_t c);
  void canvas_hline(DS1_CANVAS *cv, int32_t x0, int32_t x1, int32_t y, uint8_t c);   /* inclusive */
  void canvas_vline(DS1_CANVAS *cv, int32_t x, int32_t y0, int32_t y1, uint8_t c);   /* inclusive */
  void canvas_line(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c);
  void canvas_rect(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c);     /* inclusive */
  void canvas_rectfill(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c); /* inclusive */
  int  canvas_write_dump(const DS1_CANVAS *cv, const char *path);  /* 1 ok; format below */
  ```
  Dump format: 8 bytes `D S 1 I D X 1 \0`, `uint32` LE width, `uint32` LE height, 1 byte `has_tags`, then `w*h` index bytes row by row, then `w*h` tag bytes when `has_tags`.
  Tag meaning: `0` exact copy or fill, `1` written through a colour table, `2` written by `canvas_line`.

- [ ] **Step 1: Register the test package**

Create `test/cmake/software_renderer.cmake`:

```cmake
# Software 8-bit renderer (docs/superpowers/specs/2026-10-22-software-renderer-design.md).
#
# Pure units link no Allegro and no editor globals. Units that read the editor's
# structs or call Allegro go through ds1edit_visual_test(... ALLEGRO ...) from
# visuals.cmake, which also appends them to DS1EDIT_ALLEGRO_TESTS so the DLL
# copy covers them. Every test here is portable (authored bytes, no game data)
# unless its label says otherwise.

set(_sw ${CMAKE_SOURCE_DIR}/src/render)
set(_core ${CMAKE_SOURCE_DIR}/src/core)

function(ds1edit_sw_pure_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE unity)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
endfunction()

macro(ds1edit_sw_add_test test_name target)
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS portable TIMEOUT 120)
endmacro()

# --- Task 1: canvas primitives ---------------------------------------------
ds1edit_sw_pure_test(test_render_canvas test_render_canvas.c ${_sw}/canvas.c)
ds1edit_sw_add_test(render_canvas test_render_canvas)
```

In `test/CMakeLists.txt`, change the package loop so the new package runs after `robustness` (it uses `ds1edit_visual_test` from `visuals`) and before `python`. Replace

```cmake
foreach(_pkg map_integrity visuals equivalence robustness release python)
```

with

```cmake
foreach(_pkg map_integrity visuals equivalence robustness software_renderer release python)
```

- [ ] **Step 2: Write the failing test**

Create `test/test_render_canvas.c`:

```c
/* Canvas primitives (src/render/canvas.c).
 *
 * The canvas under test is a view into a larger buffer whose border is
 * poisoned with 0xAA, in both the pixel and the tag plane, so any write
 * outside the canvas -- the bug a clipped primitive must never have -- shows
 * up as a changed border byte. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "unity/unity.h"
#include "render/canvas.h"

#define W      16
#define H      12
#define PAD    8
#define STRIDE (W + 2 * PAD)
#define POISON 0xAA

static uint8_t    s_buf[(H + 2 * PAD) * STRIDE];
static uint8_t    s_tag[(H + 2 * PAD) * STRIDE];
static DS1_CANVAS s_cv;

void setUp(void)
{
   memset(s_buf, POISON, sizeof(s_buf));
   memset(s_tag, POISON, sizeof(s_tag));
   canvas_init_view(&s_cv, s_buf + PAD * STRIDE + PAD, s_tag + PAD * STRIDE + PAD,
                    W, H, STRIDE);
   canvas_fill(&s_cv, 0);
}

void tearDown(void) {}

static uint8_t px(int x, int y) { return s_cv.pixels[y * STRIDE + x]; }
static uint8_t tg(int x, int y) { return s_cv.tags[y * STRIDE + x]; }

static int count_index(uint8_t c)
{
   int x, y, n = 0;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         if (px(x, y) == c)
            n++;
   return n;
}

static void assert_borders_intact(void)
{
   int x, y;
   char msg[64];

   for (y = 0; y < H + 2 * PAD; y++)
      for (x = 0; x < STRIDE; x++)
      {
         int inside = (y >= PAD) && (y < PAD + H) && (x >= PAD) && (x < PAD + W);
         if (inside)
            continue;
         snprintf(msg, sizeof(msg), "border byte (%d,%d) overwritten", x, y);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, s_buf[y * STRIDE + x], msg);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, s_tag[y * STRIDE + x], msg);
      }
}

void test_fill_covers_the_canvas_and_nothing_else(void)
{
   canvas_fill(&s_cv, 5);
   TEST_ASSERT_EQUAL_INT(W * H, count_index(5));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_EXACT, tg(W - 1, H - 1));
   assert_borders_intact();
}

void test_putpixel_outside_is_ignored(void)
{
   canvas_putpixel(&s_cv, -1, 0, 9);
   canvas_putpixel(&s_cv, W, 0, 9);
   canvas_putpixel(&s_cv, 0, -1, 9);
   canvas_putpixel(&s_cv, 0, H, 9);
   canvas_putpixel(&s_cv, INT32_MIN, INT32_MAX, 9);
   TEST_ASSERT_EQUAL_INT(0, count_index(9));
   canvas_putpixel(&s_cv, 3, 4, 9);
   TEST_ASSERT_EQUAL_HEX8(9, px(3, 4));
   assert_borders_intact();
}

void test_hline_clips_left_and_right(void)
{
   canvas_hline(&s_cv, -5, W + 5, 2, 7);
   TEST_ASSERT_EQUAL_INT(W, count_index(7));
   TEST_ASSERT_EQUAL_HEX8(7, px(0, 2));
   TEST_ASSERT_EQUAL_HEX8(7, px(W - 1, 2));
   assert_borders_intact();
}

void test_hline_accepts_reversed_endpoints(void)
{
   canvas_hline(&s_cv, 5, 2, 1, 3);
   TEST_ASSERT_EQUAL_INT(4, count_index(3));
   TEST_ASSERT_EQUAL_HEX8(3, px(2, 1));
   TEST_ASSERT_EQUAL_HEX8(3, px(5, 1));
}

void test_vline_clips_top_and_bottom(void)
{
   canvas_vline(&s_cv, 3, -10, H + 10, 4);
   TEST_ASSERT_EQUAL_INT(H, count_index(4));
   assert_borders_intact();
}

void test_line_is_bresenham_and_tagged(void)
{
   int i;
   canvas_line(&s_cv, 0, 0, 5, 5, 6);
   TEST_ASSERT_EQUAL_INT(6, count_index(6));
   for (i = 0; i <= 5; i++)
   {
      TEST_ASSERT_EQUAL_HEX8(6, px(i, i));
      TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_LINE, tg(i, i));
   }
}

void test_line_crossing_the_canvas_is_clipped(void)
{
   canvas_line(&s_cv, -8, 3, W + 8, 3, 6);
   TEST_ASSERT_EQUAL_INT(W, count_index(6));
   assert_borders_intact();
}

void test_absurd_lines_write_nothing_outside(void)
{
   canvas_line(&s_cv, -100, -100, -50, -1, 6);
   TEST_ASSERT_EQUAL_INT(0, count_index(6));
   canvas_line(&s_cv, INT32_MIN / 2, 0, INT32_MAX / 2, 0, 6);
   canvas_line(&s_cv, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, 6);
   assert_borders_intact();
}

void test_rect_outline_is_inclusive(void)
{
   canvas_rect(&s_cv, 1, 1, 4, 3, 8);
   TEST_ASSERT_EQUAL_INT(10, count_index(8));
   TEST_ASSERT_EQUAL_HEX8(8, px(4, 3));
   TEST_ASSERT_EQUAL_HEX8(0, px(2, 2));
}

void test_rectfill_clips_all_four_edges(void)
{
   canvas_rectfill(&s_cv, -3, -3, W + 3, H + 3, 2);
   TEST_ASSERT_EQUAL_INT(W * H, count_index(2));
   assert_borders_intact();
}

void test_clip_rect_limits_every_primitive(void)
{
   canvas_set_clip(&s_cv, 4, 4, 8, 8);
   canvas_fill(&s_cv, 1);
   TEST_ASSERT_EQUAL_INT(16, count_index(1));
   canvas_rectfill(&s_cv, 0, 0, W - 1, H - 1, 3);
   TEST_ASSERT_EQUAL_INT(16, count_index(3));
   canvas_line(&s_cv, 0, 0, W - 1, H - 1, 6);
   TEST_ASSERT_EQUAL_INT(4, count_index(6));
   canvas_reset_clip(&s_cv);
   TEST_ASSERT_EQUAL_INT32(W, s_cv.clip.x1);
}

void test_set_clip_is_clamped_to_the_buffer(void)
{
   canvas_set_clip(&s_cv, -10, -10, W + 10, H + 10);
   TEST_ASSERT_EQUAL_INT32(0, s_cv.clip.x0);
   TEST_ASSERT_EQUAL_INT32(0, s_cv.clip.y0);
   TEST_ASSERT_EQUAL_INT32(W, s_cv.clip.x1);
   TEST_ASSERT_EQUAL_INT32(H, s_cv.clip.y1);
   canvas_set_clip(&s_cv, 10, 10, 2, 2);
   TEST_ASSERT_EQUAL_INT32(s_cv.clip.x0, s_cv.clip.x1);
   canvas_fill(&s_cv, 4);
   TEST_ASSERT_EQUAL_INT(0, count_index(4));
}

void test_create_rejects_bad_sizes(void)
{
   DS1_CANVAS cv;
   TEST_ASSERT_EQUAL_INT(0, canvas_create(&cv, 0, 5, 0));
   TEST_ASSERT_EQUAL_INT(0, canvas_create(&cv, 5, -1, 0));
   TEST_ASSERT_NULL(cv.pixels);
}

void test_dump_round_trips_header_indices_and_tags(void)
{
   DS1_CANVAS cv;
   uint8_t    got[17 + 6 + 6];
   FILE      *f;
   size_t     n;

   TEST_ASSERT_EQUAL_INT(1, canvas_create(&cv, 3, 2, 1));
   canvas_putpixel(&cv, 2, 1, 77);
   canvas_line(&cv, 0, 0, 0, 0, 5);
   TEST_ASSERT_EQUAL_INT(1, canvas_write_dump(&cv, "test_render_canvas_dump.bin"));
   canvas_destroy(&cv);

   f = fopen("test_render_canvas_dump.bin", "rb");
   TEST_ASSERT_NOT_NULL(f);
   n = fread(got, 1, sizeof(got), f);
   fclose(f);
   remove("test_render_canvas_dump.bin");

   TEST_ASSERT_EQUAL_size_t(sizeof(got), n);
   TEST_ASSERT_EQUAL_MEMORY("DS1IDX1", got, 8);
   TEST_ASSERT_EQUAL_HEX8(3, got[8]);
   TEST_ASSERT_EQUAL_HEX8(0, got[9]);
   TEST_ASSERT_EQUAL_HEX8(2, got[12]);
   TEST_ASSERT_EQUAL_HEX8(1, got[16]);
   TEST_ASSERT_EQUAL_HEX8(5, got[17 + 0]);          /* (0,0) index */
   TEST_ASSERT_EQUAL_HEX8(77, got[17 + 5]);         /* (2,1) index */
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_LINE, got[23 + 0]);
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_EXACT, got[23 + 5]);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_fill_covers_the_canvas_and_nothing_else);
   RUN_TEST(test_putpixel_outside_is_ignored);
   RUN_TEST(test_hline_clips_left_and_right);
   RUN_TEST(test_hline_accepts_reversed_endpoints);
   RUN_TEST(test_vline_clips_top_and_bottom);
   RUN_TEST(test_line_is_bresenham_and_tagged);
   RUN_TEST(test_line_crossing_the_canvas_is_clipped);
   RUN_TEST(test_absurd_lines_write_nothing_outside);
   RUN_TEST(test_rect_outline_is_inclusive);
   RUN_TEST(test_rectfill_clips_all_four_edges);
   RUN_TEST(test_clip_rect_limits_every_primitive);
   RUN_TEST(test_set_clip_is_clamped_to_the_buffer);
   RUN_TEST(test_create_rejects_bad_sizes);
   RUN_TEST(test_dump_round_trips_header_indices_and_tags);
   return UNITY_END();
}
```

- [ ] **Step 3: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/canvas.c` (the test target names a file that does not exist yet).

- [ ] **Step 4: Write the implementation**

Create `src/render/canvas.h`:

```c
/* 8-bit palette-index canvas: the software renderer's drawing surface.
 *
 * Every primitive clips against cv->clip, and canvas_set_clip() keeps the clip
 * inside the pixel buffer, so no caller-supplied coordinate can write out of
 * bounds -- sprite offsets in particular are never trusted. The optional tag
 * plane records how each pixel was last written, for the parity harness:
 * CANVAS_TAG_EXACT (copy or fill), CANVAS_TAG_TABLE (through a colour table),
 * CANVAS_TAG_LINE (canvas_line). No Allegro dependency. */

#ifndef DS1EDIT_RENDER_CANVAS_H
#define DS1EDIT_RENDER_CANVAS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CANVAS_TAG_EXACT 0
#define CANVAS_TAG_TABLE 1
#define CANVAS_TAG_LINE  2

/* Longest line canvas_line() will rasterize, in either axis. Anything longer
   comes from a corrupt coordinate and is refused rather than walked. */
#define CANVAS_MAX_LINE_SPAN (1 << 20)

typedef struct DS1_RECT
{
   int32_t x0, y0, x1, y1;          /* half-open: [x0, x1) x [y0, y1) */
} DS1_RECT;

typedef struct DS1_CANVAS
{
   uint8_t *pixels;                 /* stride * h palette indices          */
   uint8_t *tags;                   /* optional, same layout; NULL = off   */
   int32_t  w, h, stride;
   DS1_RECT clip;
   uint8_t *owned;                  /* allocation made by canvas_create    */
} DS1_CANVAS;

/* A sprite in game-style RLE. Row y's stream is data[row[y] .. row[y+1]):
   repeated (skip u8, count u8, count index bytes). Trailing transparency is
   implicit. Built by rle_encode() in spritecache.c. */
typedef struct DS1_RLE
{
   int32_t   w, h;
   int32_t   origin_x, origin_y;    /* sprite-space position of pixel (0,0) */
   uint32_t *row;                   /* h + 1 offsets into data              */
   uint8_t  *data;
   size_t    data_len;
   int32_t   opaque_count;          /* non-zero pixels                      */
} DS1_RLE;

void canvas_init_view(DS1_CANVAS *cv, uint8_t *pixels, uint8_t *tags,
                      int32_t w, int32_t h, int32_t stride);
int  canvas_create(DS1_CANVAS *cv, int32_t w, int32_t h, int with_tags);
void canvas_destroy(DS1_CANVAS *cv);

void canvas_set_clip(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void canvas_reset_clip(DS1_CANVAS *cv);

void canvas_fill(DS1_CANVAS *cv, uint8_t c);
void canvas_putpixel(DS1_CANVAS *cv, int32_t x, int32_t y, uint8_t c);
void canvas_hline(DS1_CANVAS *cv, int32_t x0, int32_t x1, int32_t y, uint8_t c);
void canvas_vline(DS1_CANVAS *cv, int32_t x, int32_t y0, int32_t y1, uint8_t c);
void canvas_line(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c);
void canvas_rect(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c);
void canvas_rectfill(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c);

/* Write the indices (and tags) to `path` in the DS1IDX1 format read by
   scripts/equivalence/renderer_parity.py. 1 on success. */
int  canvas_write_dump(const DS1_CANVAS *cv, const char *path);

#ifdef __cplusplus
}
#endif

#endif
```

Create `src/render/canvas.c`:

```c
/* See canvas.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/canvas.h"

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
   if (v < lo)
      return lo;
   if (v > hi)
      return hi;
   return v;
}

static int in_clip(const DS1_CANVAS *cv, int64_t x, int64_t y)
{
   return (x >= cv->clip.x0) && (x < cv->clip.x1) &&
          (y >= cv->clip.y0) && (y < cv->clip.y1);
}

static void plot(DS1_CANVAS *cv, int64_t x, int64_t y, uint8_t c, uint8_t tag)
{
   size_t off;

   if (!in_clip(cv, x, y))
      return;
   off = (size_t) y * (size_t) cv->stride + (size_t) x;
   cv->pixels[off] = c;
   if (cv->tags != NULL)
      cv->tags[off] = tag;
}

static void span(DS1_CANVAS *cv, int32_t y, int32_t x0, int32_t x1, uint8_t c)
{
   size_t off = (size_t) y * (size_t) cv->stride + (size_t) x0;
   size_t n   = (size_t) (x1 - x0 + 1);

   memset(cv->pixels + off, c, n);
   if (cv->tags != NULL)
      memset(cv->tags + off, CANVAS_TAG_EXACT, n);
}

void canvas_init_view(DS1_CANVAS *cv, uint8_t *pixels, uint8_t *tags,
                      int32_t w, int32_t h, int32_t stride)
{
   if (cv == NULL)
      return;
   memset(cv, 0, sizeof(*cv));
   if ((pixels == NULL) || (w <= 0) || (h <= 0) || (stride < w))
      return;
   cv->pixels = pixels;
   cv->tags   = tags;
   cv->w      = w;
   cv->h      = h;
   cv->stride = stride;
   canvas_reset_clip(cv);
}

int canvas_create(DS1_CANVAS *cv, int32_t w, int32_t h, int with_tags)
{
   size_t   n;
   uint8_t *mem;

   if (cv == NULL)
      return 0;
   memset(cv, 0, sizeof(*cv));
   if ((w <= 0) || (h <= 0))
      return 0;
   if ((size_t) w > SIZE_MAX / 2 / (size_t) h)
      return 0;
   n   = (size_t) w * (size_t) h;
   mem = (uint8_t *) calloc(with_tags ? n * 2 : n, 1);
   if (mem == NULL)
      return 0;
   canvas_init_view(cv, mem, with_tags ? mem + n : NULL, w, h, w);
   cv->owned = mem;
   return 1;
}

void canvas_destroy(DS1_CANVAS *cv)
{
   if (cv == NULL)
      return;
   free(cv->owned);
   memset(cv, 0, sizeof(*cv));
}

void canvas_reset_clip(DS1_CANVAS *cv)
{
   if (cv == NULL)
      return;
   cv->clip.x0 = 0;
   cv->clip.y0 = 0;
   cv->clip.x1 = cv->w;
   cv->clip.y1 = cv->h;
}

void canvas_set_clip(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
   if (cv == NULL)
      return;
   cv->clip.x0 = clamp32(x0, 0, cv->w);
   cv->clip.y0 = clamp32(y0, 0, cv->h);
   cv->clip.x1 = clamp32(x1, cv->clip.x0, cv->w);
   cv->clip.y1 = clamp32(y1, cv->clip.y0, cv->h);
}

void canvas_fill(DS1_CANVAS *cv, uint8_t c)
{
   int32_t y;

   if ((cv == NULL) || (cv->pixels == NULL) || (cv->clip.x1 <= cv->clip.x0))
      return;
   for (y = cv->clip.y0; y < cv->clip.y1; y++)
      span(cv, y, cv->clip.x0, cv->clip.x1 - 1, c);
}

void canvas_putpixel(DS1_CANVAS *cv, int32_t x, int32_t y, uint8_t c)
{
   if ((cv == NULL) || (cv->pixels == NULL))
      return;
   plot(cv, x, y, c, CANVAS_TAG_EXACT);
}

void canvas_hline(DS1_CANVAS *cv, int32_t x0, int32_t x1, int32_t y, uint8_t c)
{
   int32_t t;

   if ((cv == NULL) || (cv->pixels == NULL))
      return;
   if (x0 > x1)
   {
      t  = x0;
      x0 = x1;
      x1 = t;
   }
   if ((y < cv->clip.y0) || (y >= cv->clip.y1))
      return;
   if ((x1 < cv->clip.x0) || (x0 >= cv->clip.x1))
      return;
   if (x0 < cv->clip.x0)
      x0 = cv->clip.x0;
   if (x1 > cv->clip.x1 - 1)
      x1 = cv->clip.x1 - 1;
   span(cv, y, x0, x1, c);
}

void canvas_vline(DS1_CANVAS *cv, int32_t x, int32_t y0, int32_t y1, uint8_t c)
{
   int32_t t, y;

   if ((cv == NULL) || (cv->pixels == NULL))
      return;
   if (y0 > y1)
   {
      t  = y0;
      y0 = y1;
      y1 = t;
   }
   if ((x < cv->clip.x0) || (x >= cv->clip.x1))
      return;
   if (y0 < cv->clip.y0)
      y0 = cv->clip.y0;
   if (y1 > cv->clip.y1 - 1)
      y1 = cv->clip.y1 - 1;
   for (y = y0; y <= y1; y++)
      plot(cv, x, y, c, CANVAS_TAG_EXACT);
}

void canvas_line(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{
   int64_t dx, dy, sx, sy, err, e2, x, y;

   if ((cv == NULL) || (cv->pixels == NULL))
      return;
   if (((x0 < cv->clip.x0) && (x1 < cv->clip.x0)) ||
       ((x0 >= cv->clip.x1) && (x1 >= cv->clip.x1)) ||
       ((y0 < cv->clip.y0) && (y1 < cv->clip.y0)) ||
       ((y0 >= cv->clip.y1) && (y1 >= cv->clip.y1)))
      return;

   dx = (int64_t) x1 - x0;
   if (dx < 0)
      dx = -dx;
   dy = (int64_t) y1 - y0;
   if (dy < 0)
      dy = -dy;
   if ((dx > CANVAS_MAX_LINE_SPAN) || (dy > CANVAS_MAX_LINE_SPAN))
      return;

   sx  = (x0 < x1) ? 1 : -1;
   sy  = (y0 < y1) ? 1 : -1;
   err = dx - dy;
   x   = x0;
   y   = y0;
   for (;;)
   {
      plot(cv, x, y, c, CANVAS_TAG_LINE);
      if ((x == x1) && (y == y1))
         break;
      e2 = 2 * err;
      if (e2 > -dy)
      {
         err -= dy;
         x += sx;
      }
      if (e2 < dx)
      {
         err += dx;
         y += sy;
      }
   }
}

void canvas_rect(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{
   canvas_hline(cv, x0, x1, y0, c);
   canvas_hline(cv, x0, x1, y1, c);
   canvas_vline(cv, x0, y0, y1, c);
   canvas_vline(cv, x1, y0, y1, c);
}

void canvas_rectfill(DS1_CANVAS *cv, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{
   int32_t t, y;

   if ((cv == NULL) || (cv->pixels == NULL))
      return;
   if (x0 > x1)
   {
      t  = x0;
      x0 = x1;
      x1 = t;
   }
   if (y0 > y1)
   {
      t  = y0;
      y0 = y1;
      y1 = t;
   }
   if ((x1 < cv->clip.x0) || (x0 >= cv->clip.x1) ||
       (y1 < cv->clip.y0) || (y0 >= cv->clip.y1))
      return;
   if (x0 < cv->clip.x0)
      x0 = cv->clip.x0;
   if (x1 > cv->clip.x1 - 1)
      x1 = cv->clip.x1 - 1;
   if (y0 < cv->clip.y0)
      y0 = cv->clip.y0;
   if (y1 > cv->clip.y1 - 1)
      y1 = cv->clip.y1 - 1;
   for (y = y0; y <= y1; y++)
      span(cv, y, x0, x1, c);
}

static void put_le32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t) v;
   p[1] = (uint8_t) (v >> 8);
   p[2] = (uint8_t) (v >> 16);
   p[3] = (uint8_t) (v >> 24);
}

int canvas_write_dump(const DS1_CANVAS *cv, const char *path)
{
   static const uint8_t magic[8] = { 'D', 'S', '1', 'I', 'D', 'X', '1', 0 };
   uint8_t hdr[17];
   FILE   *f;
   int32_t y;
   int     ok;

   if ((cv == NULL) || (cv->pixels == NULL) || (path == NULL))
      return 0;
   memcpy(hdr, magic, sizeof(magic));
   put_le32(hdr + 8, (uint32_t) cv->w);
   put_le32(hdr + 12, (uint32_t) cv->h);
   hdr[16] = (cv->tags != NULL) ? 1 : 0;

   f = fopen(path, "wb");
   if (f == NULL)
      return 0;
   ok = (fwrite(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
   for (y = 0; ok && (y < cv->h); y++)
      ok = (fwrite(cv->pixels + (size_t) y * cv->stride, 1, (size_t) cv->w, f)
            == (size_t) cv->w);
   for (y = 0; ok && (cv->tags != NULL) && (y < cv->h); y++)
      ok = (fwrite(cv->tags + (size_t) y * cv->stride, 1, (size_t) cv->w, f)
            == (size_t) cv->w);
   if (fclose(f) != 0)
      ok = 0;
   return ok;
}
```

Add the file to `SOURCES` in `CMakeLists.txt`, under the existing `# render/` entries:

```cmake
    # render/
    src/render/preview.c
    src/render/gfx.c
    src/render/canvas.c
```

- [ ] **Step 5: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_canvas
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_canvas$" --output-on-failure
```
Expected: `14 Tests 0 Failures 0 Ignored` in the test output and `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 6: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/canvas.h src/render/canvas.c test/test_render_canvas.c test/cmake/software_renderer.cmake test/CMakeLists.txt CMakeLists.txt
git commit -m "Give the software renderer an 8-bit canvas that cannot write outside itself"
```

---

### Task 2: RLE encoding and per-zoom copies

**Files:**
- Create: `src/render/spritecache.h`, `src/render/spritecache.c`, `test/test_render_rle.c`
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: `DS1_RLE` (Task 1); `DS1EDIT_INDEX_FRAME` (`src/core/index_frame.h`); `void index_buf_scale_down(const unsigned char *src, int src_w, int src_h, unsigned char *dst, int dst_w, int dst_h)` (declared in `src/core/dt1_draw.h`, implemented in `src/core/dt1_decode.c`).
- Produces (`src/render/spritecache.h`):
  ```c
  enum { SC_ZOOM_21 = 0, SC_ZOOM_11, SC_ZOOM_12, SC_ZOOM_14, SC_ZOOM_18, SC_ZOOM_116, SC_ZOOM_MAX };  /* == ZOOM_E */
  int  sc_zoom_ratio(int zoom, int *mul, int *div);                                        /* 1 valid */
  int  sc_zoom_frame(const DS1EDIT_INDEX_FRAME *src, int zoom, DS1EDIT_INDEX_FRAME *dst);  /* 1 ok; dst->pixels malloc'd */
  int  rle_encode(const DS1EDIT_INDEX_FRAME *f, DS1_RLE *out);                             /* 1 ok */
  void rle_free(DS1_RLE *r);
  int  rle_decode(const DS1_RLE *r, uint8_t *dst, int32_t dst_stride);                     /* 1 ok */
  ```

- [ ] **Step 1: Write the failing test**

Create `test/test_render_rle.c`:

```c
/* Index frame -> game-style RLE, and the per-zoom copies (spritecache.c).
 *
 * The expected byte streams and decimations are written out by hand from the
 * format rules in canvas.h and the spec ("per-zoom RLE copies made by
 * nearest-pixel decimation (2:1 by pixel doubling)"). */

#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "render/spritecache.h"

void setUp(void) {}
void tearDown(void) {}

static DS1EDIT_INDEX_FRAME frame_view(uint8_t *px, int32_t w, int32_t h, int32_t stride)
{
   DS1EDIT_INDEX_FRAME f;
   memset(&f, 0, sizeof(f));
   f.width = w;
   f.height = h;
   f.stride = stride;
   f.pixels = px;
   f.pixels_len = (size_t) stride * (size_t) h;
   return f;
}

void test_rle_mixed_row_bytes_and_round_trip(void)
{
   uint8_t row[8] = { 0, 0, 5, 6, 0, 7, 0, 0 };
   uint8_t expect[7] = { 2, 2, 5, 6, 1, 1, 7 };
   uint8_t back[8];
   DS1EDIT_INDEX_FRAME f = frame_view(row, 8, 1, 8);
   DS1_RLE r;

   TEST_ASSERT_EQUAL_INT(1, rle_encode(&f, &r));
   TEST_ASSERT_EQUAL_size_t(7, r.data_len);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, r.data, 7);
   TEST_ASSERT_EQUAL_UINT32(0, r.row[0]);
   TEST_ASSERT_EQUAL_UINT32(7, r.row[1]);
   TEST_ASSERT_EQUAL_INT32(3, r.opaque_count);
   memset(back, 0xEE, sizeof(back));
   TEST_ASSERT_EQUAL_INT(1, rle_decode(&r, back, 8));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(row, back, 8);
   rle_free(&r);
}

void test_rle_long_runs_split_at_255(void)
{
   uint8_t *row = (uint8_t *) calloc(600, 1);
   uint8_t *back = (uint8_t *) malloc(600);
   DS1EDIT_INDEX_FRAME f;
   DS1_RLE r;

   memset(row + 300, 3, 300);
   f = frame_view(row, 600, 1, 600);
   TEST_ASSERT_EQUAL_INT(1, rle_encode(&f, &r));
   /* skip 300 = (255, 0) then (45, 255 px); then (0, 45 px) */
   TEST_ASSERT_EQUAL_size_t(2 + 2 + 255 + 2 + 45, r.data_len);
   TEST_ASSERT_EQUAL_HEX8(255, r.data[0]);
   TEST_ASSERT_EQUAL_HEX8(0, r.data[1]);
   TEST_ASSERT_EQUAL_HEX8(45, r.data[2]);
   TEST_ASSERT_EQUAL_HEX8(255, r.data[3]);
   TEST_ASSERT_EQUAL_HEX8(0, r.data[4 + 255]);
   TEST_ASSERT_EQUAL_HEX8(45, r.data[5 + 255]);
   TEST_ASSERT_EQUAL_INT(1, rle_decode(&r, back, 600));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(row, back, 600);
   rle_free(&r);
   free(row);
   free(back);
}

void test_rle_transparent_rows_are_empty(void)
{
   uint8_t px[12] = { 0 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 4, 3, 4);
   DS1_RLE r;
   int y;

   TEST_ASSERT_EQUAL_INT(1, rle_encode(&f, &r));
   TEST_ASSERT_EQUAL_size_t(0, r.data_len);
   TEST_ASSERT_EQUAL_INT32(0, r.opaque_count);
   for (y = 0; y <= 3; y++)
      TEST_ASSERT_EQUAL_UINT32(0, r.row[y]);
   rle_free(&r);
}

void test_rle_respects_stride(void)
{
   /* width 4, stride 10: the padding holds garbage that must not leak in */
   uint8_t px[20] = { 1, 0, 2, 3, 9, 9, 9, 9, 9, 9,
                      0, 4, 0, 5, 9, 9, 9, 9, 9, 9 };
   uint8_t expect[8] = { 1, 0, 2, 3, 0, 4, 0, 5 };
   uint8_t back[8];
   DS1EDIT_INDEX_FRAME f = frame_view(px, 4, 2, 10);
   DS1_RLE r;

   TEST_ASSERT_EQUAL_INT(1, rle_encode(&f, &r));
   TEST_ASSERT_EQUAL_INT(1, rle_decode(&r, back, 4));
   TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, back, 8);
   rle_free(&r);
}

void test_rle_rejects_bad_frames(void)
{
   uint8_t px[4] = { 1, 2, 3, 4 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 4, 1, 4);
   DS1_RLE r;

   TEST_ASSERT_EQUAL_INT(0, rle_encode(NULL, &r));
   f.width = 0;
   TEST_ASSERT_EQUAL_INT(0, rle_encode(&f, &r));
   f.width = 4;
   f.stride = 3;
   TEST_ASSERT_EQUAL_INT(0, rle_encode(&f, &r));
   f.stride = 4;
   f.pixels = NULL;
   TEST_ASSERT_EQUAL_INT(0, rle_encode(&f, &r));
}

void test_zoom_ratios(void)
{
   int mul, div;
   TEST_ASSERT_EQUAL_INT(1, sc_zoom_ratio(SC_ZOOM_21, &mul, &div));
   TEST_ASSERT_EQUAL_INT(2, mul);
   TEST_ASSERT_EQUAL_INT(1, div);
   TEST_ASSERT_EQUAL_INT(1, sc_zoom_ratio(SC_ZOOM_116, &mul, &div));
   TEST_ASSERT_EQUAL_INT(1, mul);
   TEST_ASSERT_EQUAL_INT(16, div);
   TEST_ASSERT_EQUAL_INT(0, sc_zoom_ratio(SC_ZOOM_MAX, &mul, &div));
   TEST_ASSERT_EQUAL_INT(0, sc_zoom_ratio(-1, &mul, &div));
}

void test_zoom_11_is_a_copy(void)
{
   uint8_t px[6] = { 1, 2, 3, 4, 5, 6 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 3, 2, 3), z;

   TEST_ASSERT_EQUAL_INT(1, sc_zoom_frame(&f, SC_ZOOM_11, &z));
   TEST_ASSERT_EQUAL_INT32(3, z.width);
   TEST_ASSERT_EQUAL_INT32(2, z.height);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(px, z.pixels, 6);
   TEST_ASSERT_TRUE(z.pixels != px);
   free(z.pixels);
}

void test_zoom_21_doubles_pixels(void)
{
   uint8_t px[2] = { 1, 2 };
   uint8_t expect[8] = { 1, 1, 2, 2, 1, 1, 2, 2 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 2, 1, 2), z;

   TEST_ASSERT_EQUAL_INT(1, sc_zoom_frame(&f, SC_ZOOM_21, &z));
   TEST_ASSERT_EQUAL_INT32(4, z.width);
   TEST_ASSERT_EQUAL_INT32(2, z.height);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, z.pixels, 8);
   free(z.pixels);
}

void test_zoom_out_levels_match_reference_decimation(void)
{
   static const int zooms[4] = { SC_ZOOM_12, SC_ZOOM_14, SC_ZOOM_18, SC_ZOOM_116 };
   static uint8_t px[160 * 80];
   DS1EDIT_INDEX_FRAME f = frame_view(px, 160, 80, 160), z;
   int i, x, y, mul, div;

   for (y = 0; y < 80; y++)
      for (x = 0; x < 160; x++)
         px[y * 160 + x] = (uint8_t) (((x * 7 + y * 13) % 255) + 1);

   for (i = 0; i < 4; i++)
   {
      TEST_ASSERT_EQUAL_INT(1, sc_zoom_ratio(zooms[i], &mul, &div));
      TEST_ASSERT_EQUAL_INT(1, sc_zoom_frame(&f, zooms[i], &z));
      TEST_ASSERT_EQUAL_INT32(160 / div, z.width);
      TEST_ASSERT_EQUAL_INT32(80 / div, z.height);
      for (y = 0; y < z.height; y++)
         for (x = 0; x < z.width; x++)
            TEST_ASSERT_EQUAL_HEX8(px[(y * 80 / z.height) * 160 + (x * 160 / z.width)],
                                   z.pixels[y * z.width + x]);
      free(z.pixels);
   }
}

void test_zoom_that_decimates_to_nothing_is_refused(void)
{
   uint8_t px[64] = { 1 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 8, 8, 8), z;
   TEST_ASSERT_EQUAL_INT(0, sc_zoom_frame(&f, SC_ZOOM_116, &z));
   TEST_ASSERT_NULL(z.pixels);
}

void test_zoom_scales_origin_with_floor_division(void)
{
   uint8_t px[16] = { 1 };
   DS1EDIT_INDEX_FRAME f = frame_view(px, 4, 4, 4), z;

   f.origin_x = -5;
   f.origin_y = 5;
   TEST_ASSERT_EQUAL_INT(1, sc_zoom_frame(&f, SC_ZOOM_12, &z));
   TEST_ASSERT_EQUAL_INT32(-3, z.origin_x);
   TEST_ASSERT_EQUAL_INT32(2, z.origin_y);
   free(z.pixels);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_rle_mixed_row_bytes_and_round_trip);
   RUN_TEST(test_rle_long_runs_split_at_255);
   RUN_TEST(test_rle_transparent_rows_are_empty);
   RUN_TEST(test_rle_respects_stride);
   RUN_TEST(test_rle_rejects_bad_frames);
   RUN_TEST(test_zoom_ratios);
   RUN_TEST(test_zoom_11_is_a_copy);
   RUN_TEST(test_zoom_21_doubles_pixels);
   RUN_TEST(test_zoom_out_levels_match_reference_decimation);
   RUN_TEST(test_zoom_that_decimates_to_nothing_is_refused);
   RUN_TEST(test_zoom_scales_origin_with_floor_division);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 2: RLE encoding and zoom copies ------------------------------------
ds1edit_sw_pure_test(test_render_rle test_render_rle.c
    ${_sw}/spritecache.c ${_core}/dt1_decode.c)
ds1edit_sw_add_test(render_rle test_render_rle)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/spritecache.c`.

- [ ] **Step 3: Write the implementation**

Create `src/render/spritecache.h`:

```c
/* Sprite cache for the software renderer.
 *
 * Converts the decoders' raw palette-index frames (the DS1EDIT_INDEX_FRAME
 * seam, src/core/index_frame.h; DT1 blocks and COF layer frames arrive as
 * CACHED_TILE index buffers wrapped in a frame view) into game-style RLE, one
 * copy per asset per zoom level, built on first use and freed when the source
 * buffers are freed (spritecache_clear, Task 4). No Allegro dependency. */

#ifndef DS1EDIT_RENDER_SPRITECACHE_H
#define DS1EDIT_RENDER_SPRITECACHE_H

#include <stddef.h>
#include <stdint.h>

#include "core/index_frame.h"
#include "render/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Same values as ZOOM_E in structs.h (map_sw.c asserts it at compile time);
   this header stays free of the editor's Allegro-dependent structs. */
enum
{
   SC_ZOOM_21 = 0,   /* 2:1, pixel doubling */
   SC_ZOOM_11,
   SC_ZOOM_12,
   SC_ZOOM_14,
   SC_ZOOM_18,
   SC_ZOOM_116,
   SC_ZOOM_MAX
};

int  sc_zoom_ratio(int zoom, int *mul, int *div);

/* Nearest-pixel copy of `src` at `zoom` (2:1 doubles; 1:N samples
   src(x*sw/dw, y*sh/dh), the rule of index_buf_scale_down). dst->pixels is
   malloc'd; free() it. Returns 0 (dst zeroed) for invalid input or a copy that
   decimates to nothing. */
int  sc_zoom_frame(const DS1EDIT_INDEX_FRAME *src, int zoom, DS1EDIT_INDEX_FRAME *dst);

int  rle_encode(const DS1EDIT_INDEX_FRAME *f, DS1_RLE *out);
void rle_free(DS1_RLE *r);

/* Expand into dst (w x h at dst_stride); index 0 where the sprite is
   transparent. Test and debugging helper. */
int  rle_decode(const DS1_RLE *r, uint8_t *dst, int32_t dst_stride);

#ifdef __cplusplus
}
#endif

#endif
```

Create `src/render/spritecache.c`:

```c
/* See spritecache.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/spritecache.h"
#include "core/dt1_draw.h"

/* Wider than any real sprite; protects the w * 2 and w * h arithmetic. */
#define SC_MAX_DIM (1 << 15)

int sc_zoom_ratio(int zoom, int *mul, int *div)
{
   static const int k_mul[SC_ZOOM_MAX] = { 2, 1, 1, 1, 1, 1 };
   static const int k_div[SC_ZOOM_MAX] = { 1, 1, 2, 4, 8, 16 };

   if ((zoom < 0) || (zoom >= SC_ZOOM_MAX) || (mul == NULL) || (div == NULL))
      return 0;
   *mul = k_mul[zoom];
   *div = k_div[zoom];
   return 1;
}

static int32_t floor_div(int32_t a, int32_t b)
{
   int32_t q = a / b;
   if (((a % b) != 0) && ((a < 0) != (b < 0)))
      q--;
   return q;
}

static int frame_ok(const DS1EDIT_INDEX_FRAME *f)
{
   return (f != NULL) && (f->pixels != NULL) && (f->width > 0) && (f->height > 0) &&
          (f->width <= SC_MAX_DIM) && (f->height <= SC_MAX_DIM) &&
          (f->stride >= f->width);
}

int sc_zoom_frame(const DS1EDIT_INDEX_FRAME *src, int zoom, DS1EDIT_INDEX_FRAME *dst)
{
   int            mul, div;
   int32_t        w, h, x, y;
   uint8_t       *packed = NULL;
   const uint8_t *s;

   if (dst == NULL)
      return 0;
   memset(dst, 0, sizeof(*dst));
   if (!frame_ok(src) || !sc_zoom_ratio(zoom, &mul, &div))
      return 0;
   w = src->width * mul / div;
   h = src->height * mul / div;
   if ((w <= 0) || (h <= 0))
      return 0;

   dst->pixels = (uint8_t *) malloc((size_t) w * (size_t) h);
   if (dst->pixels == NULL)
      return 0;
   dst->width      = w;
   dst->height     = h;
   dst->stride     = w;
   dst->pixels_len = (size_t) w * (size_t) h;
   dst->origin_x   = floor_div(src->origin_x * mul, div);
   dst->origin_y   = floor_div(src->origin_y * mul, div);
   dst->frame      = src->frame;
   dst->direction  = src->direction;

   if (mul == 2)
   {
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
            dst->pixels[(size_t) y * w + x] =
               src->pixels[(size_t) (y / 2) * src->stride + (size_t) (x / 2)];
      return 1;
   }
   if (div == 1)
   {
      for (y = 0; y < h; y++)
         memcpy(dst->pixels + (size_t) y * w,
                src->pixels + (size_t) y * src->stride, (size_t) w);
      return 1;
   }

   s = src->pixels;
   if (src->stride != src->width)
   {
      packed = (uint8_t *) malloc((size_t) src->width * (size_t) src->height);
      if (packed == NULL)
      {
         free(dst->pixels);
         memset(dst, 0, sizeof(*dst));
         return 0;
      }
      for (y = 0; y < src->height; y++)
         memcpy(packed + (size_t) y * src->width,
                src->pixels + (size_t) y * src->stride, (size_t) src->width);
      s = packed;
   }
   index_buf_scale_down(s, src->width, src->height, dst->pixels, w, h);
   free(packed);
   return 1;
}

int rle_encode(const DS1EDIT_INDEX_FRAME *f, DS1_RLE *out)
{
   size_t   cap;
   uint8_t *p, *data;
   int32_t  x, y;

   if (out == NULL)
      return 0;
   memset(out, 0, sizeof(*out));
   if (!frame_ok(f))
      return 0;

   /* Worst case per row: alternating pixels cost 3 bytes per 2 pixels, a long
      transparent gap 2 bytes per 255; 2w + 4 covers both. */
   cap  = (size_t) f->height * ((size_t) f->width * 2 + 4);
   data = (uint8_t *) malloc(cap > 0 ? cap : 1);
   out->row = (uint32_t *) malloc(((size_t) f->height + 1) * sizeof(uint32_t));
   if ((data == NULL) || (out->row == NULL))
   {
      free(data);
      free(out->row);
      memset(out, 0, sizeof(*out));
      return 0;
   }

   p = data;
   for (y = 0; y < f->height; y++)
   {
      const uint8_t *src = f->pixels + (size_t) y * f->stride;

      out->row[y] = (uint32_t) (p - data);
      x = 0;
      while (x < f->width)
      {
         int32_t skip = 0, run = 0, start, first = 1;

         while ((x < f->width) && (src[x] == 0))
         {
            x++;
            skip++;
         }
         if (x >= f->width)
            break;                          /* trailing transparency: implicit */
         start = x;
         while ((x < f->width) && (src[x] != 0))
         {
            x++;
            run++;
         }
         out->opaque_count += run;
         while (skip > 255)
         {
            *p++ = 255;
            *p++ = 0;
            skip -= 255;
         }
         while (run > 0)
         {
            int32_t n = (run > 255) ? 255 : run;
            *p++ = (uint8_t) (first ? skip : 0);
            *p++ = (uint8_t) n;
            memcpy(p, src + start, (size_t) n);
            p += n;
            start += n;
            run -= n;
            first = 0;
         }
      }
   }
   out->row[f->height] = (uint32_t) (p - data);

   out->data_len = (size_t) (p - data);
   out->data     = (uint8_t *) realloc(data, out->data_len > 0 ? out->data_len : 1);
   if (out->data == NULL)
      out->data = data;                     /* shrinking failed: keep the big block */
   out->w        = f->width;
   out->h        = f->height;
   out->origin_x = f->origin_x;
   out->origin_y = f->origin_y;
   return 1;
}

void rle_free(DS1_RLE *r)
{
   if (r == NULL)
      return;
   free(r->data);
   free(r->row);
   memset(r, 0, sizeof(*r));
}

int rle_decode(const DS1_RLE *r, uint8_t *dst, int32_t dst_stride)
{
   int32_t y;

   if ((r == NULL) || (dst == NULL) || (r->row == NULL) || (dst_stride < r->w))
      return 0;
   for (y = 0; y < r->h; y++)
   {
      const uint8_t *p   = r->data + r->row[y];
      const uint8_t *end = r->data + r->row[y + 1];
      uint8_t       *out = dst + (size_t) y * dst_stride;
      int32_t        x   = 0;

      memset(out, 0, (size_t) r->w);
      while (p + 2 <= end)
      {
         int32_t n;
         x += p[0];
         n = p[1];
         p += 2;
         if ((x + n > r->w) || (n > end - p))
            return 0;
         memcpy(out + x, p, (size_t) n);
         p += n;
         x += n;
      }
   }
   return 1;
}
```

Add `src/render/spritecache.c` to `SOURCES` in `CMakeLists.txt` on the line after `src/render/canvas.c`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_rle
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_rle$" --output-on-failure
```
Expected: `11 Tests 0 Failures 0 Ignored`, `100% tests passed`.

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/spritecache.h src/render/spritecache.c test/test_render_rle.c test/cmake/software_renderer.cmake CMakeLists.txt
git commit -m "Encode index frames as game-style RLE, with one copy per zoom level"
```

---

### Task 3: RLE blits (opaque, flip, colormap, source table, destination table)

**Files:**
- Modify: `src/render/canvas.h`, `src/render/canvas.c`
- Create: `test/test_render_blit.c`
- Modify: `test/cmake/software_renderer.cmake`

**Interfaces:**
- Consumes: `DS1_CANVAS`, `DS1_RLE` (Task 1); `rle_encode`, `rle_free` (Task 2).
- Produces (`src/render/canvas.h`):
  ```c
  void canvas_blit_rle      (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip);
  void canvas_blit_rle_cmap (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip, const uint8_t *cmap);  /* dst = cmap[src*256 + dst] */
  void canvas_blit_rle_lit  (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip, const uint8_t *table); /* dst = table[src] */
  void canvas_blit_rle_shade(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip, const uint8_t *table); /* dst = table[dst] */
  ```
  Tables are passed flat (`&t[0][0]`), never as `const uint8_t (*)[256]`: C99 does not convert `uint8_t (*)[256]` to a pointer to a const-qualified array implicitly, and GCC warns on it under `-Wpedantic`.
  `(x, y)` is where sprite pixel (0,0) lands; with `flip` the sprite is mirrored horizontally inside the same `w`-wide box. Tags: opaque blit writes `CANVAS_TAG_EXACT`, the other three `CANVAS_TAG_TABLE`.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_blit.c`:

```c
/* RLE blits (canvas.c): placement, transparency, horizontal flip, clipping on
 * all four edges, absurd offsets, and the three colour-table modes. Sprites
 * are built with rle_encode from authored index frames. Same poisoned-border
 * harness as test_render_canvas.c. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "unity/unity.h"
#include "render/canvas.h"
#include "render/spritecache.h"

#define W      16
#define H      12
#define PAD    8
#define STRIDE (W + 2 * PAD)
#define POISON 0xAA
#define BG     9

static uint8_t    s_buf[(H + 2 * PAD) * STRIDE];
static uint8_t    s_tag[(H + 2 * PAD) * STRIDE];
static DS1_CANVAS s_cv;
static uint8_t    s_cmap[256 * 256];
static uint8_t    s_table[256];

void setUp(void)
{
   memset(s_buf, POISON, sizeof(s_buf));
   memset(s_tag, POISON, sizeof(s_tag));
   canvas_init_view(&s_cv, s_buf + PAD * STRIDE + PAD, s_tag + PAD * STRIDE + PAD,
                    W, H, STRIDE);
   canvas_fill(&s_cv, BG);
}

void tearDown(void) {}

static uint8_t px(int x, int y) { return s_cv.pixels[y * STRIDE + x]; }
static uint8_t tg(int x, int y) { return s_cv.tags[y * STRIDE + x]; }

static int count_index(uint8_t c)
{
   int x, y, n = 0;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         if (px(x, y) == c)
            n++;
   return n;
}

static void assert_borders_intact(void)
{
   int x, y;
   char msg[64];

   for (y = 0; y < H + 2 * PAD; y++)
      for (x = 0; x < STRIDE; x++)
      {
         int inside = (y >= PAD) && (y < PAD + H) && (x >= PAD) && (x < PAD + W);
         if (inside)
            continue;
         snprintf(msg, sizeof(msg), "border byte (%d,%d) overwritten", x, y);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, s_buf[y * STRIDE + x], msg);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, s_tag[y * STRIDE + x], msg);
      }
}

static DS1_RLE make_sprite(const uint8_t *px_in, int32_t w, int32_t h)
{
   DS1EDIT_INDEX_FRAME f;
   DS1_RLE r;

   memset(&f, 0, sizeof(f));
   f.width = w;
   f.height = h;
   f.stride = w;
   f.pixels = (uint8_t *) px_in;
   f.pixels_len = (size_t) w * h;
   TEST_ASSERT_EQUAL_INT(1, rle_encode(&f, &r));
   return r;
}

static const uint8_t k_small[6] = { 1, 0, 2,
                                    0, 3, 0 };

void test_opaque_blit_places_pixels_and_skips_transparency(void)
{
   DS1_RLE r = make_sprite(k_small, 3, 2);

   canvas_blit_rle(&s_cv, &r, 2, 1, 0);
   TEST_ASSERT_EQUAL_HEX8(1, px(2, 1));
   TEST_ASSERT_EQUAL_HEX8(BG, px(3, 1));
   TEST_ASSERT_EQUAL_HEX8(2, px(4, 1));
   TEST_ASSERT_EQUAL_HEX8(BG, px(2, 2));
   TEST_ASSERT_EQUAL_HEX8(3, px(3, 2));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_EXACT, tg(3, 2));
   TEST_ASSERT_EQUAL_INT(W * H - 3, count_index(BG));
   rle_free(&r);
}

void test_flip_mirrors_inside_the_same_box(void)
{
   DS1_RLE r = make_sprite(k_small, 3, 2);

   canvas_blit_rle(&s_cv, &r, 2, 1, 1);
   TEST_ASSERT_EQUAL_HEX8(2, px(2, 1));
   TEST_ASSERT_EQUAL_HEX8(BG, px(3, 1));
   TEST_ASSERT_EQUAL_HEX8(1, px(4, 1));
   TEST_ASSERT_EQUAL_HEX8(3, px(3, 2));
   rle_free(&r);
}

void test_clipping_on_all_four_edges_with_and_without_flip(void)
{
   uint8_t solid[16];
   DS1_RLE r;
   int flip;

   memset(solid, 7, sizeof(solid));
   r = make_sprite(solid, 4, 4);
   for (flip = 0; flip <= 1; flip++)
   {
      setUp();
      canvas_blit_rle(&s_cv, &r, -2, -2, flip);          /* top-left: 2x2 visible */
      TEST_ASSERT_EQUAL_INT(4, count_index(7));
      canvas_blit_rle(&s_cv, &r, W - 2, H - 2, flip);    /* bottom-right */
      TEST_ASSERT_EQUAL_INT(8, count_index(7));
      canvas_blit_rle(&s_cv, &r, W - 2, -3, flip);       /* top-right: 2x1 */
      TEST_ASSERT_EQUAL_INT(10, count_index(7));
      canvas_blit_rle(&s_cv, &r, -3, H - 1, flip);       /* bottom-left: 1x1 */
      TEST_ASSERT_EQUAL_INT(11, count_index(7));
      assert_borders_intact();
   }
   rle_free(&r);
}

void test_flip_clips_the_mirrored_side(void)
{
   const uint8_t row[4] = { 1, 2, 3, 4 };
   DS1_RLE r = make_sprite(row, 4, 1);

   canvas_blit_rle(&s_cv, &r, -1, 0, 1);       /* mirrored: 4 3 2 1, first column off */
   TEST_ASSERT_EQUAL_HEX8(3, px(0, 0));
   TEST_ASSERT_EQUAL_HEX8(2, px(1, 0));
   TEST_ASSERT_EQUAL_HEX8(1, px(2, 0));
   TEST_ASSERT_EQUAL_HEX8(BG, px(3, 0));
   assert_borders_intact();
   rle_free(&r);
}

void test_absurd_offsets_write_nothing(void)
{
   uint8_t solid[16];
   DS1_RLE r;

   memset(solid, 7, sizeof(solid));
   r = make_sprite(solid, 4, 4);
   canvas_blit_rle(&s_cv, &r, INT32_MIN, INT32_MAX, 0);
   canvas_blit_rle(&s_cv, &r, INT32_MAX, 0, 1);
   canvas_blit_rle(&s_cv, &r, 0, INT32_MIN, 0);
   canvas_blit_rle(&s_cv, &r, INT32_MAX - 1, INT32_MAX - 1, 1);
   TEST_ASSERT_EQUAL_INT(0, count_index(7));
   assert_borders_intact();
   rle_free(&r);
}

void test_cmap_blit_reads_source_row_destination_column(void)
{
   int s, d;
   const uint8_t one[1] = { 5 };
   DS1_RLE r = make_sprite(one, 1, 1);

   for (s = 0; s < 256; s++)
      for (d = 0; d < 256; d++)
         s_cmap[s * 256 + d] = (uint8_t) ((s + d) & 0xFF);
   canvas_blit_rle_cmap(&s_cv, &r, 4, 4, 0, s_cmap);
   TEST_ASSERT_EQUAL_HEX8(5 + BG, px(4, 4));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_TABLE, tg(4, 4));
   TEST_ASSERT_EQUAL_HEX8(BG, px(5, 4));
   rle_free(&r);
}

void test_lit_blit_maps_the_source_index(void)
{
   int i;
   const uint8_t one[1] = { 5 };
   DS1_RLE r = make_sprite(one, 1, 1);

   for (i = 0; i < 256; i++)
      s_table[i] = (uint8_t) (255 - i);
   canvas_blit_rle_lit(&s_cv, &r, 1, 1, 0, s_table);
   TEST_ASSERT_EQUAL_HEX8(250, px(1, 1));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_TABLE, tg(1, 1));
   rle_free(&r);
}

void test_shade_blit_maps_the_destination_under_opaque_pixels_only(void)
{
   int i;
   DS1_RLE r = make_sprite(k_small, 3, 2);

   canvas_fill(&s_cv, 40);
   for (i = 0; i < 256; i++)
      s_table[i] = (uint8_t) (i / 2);
   canvas_blit_rle_shade(&s_cv, &r, 0, 0, 0, s_table);
   TEST_ASSERT_EQUAL_HEX8(20, px(0, 0));
   TEST_ASSERT_EQUAL_HEX8(40, px(1, 0));
   TEST_ASSERT_EQUAL_HEX8(20, px(2, 0));
   TEST_ASSERT_EQUAL_HEX8(20, px(1, 1));
   TEST_ASSERT_EQUAL_HEX8(40, px(0, 1));
   rle_free(&r);
}

void test_clip_rect_is_respected(void)
{
   static uint8_t solid[W * H];
   DS1_RLE r;

   memset(solid, 7, sizeof(solid));
   r = make_sprite(solid, W, H);
   canvas_set_clip(&s_cv, 4, 4, 8, 8);
   canvas_blit_rle(&s_cv, &r, 0, 0, 0);
   TEST_ASSERT_EQUAL_INT(16, count_index(7));
   rle_free(&r);
}

void test_null_inputs_write_nothing(void)
{
   DS1_RLE r = make_sprite(k_small, 3, 2);

   canvas_blit_rle(NULL, &r, 0, 0, 0);
   canvas_blit_rle(&s_cv, NULL, 0, 0, 0);
   canvas_blit_rle_cmap(&s_cv, &r, 0, 0, 0, NULL);
   canvas_blit_rle_lit(&s_cv, &r, 0, 0, 0, NULL);
   canvas_blit_rle_shade(&s_cv, &r, 0, 0, 0, NULL);
   TEST_ASSERT_EQUAL_INT(W * H, count_index(BG));
   rle_free(&r);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_opaque_blit_places_pixels_and_skips_transparency);
   RUN_TEST(test_flip_mirrors_inside_the_same_box);
   RUN_TEST(test_clipping_on_all_four_edges_with_and_without_flip);
   RUN_TEST(test_flip_clips_the_mirrored_side);
   RUN_TEST(test_absurd_offsets_write_nothing);
   RUN_TEST(test_cmap_blit_reads_source_row_destination_column);
   RUN_TEST(test_lit_blit_maps_the_source_index);
   RUN_TEST(test_shade_blit_maps_the_destination_under_opaque_pixels_only);
   RUN_TEST(test_clip_rect_is_respected);
   RUN_TEST(test_null_inputs_write_nothing);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 3: RLE blits -------------------------------------------------------
ds1edit_sw_pure_test(test_render_blit test_render_blit.c
    ${_sw}/canvas.c ${_sw}/spritecache.c ${_core}/dt1_decode.c)
ds1edit_sw_add_test(render_blit test_render_blit)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_blit
```
Expected: build FAILS with `C4013: 'canvas_blit_rle' undefined` (MSVC) followed by `LNK2019: unresolved external symbol _canvas_blit_rle`.

- [ ] **Step 3: Write the implementation**

In `src/render/canvas.h`, insert after the `canvas_rectfill` declaration:

```c
/* RLE blits. (x, y) is where sprite pixel (0,0) lands; with `flip` the sprite
   is mirrored horizontally inside the same w-wide box. Every write is clipped.
   Tables are flat: cmap is 256x256 indexed [src * 256 + dst]; table is 256
   entries. A NULL table draws nothing. */
void canvas_blit_rle      (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip);
void canvas_blit_rle_cmap (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip,
                           const uint8_t *cmap);    /* dst = cmap[src*256 + dst] */
void canvas_blit_rle_lit  (DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip,
                           const uint8_t *table);   /* dst = table[src] (darkness on the sprite) */
void canvas_blit_rle_shade(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip,
                           const uint8_t *table);   /* dst = table[dst] under opaque pixels (shadows) */
```

Append to `src/render/canvas.c`:

```c
/* ---- RLE blits ------------------------------------------------------------ */

enum { BLIT_OPAQUE, BLIT_CMAP, BLIT_LIT, BLIT_SHADE };

static void blit_rle(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y,
                     int flip, int mode, const uint8_t *table)
{
   int32_t sy;
   uint8_t tag = (mode == BLIT_OPAQUE) ? CANVAS_TAG_EXACT : CANVAS_TAG_TABLE;

   if ((cv == NULL) || (cv->pixels == NULL) || (spr == NULL) ||
       (spr->row == NULL) || (spr->w <= 0) || (spr->h <= 0))
      return;
   if ((mode != BLIT_OPAQUE) && (table == NULL))
      return;

   for (sy = 0; sy < spr->h; sy++)
   {
      int64_t        dy = (int64_t) y + sy;
      const uint8_t *p, *end;
      int64_t        sx = 0;
      uint8_t       *drow, *trow;

      if (dy < cv->clip.y0)
         continue;
      if (dy >= cv->clip.y1)
         break;
      drow = cv->pixels + (size_t) dy * cv->stride;
      trow = (cv->tags != NULL) ? cv->tags + (size_t) dy * cv->stride : NULL;
      p    = spr->data + spr->row[sy];
      end  = spr->data + spr->row[sy + 1];

      while (p + 2 <= end)
      {
         int64_t n, d0, lo, hi, k;

         sx += p[0];
         n = p[1];
         p += 2;
         if (n > end - p)
            n = end - p;                    /* a truncated run draws what it has */

         /* Pixel k of this run lands at d0 + k, or d0 - k when flipped. Work
            out the k range that stays inside the clip in 64-bit, so no offset
            can wrap. */
         if (!flip)
         {
            d0 = (int64_t) x + sx;
            lo = cv->clip.x0 - d0;
            hi = cv->clip.x1 - d0;
         }
         else
         {
            d0 = (int64_t) x + (spr->w - 1 - sx);
            lo = d0 - (cv->clip.x1 - 1);
            hi = d0 - cv->clip.x0 + 1;
         }
         if (lo < 0)
            lo = 0;
         if (hi > n)
            hi = n;

         for (k = lo; k < hi; k++)
         {
            int64_t  dx = flip ? d0 - k : d0 + k;
            uint8_t  s  = p[k];
            uint8_t *d  = drow + dx;

            switch (mode)
            {
            case BLIT_OPAQUE: *d = s;                              break;
            case BLIT_CMAP:   *d = table[(size_t) s * 256u + *d];  break;
            case BLIT_LIT:    *d = table[s];                       break;
            default:          *d = table[*d];                      break;
            }
            if (trow != NULL)
               trow[dx] = tag;
         }
         p += n;
         sx += n;
      }
   }
}

void canvas_blit_rle(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y, int flip)
{
   blit_rle(cv, spr, x, y, flip, BLIT_OPAQUE, NULL);
}

void canvas_blit_rle_cmap(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y,
                          int flip, const uint8_t *cmap)
{
   blit_rle(cv, spr, x, y, flip, BLIT_CMAP, cmap);
}

void canvas_blit_rle_lit(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y,
                         int flip, const uint8_t *table)
{
   blit_rle(cv, spr, x, y, flip, BLIT_LIT, table);
}

void canvas_blit_rle_shade(DS1_CANVAS *cv, const DS1_RLE *spr, int32_t x, int32_t y,
                           int flip, const uint8_t *table)
{
   blit_rle(cv, spr, x, y, flip, BLIT_SHADE, table);
}
```

The `switch` sits inside the per-pixel loop to keep this task small and obviously correct; Task 14's measurement decides whether it needs splitting into one loop per mode (spec: "Multithreaded or SIMD compositing" is out of scope, plain loop specialization is not).

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" --build tmp/build-sw --config Release --target test_render_blit
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_(blit|canvas)$" --output-on-failure
```
Expected: `10 Tests 0 Failures 0 Ignored` for `render_blit`, and `100% tests passed, 0 tests failed out of 2`.

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/canvas.h src/render/canvas.c test/test_render_blit.c test/cmake/software_renderer.cmake
git commit -m "Blit RLE sprites into the canvas, flipped or through colour tables, always clipped"
```

---

### Task 4: The sprite cache, placeholders, and invalidation

**Files:**
- Modify: `src/render/spritecache.h`, `src/render/spritecache.c`
- Modify: `src/core/dt1.c` (`dt1_free`), `src/core/cof.c` (`anim_destroy_cof`)
- Create: `test/test_render_spritecache.c`, `test/test_render_spritecache_fixtures.c`
- Modify: `test/cmake/software_renderer.cmake`, `test/cmake/visuals.cmake`, `test/cmake/robustness.cmake`

**Interfaces:**
- Consumes: `rle_encode`, `rle_free`, `rle_decode`, `sc_zoom_frame`, `SC_ZOOM_*` (Task 2); `canvas_blit_rle` (Task 3); `dc6_decode_indices`, `dcc_decode_indices`, `ds1edit_index_frames_free` (`src/core/index_frame.h`); `decode_sub_tile_isometric` (`src/core/dt1_draw.h`); `visual_read_file` (`test/visual_test_support.h`).
- Produces (`src/render/spritecache.h`):
  ```c
  typedef struct SPRITECACHE_STATS { size_t entries; size_t bytes; unsigned long builds, hits, placeholders, failures_logged; } SPRITECACHE_STATS;
  const DS1_RLE *spritecache_get(const void *key, const DS1EDIT_INDEX_FRAME *src_1x1, int zoom);  /* NULL: nothing to draw */
  const DS1_RLE *spritecache_placeholder(int32_t w, int32_t h, uint8_t index);                     /* filled box */
  int  spritecache_log_failure_once(const void *key, const char *what);                             /* 1 when it logged */
  void spritecache_clear(void);
  void spritecache_stats(SPRITECACHE_STATS *out);
  ```
  `key` identifies the source buffer (the `CACHED_TILE *` that owns the indices). Returned pointers stay valid until `spritecache_clear()`. `spritecache_clear()` is called by `dt1_free()` and `anim_destroy_cof()` before they free any `CACHED_TILE`, so a freed address can never be reused as a stale key; a map close frees its DT1s and COFs through those two functions, which is how the cache is "freed when a map closes" (spec).

- [ ] **Step 1: Write the failing tests**

Create `test/test_render_spritecache.c`:

```c
/* The sprite cache (spritecache.c): build once per (key, zoom), hit after,
 * one copy per zoom level matching the reference decimation, placeholders,
 * once-per-asset failure logging, and clearing. */

#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "render/spritecache.h"

static uint8_t s_px[16 * 8];
static DS1EDIT_INDEX_FRAME s_frame;

void setUp(void)
{
   int x, y;
   spritecache_clear();
   for (y = 0; y < 8; y++)
      for (x = 0; x < 16; x++)
         s_px[y * 16 + x] = (uint8_t) ((x + y * 16) % 250 + 1);
   memset(&s_frame, 0, sizeof(s_frame));
   s_frame.width = 16;
   s_frame.height = 8;
   s_frame.stride = 16;
   s_frame.pixels = s_px;
   s_frame.pixels_len = sizeof(s_px);
}

void tearDown(void) { spritecache_clear(); }

void test_get_builds_once_then_hits(void)
{
   SPRITECACHE_STATS st;
   const DS1_RLE *a = spritecache_get(s_px, &s_frame, SC_ZOOM_11);
   const DS1_RLE *b = spritecache_get(s_px, &s_frame, SC_ZOOM_11);

   TEST_ASSERT_NOT_NULL(a);
   TEST_ASSERT_EQUAL_PTR(a, b);
   spritecache_stats(&st);
   TEST_ASSERT_EQUAL_UINT32(1, st.builds);
   TEST_ASSERT_EQUAL_UINT32(1, st.hits);
   TEST_ASSERT_EQUAL_size_t(1, st.entries);
}

void test_each_zoom_is_its_own_copy(void)
{
   const DS1_RLE *a = spritecache_get(s_px, &s_frame, SC_ZOOM_11);
   const DS1_RLE *b = spritecache_get(s_px, &s_frame, SC_ZOOM_12);
   const DS1_RLE *c = spritecache_get(s_px, &s_frame, SC_ZOOM_21);

   TEST_ASSERT_TRUE(a != b && b != c && a != c);
   TEST_ASSERT_EQUAL_INT32(16, a->w);
   TEST_ASSERT_EQUAL_INT32(8, b->w);
   TEST_ASSERT_EQUAL_INT32(4, b->h);
   TEST_ASSERT_EQUAL_INT32(32, c->w);
}

void test_zoom_copy_matches_the_reference_decimation(void)
{
   uint8_t back[4 * 2];
   const DS1_RLE *r = spritecache_get(s_px, &s_frame, SC_ZOOM_14);
   int x, y;

   TEST_ASSERT_NOT_NULL(r);
   TEST_ASSERT_EQUAL_INT(1, rle_decode(r, back, 4));
   for (y = 0; y < 2; y++)
      for (x = 0; x < 4; x++)
         TEST_ASSERT_EQUAL_HEX8(s_px[(y * 8 / 2) * 16 + (x * 16 / 4)], back[y * 4 + x]);
}

void test_rebuilt_after_clear(void)
{
   SPRITECACHE_STATS st;
   spritecache_get(s_px, &s_frame, SC_ZOOM_11);
   spritecache_clear();
   spritecache_stats(&st);
   TEST_ASSERT_EQUAL_size_t(0, st.entries);
   TEST_ASSERT_NOT_NULL(spritecache_get(s_px, &s_frame, SC_ZOOM_11));
   spritecache_stats(&st);
   TEST_ASSERT_EQUAL_UINT32(1, st.builds);
}

void test_many_keys_survive_table_growth(void)
{
   static uint8_t keys[3000];
   const DS1_RLE *first = spritecache_get(&keys[0], &s_frame, SC_ZOOM_11);
   int i;

   for (i = 1; i < 3000; i++)
      TEST_ASSERT_NOT_NULL(spritecache_get(&keys[i], &s_frame, SC_ZOOM_11));
   TEST_ASSERT_EQUAL_PTR(first, spritecache_get(&keys[0], &s_frame, SC_ZOOM_11));
}

void test_invalid_inputs_return_null(void)
{
   TEST_ASSERT_NULL(spritecache_get(NULL, &s_frame, SC_ZOOM_11));
   TEST_ASSERT_NULL(spritecache_get(s_px, NULL, SC_ZOOM_11));
   TEST_ASSERT_NULL(spritecache_get(s_px, &s_frame, SC_ZOOM_MAX));
   s_frame.width = 8;
   s_frame.height = 8;
   TEST_ASSERT_NULL(spritecache_get(s_px, &s_frame, SC_ZOOM_116));  /* decimates to 0x0 */
}

void test_placeholder_is_a_filled_box_and_reused(void)
{
   uint8_t back[3 * 2];
   const DS1_RLE *p = spritecache_placeholder(3, 2, 200);
   int i;

   TEST_ASSERT_NOT_NULL(p);
   TEST_ASSERT_EQUAL_PTR(p, spritecache_placeholder(3, 2, 200));
   TEST_ASSERT_EQUAL_INT(1, rle_decode(p, back, 3));
   for (i = 0; i < 6; i++)
      TEST_ASSERT_EQUAL_HEX8(200, back[i]);
   TEST_ASSERT_NULL(spritecache_placeholder(0, 2, 200));
   TEST_ASSERT_NULL(spritecache_placeholder(3, 2, 0));    /* index 0 would be invisible */
}

void test_failures_are_logged_once_per_key(void)
{
   int a, b;
   SPRITECACHE_STATS st;

   TEST_ASSERT_EQUAL_INT(1, spritecache_log_failure_once(&a, "test sprite a"));
   TEST_ASSERT_EQUAL_INT(0, spritecache_log_failure_once(&a, "test sprite a"));
   TEST_ASSERT_EQUAL_INT(1, spritecache_log_failure_once(&b, "test sprite b"));
   spritecache_stats(&st);
   TEST_ASSERT_EQUAL_UINT32(2, st.failures_logged);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_get_builds_once_then_hits);
   RUN_TEST(test_each_zoom_is_its_own_copy);
   RUN_TEST(test_zoom_copy_matches_the_reference_decimation);
   RUN_TEST(test_rebuilt_after_clear);
   RUN_TEST(test_many_keys_survive_table_growth);
   RUN_TEST(test_invalid_inputs_return_null);
   RUN_TEST(test_placeholder_is_a_filled_box_and_reused);
   RUN_TEST(test_failures_are_logged_once_per_key);
   return UNITY_END();
}
```

Create `test/test_render_spritecache_fixtures.c` (spec: "index frame -> RLE -> blit reproduces the index frame for authored DC6/DCC/DT1 fixtures"):

```c
/* Index frame -> RLE -> blit reproduces the index frame, for the authored
 * decoder fixtures in test/equivalence/fixtures/decoders (DC6, DCC; their
 * expected frames come from scripts/equivalence/decoder_cases.py) and for an
 * authored isometric DT1 sub-tile decoded by the production decoder. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "core/index_frame.h"
#include "core/dt1_draw.h"
#include "render/canvas.h"
#include "render/spritecache.h"
#include "visual_test_support.h"

void setUp(void) { spritecache_clear(); }
void tearDown(void) { spritecache_clear(); }

/* Blit the frame's cached RLE onto a zeroed canvas the frame's size and
   compare every index. */
static void assert_round_trip(const DS1EDIT_INDEX_FRAME *f, const char *what)
{
   DS1_CANVAS cv;
   const DS1_RLE *r;
   int32_t x, y;
   char msg[160];

   if ((f->width == 0) || (f->height == 0))
      return;                                   /* empty frames draw nothing */
   r = spritecache_get(f->pixels, f, SC_ZOOM_11);
   snprintf(msg, sizeof(msg), "%s: no RLE", what);
   TEST_ASSERT_NOT_NULL_MESSAGE(r, msg);
   TEST_ASSERT_EQUAL_INT(1, canvas_create(&cv, f->width, f->height, 0));
   canvas_blit_rle(&cv, r, 0, 0, 0);
   for (y = 0; y < f->height; y++)
      for (x = 0; x < f->width; x++)
      {
         snprintf(msg, sizeof(msg), "%s pixel (%d,%d)", what, (int) x, (int) y);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(f->pixels[(size_t) y * f->stride + x],
                                        cv.pixels[(size_t) y * cv.stride + x], msg);
      }
   canvas_destroy(&cv);
}

static void round_trip_file(const char *name, int is_dcc)
{
   char path[512], what[200], err[200];
   unsigned char *bytes;
   size_t len, count = 0, i;
   DS1EDIT_INDEX_FRAME *frames = NULL;
   int ok;

   snprintf(path, sizeof(path), "%s/%s", DS1EDIT_DECODER_FIXTURES, name);
   bytes = visual_read_file(path, &len);
   TEST_ASSERT_NOT_NULL_MESSAGE(bytes, path);
   ok = is_dcc ? dcc_decode_indices(bytes, len, DS1EDIT_ALL_DIRECTIONS, &frames, &count, err, sizeof(err))
               : dc6_decode_indices(bytes, len, DS1EDIT_ALL_DIRECTIONS, &frames, &count, err, sizeof(err));
   TEST_ASSERT_EQUAL_INT_MESSAGE(1, ok, err);
   TEST_ASSERT_TRUE(count > 0);
   for (i = 0; i < count; i++)
   {
      snprintf(what, sizeof(what), "%s frame %u", name, (unsigned) i);
      assert_round_trip(&frames[i], what);
   }
   ds1edit_index_frames_free(frames, count);
   free(bytes);
}

void test_dc6_fixtures_round_trip(void)
{
   round_trip_file("dc6_rle_basic.dc6", 0);
   round_trip_file("dc6_multi_direction.dc6", 0);
   round_trip_file("dc6_clip.dc6", 0);
}

void test_dcc_fixtures_round_trip(void)
{
   round_trip_file("dcc_single.dcc", 1);
   round_trip_file("dcc_sequence.dcc", 1);
   round_trip_file("dcc_flag0.dcc", 1);
}

void test_dt1_isometric_subtile_round_trips(void)
{
   /* An isometric sub-tile is 256 bytes laid out on a 32x15 diamond. */
   uint8_t data[256], buf[32 * 15];
   DS1EDIT_INDEX_FRAME f;
   int i;

   for (i = 0; i < 256; i++)
      data[i] = (uint8_t) (i % 254 + 1);
   memset(buf, 0, sizeof(buf));
   decode_sub_tile_isometric(buf, 32, 15, 0, 0, data, 256);
   memset(&f, 0, sizeof(f));
   f.width = 32;
   f.height = 15;
   f.stride = 32;
   f.pixels = buf;
   f.pixels_len = sizeof(buf);
   assert_round_trip(&f, "dt1 isometric sub-tile");
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_dc6_fixtures_round_trip);
   RUN_TEST(test_dcc_fixtures_round_trip);
   RUN_TEST(test_dt1_isometric_subtile_round_trips);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 4: sprite cache ----------------------------------------------------
ds1edit_sw_pure_test(test_render_spritecache test_render_spritecache.c
    ${_sw}/spritecache.c ${_core}/dt1_decode.c)
ds1edit_sw_add_test(render_spritecache test_render_spritecache)

# DCC decoding needs dcc.c, which reaches the editor globals and Allegro, so
# this one goes through the visual-test harness. index_frame.c supplies
# dc6_decode_indices and is linked with dcc.c as every decoder test does.
ds1edit_visual_test(test_render_spritecache_fixtures ALLEGRO SOURCES
    test_render_spritecache_fixtures.c ${_sw}/spritecache.c ${_sw}/canvas.c
    ${_core}/dcc.c ${_core}/index_frame.c ${_core}/dt1_decode.c
    ${_vsup} ${_vglobals})
ds1edit_sw_add_test(render_spritecache_fixtures test_render_spritecache_fixtures)
```

- [ ] **Step 2: Run them to make sure they fail**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_spritecache
```
Expected: build FAILS with `'SPRITECACHE_STATS': undeclared identifier` (and unresolved `_spritecache_get`).

- [ ] **Step 3: Write the implementation**

In `src/render/spritecache.h`, insert before the closing `#ifdef __cplusplus` block:

```c
typedef struct SPRITECACHE_STATS
{
   size_t        entries;          /* cached (key, zoom) copies          */
   size_t        bytes;            /* RLE bytes held, approximately      */
   unsigned long builds;
   unsigned long hits;
   unsigned long placeholders;     /* distinct placeholder boxes built   */
   unsigned long failures_logged;
} SPRITECACHE_STATS;

/* The RLE copy of `src_1x1` at `zoom`, built on first use. `key` names the
   source buffer (the CACHED_TILE that owns the indices); the result stays
   valid until spritecache_clear(). NULL when there is nothing to draw: bad
   input, a copy that decimates to nothing, or allocation failure. */
const DS1_RLE *spritecache_get(const void *key, const DS1EDIT_INDEX_FRAME *src_1x1, int zoom);

/* A w x h box filled with `index` (the magenta placeholder for a sprite that
   failed to decode). Built once per (w, h, index). NULL for index 0. */
const DS1_RLE *spritecache_placeholder(int32_t w, int32_t h, uint8_t index);

/* Log "render: <what> failed to decode; drawing a placeholder box" to stderr
   the first time `key` is reported. Returns 1 when it logged. */
int  spritecache_log_failure_once(const void *key, const char *what);

/* Free every cached copy, placeholder and failure record. Called before the
   source buffers are freed (dt1_free, anim_destroy_cof). */
void spritecache_clear(void);

void spritecache_stats(SPRITECACHE_STATS *out);
```

Append to `src/render/spritecache.c`:

```c
/* ---- the cache ------------------------------------------------------------- */

typedef struct SC_ENTRY
{
   const void *key;            /* NULL = empty slot */
   int32_t     zoom;
   DS1_RLE    *rle;            /* separately allocated: stable address */
} SC_ENTRY;

#define SC_PLACEHOLDER_MAX 16
#define SC_FAIL_MAX        4096

static SC_ENTRY          *s_tab;
static size_t             s_cap, s_count;
static SPRITECACHE_STATS  s_stats;

static struct
{
   int      used;
   int32_t  w, h;
   uint8_t  index;
   DS1_RLE  rle;
} s_ph[SC_PLACEHOLDER_MAX];

static const void *s_failed[SC_FAIL_MAX];
static size_t      s_failed_count;

static size_t sc_slot(const void *key, int32_t zoom, size_t cap)
{
   uint64_t h = (uint64_t) (uintptr_t) key;

   h ^= (uint64_t) (uint32_t) zoom * UINT64_C(0x9E3779B97F4A7C15);
   h ^= h >> 33;
   h *= UINT64_C(0xFF51AFD7ED558CCD);
   h ^= h >> 33;
   return (size_t) h & (cap - 1);
}

static int sc_grow(void)
{
   size_t    new_cap = s_cap ? s_cap * 2 : 1024, i, j;
   SC_ENTRY *nt = (SC_ENTRY *) calloc(new_cap, sizeof(*nt));

   if (nt == NULL)
      return 0;
   for (i = 0; i < s_cap; i++)
   {
      if (s_tab[i].key == NULL)
         continue;
      j = sc_slot(s_tab[i].key, s_tab[i].zoom, new_cap);
      while (nt[j].key != NULL)
         j = (j + 1) & (new_cap - 1);
      nt[j] = s_tab[i];
   }
   free(s_tab);
   s_tab = nt;
   s_cap = new_cap;
   return 1;
}

static SC_ENTRY *sc_find(const void *key, int32_t zoom)
{
   size_t j;

   if (s_cap == 0)
      return NULL;
   j = sc_slot(key, zoom, s_cap);
   while (s_tab[j].key != NULL)
   {
      if ((s_tab[j].key == key) && (s_tab[j].zoom == zoom))
         return &s_tab[j];
      j = (j + 1) & (s_cap - 1);
   }
   return NULL;
}

static size_t rle_bytes(const DS1_RLE *r)
{
   return r->data_len + ((size_t) r->h + 1) * sizeof(uint32_t) + sizeof(*r);
}

const DS1_RLE *spritecache_get(const void *key, const DS1EDIT_INDEX_FRAME *src_1x1, int zoom)
{
   SC_ENTRY            *e;
   DS1_RLE             *r;
   DS1EDIT_INDEX_FRAME  zf;
   int                  ok, mul, div;
   size_t               j;

   if ((key == NULL) || (src_1x1 == NULL) || !sc_zoom_ratio(zoom, &mul, &div))
      return NULL;
   e = sc_find(key, zoom);
   if (e != NULL)
   {
      s_stats.hits++;
      return e->rle;
   }

   r = (DS1_RLE *) calloc(1, sizeof(*r));
   if (r == NULL)
      return NULL;
   if (zoom == SC_ZOOM_11)
      ok = rle_encode(src_1x1, r);
   else
   {
      ok = sc_zoom_frame(src_1x1, zoom, &zf);
      if (ok)
      {
         ok = rle_encode(&zf, r);
         free(zf.pixels);
      }
   }
   if (!ok)
   {
      free(r);
      return NULL;
   }
   if (((s_count + 1) * 10 > s_cap * 7) && !sc_grow())
   {
      rle_free(r);
      free(r);
      return NULL;
   }
   j = sc_slot(key, zoom, s_cap);
   while (s_tab[j].key != NULL)
      j = (j + 1) & (s_cap - 1);
   s_tab[j].key  = key;
   s_tab[j].zoom = zoom;
   s_tab[j].rle  = r;
   s_count++;
   s_stats.entries = s_count;
   s_stats.bytes  += rle_bytes(r);
   s_stats.builds++;
   return r;
}

const DS1_RLE *spritecache_placeholder(int32_t w, int32_t h, uint8_t index)
{
   DS1EDIT_INDEX_FRAME f;
   uint8_t *px;
   int i;

   if ((w <= 0) || (h <= 0) || (w > 4096) || (h > 4096) || (index == 0))
      return NULL;
   for (i = 0; i < SC_PLACEHOLDER_MAX; i++)
      if (s_ph[i].used && (s_ph[i].w == w) && (s_ph[i].h == h) && (s_ph[i].index == index))
         return &s_ph[i].rle;
   for (i = 0; i < SC_PLACEHOLDER_MAX; i++)
      if (!s_ph[i].used)
         break;
   if (i == SC_PLACEHOLDER_MAX)
      return NULL;

   px = (uint8_t *) malloc((size_t) w * (size_t) h);
   if (px == NULL)
      return NULL;
   memset(px, index, (size_t) w * (size_t) h);
   memset(&f, 0, sizeof(f));
   f.width      = w;
   f.height     = h;
   f.stride     = w;
   f.pixels     = px;
   f.pixels_len = (size_t) w * (size_t) h;
   if (!rle_encode(&f, &s_ph[i].rle))
   {
      free(px);
      return NULL;
   }
   free(px);
   s_ph[i].used  = 1;
   s_ph[i].w     = w;
   s_ph[i].h     = h;
   s_ph[i].index = index;
   s_stats.placeholders++;
   return &s_ph[i].rle;
}

int spritecache_log_failure_once(const void *key, const char *what)
{
   size_t i;

   if ((key == NULL) || (s_failed_count >= SC_FAIL_MAX))
      return 0;
   for (i = 0; i < s_failed_count; i++)
      if (s_failed[i] == key)
         return 0;
   s_failed[s_failed_count++] = key;
   fprintf(stderr, "render: %s failed to decode; drawing a placeholder box\n",
           (what != NULL) ? what : "a sprite");
   fflush(stderr);
   s_stats.failures_logged++;
   return 1;
}

void spritecache_clear(void)
{
   size_t i;
   int    p;

   for (i = 0; i < s_cap; i++)
   {
      if (s_tab[i].rle != NULL)
      {
         rle_free(s_tab[i].rle);
         free(s_tab[i].rle);
      }
   }
   free(s_tab);
   s_tab   = NULL;
   s_cap   = 0;
   s_count = 0;
   for (p = 0; p < SC_PLACEHOLDER_MAX; p++)
      if (s_ph[p].used)
         rle_free(&s_ph[p].rle);
   memset(s_ph, 0, sizeof(s_ph));
   s_failed_count = 0;
   memset(&s_stats, 0, sizeof(s_stats));
}

void spritecache_stats(SPRITECACHE_STATS *out)
{
   if (out != NULL)
      *out = s_stats;
}
```

In `src/core/dt1.c`, add `#include "render/spritecache.h"` after the existing includes, and in `dt1_free()` insert the call right after the early return (anchor: the three lines below):

```c
   if (strlen(glb_dt1[i].name) == 0)
      return 0;

   /* The software renderer's RLE copies are keyed by these tiles' addresses;
      drop them before the tiles go, so a freed address is never a live key. */
   spritecache_clear();
```

In `src/core/cof.c`, add `#include "render/spritecache.h"` after `#include "core/rgba_cache.h"`, and in `anim_destroy_cof()` insert after its early return:

```c
   if (cof == NULL)
      return 0;

   /* See dt1_free(): the layer frames are sprite-cache keys. */
   spritecache_clear();
```

Link `spritecache.c` wherever `dt1.c` is linked into a test. In `test/cmake/visuals.cmake` change

```cmake
set(_dt1_sources ${_core}/dt1.c ${_core}/dt1_decode.c ${_core}/dt1_draw.c
    ${_core}/rgba_cache.c)
```

to

```cmake
set(_dt1_sources ${_core}/dt1.c ${_core}/dt1_decode.c ${_core}/dt1_draw.c
    ${_core}/rgba_cache.c ${CMAKE_SOURCE_DIR}/src/render/spritecache.c)
```

and in `test/cmake/robustness.cmake` change the `test_adversarial_codecs` source list line

```cmake
        ${_core}/dt1_draw.c ${_core}/rgba_cache.c
```

to

```cmake
        ${_core}/dt1_draw.c ${_core}/rgba_cache.c ${CMAKE_SOURCE_DIR}/src/render/spritecache.c
```

- [ ] **Step 4: Run the tests to verify they pass, and that nothing that links `dt1.c` broke**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_spritecache test_render_spritecache_fixtures test_dt1_load test_visual_reference_probe test_adversarial_codecs ds1edit
"$CTEST" --test-dir tmp/build-sw -C Release -R "^(render_spritecache|render_spritecache_fixtures|dt1_load|adversarial_codecs|visual_reference_probe_decoder_cases)$" --output-on-failure
```
Expected: `8 Tests 0 Failures` for `render_spritecache`, `3 Tests 0 Failures` for the fixtures, and `100% tests passed, 0 tests failed out of 5`. The `ds1edit` target links (it writes `bin/ds1edit.exe`).

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/spritecache.h src/render/spritecache.c src/core/dt1.c src/core/cof.c test/test_render_spritecache.c test/test_render_spritecache_fixtures.c test/cmake/software_renderer.cmake test/cmake/visuals.cmake test/cmake/robustness.cmake
git commit -m "Cache one RLE copy per sprite per zoom, dropped before its source is freed"
```

---

### Task 5: D2CMP blend and darkness tables

**Files:**
- Create: `src/render/sw_tables.h`, `src/render/sw_tables.c`, `test/test_render_sw_tables.c`
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: `RGBA_PALETTE`, `palette_d2_to_rgba`, `palette_find_closest` (`src/core/palette.h`).
- Produces (`src/render/sw_tables.h`):
  ```c
  #define SW_PL2_FULL_SIZE   0x6C327L
  #define SW_PL2_OFF_DARK    0x00400L
  #define SW_PL2_OFF_ALPHA75 0x03500L   /* COF_25TRANS * 256 */
  #define SW_PL2_OFF_ALPHA50 0x13500L   /* COF_50TRANS * 256 */
  #define SW_PL2_OFF_ALPHA25 0x23500L   /* COF_75TRANS * 256 */
  #define SW_PL2_OFF_ADD     0x33500L   /* COF_ALPHA * 256 */
  #define SW_PL2_OFF_MUL     0x43500L   /* COF_LUMINANCE * 256 */
  #define SW_PL2_OFF_SCREEN  0x5B500L   /* COF_ALPHABRIGHT * 256 */
  typedef struct SW_TABLES {
     uint8_t alpha25[256][256], alpha50[256][256], alpha75[256][256];   /* trans_b 0, 1, 2: [src][dst] */
     uint8_t add[256][256], mul[256][256], screen[256][256];            /* trans_b 3, 4, 6 */
     uint8_t dark[33][256];                                             /* darkness level 0..32; 32 = full light */
     int     from_pl2;
  } SW_TABLES;
  int  sw_tables_build(SW_TABLES *t, const uint8_t *pl2, long pl2_size);   /* 1 ok */
  void sw_tables_generate(SW_TABLES *t, const RGBA_PALETTE *raw);          /* formulas only */
  const uint8_t *sw_tables_blend(const SW_TABLES *t, int trans_b);         /* flat [src*256+dst]; NULL for 5 / out of range */
  ```
  Mapping (memory note "D2 blend modes", `docs/guides/palettes-and-colour.md` and its verification report): `trans_b` 0 = source 63/255 (`0x23500`), 1 = 127/255 (`0x13500`), 2 = 191/255 (`0x3500`), 3 = additive `min(s+d,255)` (`0x33500`), 4 = multiply `s*d/255` (`0x43500`), 5 = unused, 6 = screen `s+d-s*d/255` (`0x5B500`). Alpha formula per channel: `(d*(255-a) + s*a)/255`; darkness level L (1..32): `c*L >> 5`; every result snapped with the nearest-colour search (`palette_find_closest`: Euclidean, index 0 skipped, first minimum wins). File layout: byte at `off + 256*src + dst`. The screen table is "Located" only (not byte-verified) in the guide, so a full PL2 is always preferred over the formula.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_sw_tables.c`:

```c
/* D2CMP colour tables (sw_tables.c).
 *
 * Formula checks use an independent nearest-colour search written here, not
 * palette_find_closest, so the test is not the implementation restated. The
 * PL2 copy checks use a synthetic full-size PL2 with a recognisable byte
 * pattern. The game-data check (vanilla 1.13c Pal.PL2 bytes vs the formulas)
 * runs only when DS1EDIT_TEST_PL2_DIR names a directory holding pal0.bin ..
 * pal4.bin written by the editor from the vanilla install; otherwise it is
 * reported IGNORED, never passed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "core/palette.h"
#include "render/sw_tables.h"

static SW_TABLES s_t;
static uint8_t   s_pl2_small[1024];
static RGBA_PALETTE s_raw;

static void build_palette_bytes(uint8_t *pl2)
{
   int i;
   for (i = 0; i < 256; i++)
   {
      pl2[4 * i + 0] = (uint8_t) i;
      pl2[4 * i + 1] = (uint8_t) ((i * 73) & 0xFF);
      pl2[4 * i + 2] = (uint8_t) ((i * 151) & 0xFF);
      pl2[4 * i + 3] = 0;
   }
   pl2[0] = pl2[1] = pl2[2] = 0;
}

void setUp(void)
{
   build_palette_bytes(s_pl2_small);
   palette_d2_to_rgba(s_pl2_small, sizeof(s_pl2_small), NULL, &s_raw);
}

void tearDown(void) {}

static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static uint8_t ref_nearest(int r, int g, int b)
{
   int i, best = 1;
   long best_d = -1;
   r = clamp255(r); g = clamp255(g); b = clamp255(b);
   for (i = 1; i < 256; i++)
   {
      long dr = s_raw.colors[i].r - r, dg = s_raw.colors[i].g - g, db = s_raw.colors[i].b - b;
      long d = dr * dr + dg * dg + db * db;
      if ((best_d < 0) || (d < best_d))
      {
         best_d = d;
         best = i;
      }
   }
   return (uint8_t) best;
}

static const int k_probe[6] = { 1, 2, 17, 100, 200, 255 };

static uint8_t ref_alpha(int a, int s, int d)
{
   return ref_nearest((s_raw.colors[d].r * (255 - a) + s_raw.colors[s].r * a) / 255,
                      (s_raw.colors[d].g * (255 - a) + s_raw.colors[s].g * a) / 255,
                      (s_raw.colors[d].b * (255 - a) + s_raw.colors[s].b * a) / 255);
}

void test_generated_alpha_tables_follow_the_d2cmp_formula(void)
{
   int i, j;
   sw_tables_generate(&s_t, &s_raw);
   TEST_ASSERT_EQUAL_INT(0, s_t.from_pl2);
   for (i = 0; i < 6; i++)
      for (j = 0; j < 6; j++)
      {
         int s = k_probe[i], d = k_probe[j];
         TEST_ASSERT_EQUAL_HEX8(ref_alpha(63, s, d), s_t.alpha25[s][d]);
         TEST_ASSERT_EQUAL_HEX8(ref_alpha(127, s, d), s_t.alpha50[s][d]);
         TEST_ASSERT_EQUAL_HEX8(ref_alpha(191, s, d), s_t.alpha75[s][d]);
      }
}

void test_generated_add_multiply_screen_follow_the_formulas(void)
{
   int i, j;
   sw_tables_generate(&s_t, &s_raw);
   for (i = 0; i < 6; i++)
      for (j = 0; j < 6; j++)
      {
         int s = k_probe[i], d = k_probe[j];
         const RGBA_COLOR *cs = &s_raw.colors[s], *cd = &s_raw.colors[d];
         TEST_ASSERT_EQUAL_HEX8(ref_nearest(cs->r + cd->r, cs->g + cd->g, cs->b + cd->b),
                                s_t.add[s][d]);
         TEST_ASSERT_EQUAL_HEX8(ref_nearest(cs->r * cd->r / 255, cs->g * cd->g / 255,
                                            cs->b * cd->b / 255), s_t.mul[s][d]);
         TEST_ASSERT_EQUAL_HEX8(ref_nearest(cs->r + cd->r - cs->r * cd->r / 255,
                                            cs->g + cd->g - cs->g * cd->g / 255,
                                            cs->b + cd->b - cs->b * cd->b / 255),
                                s_t.screen[s][d]);
      }
}

void test_generated_darkness_follows_the_formula_at_every_level(void)
{
   int L, i;
   sw_tables_generate(&s_t, &s_raw);
   for (L = 1; L <= 32; L++)
      for (i = 0; i < 256; i++)
         TEST_ASSERT_EQUAL_HEX8(ref_nearest((s_raw.colors[i].r * L) >> 5,
                                            (s_raw.colors[i].g * L) >> 5,
                                            (s_raw.colors[i].b * L) >> 5),
                                s_t.dark[L][i]);
   for (i = 0; i < 256; i++)
      TEST_ASSERT_EQUAL_HEX8(ref_nearest(0, 0, 0), s_t.dark[0][i]);
}

void test_full_pl2_tables_are_copied_byte_for_byte(void)
{
   uint8_t *pl2 = (uint8_t *) malloc(SW_PL2_FULL_SIZE);
   long off;
   int L;

   TEST_ASSERT_NOT_NULL(pl2);
   for (off = 0; off < SW_PL2_FULL_SIZE; off++)
      pl2[off] = (uint8_t) ((off * 7 + 3) & 0xFF);
   build_palette_bytes(pl2);

   TEST_ASSERT_EQUAL_INT(1, sw_tables_build(&s_t, pl2, SW_PL2_FULL_SIZE));
   TEST_ASSERT_EQUAL_INT(1, s_t.from_pl2);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_ALPHA25 + 5 * 256 + 9], s_t.alpha25[5][9]);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_ALPHA50 + 200 * 256 + 1], s_t.alpha50[200][1]);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_ALPHA75 + 255 * 256 + 255], s_t.alpha75[255][255]);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_ADD + 3 * 256 + 4], s_t.add[3][4]);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_MUL + 4 * 256 + 3], s_t.mul[4][3]);
   TEST_ASSERT_EQUAL_HEX8(pl2[SW_PL2_OFF_SCREEN + 17 * 256 + 100], s_t.screen[17][100]);
   for (L = 1; L <= 32; L++)
      TEST_ASSERT_EQUAL_HEX8_ARRAY(pl2 + SW_PL2_OFF_DARK + (L - 1) * 256, s_t.dark[L], 256);
   free(pl2);
}

void test_short_pl2_falls_back_to_the_formulas(void)
{
   TEST_ASSERT_EQUAL_INT(1, sw_tables_build(&s_t, s_pl2_small, sizeof(s_pl2_small)));
   TEST_ASSERT_EQUAL_INT(0, s_t.from_pl2);
   TEST_ASSERT_EQUAL_HEX8(ref_alpha(127, 17, 200), s_t.alpha50[17][200]);
}

void test_blend_selector_maps_every_trans_b(void)
{
   TEST_ASSERT_EQUAL_PTR(&s_t.alpha25[0][0], sw_tables_blend(&s_t, 0));
   TEST_ASSERT_EQUAL_PTR(&s_t.alpha50[0][0], sw_tables_blend(&s_t, 1));
   TEST_ASSERT_EQUAL_PTR(&s_t.alpha75[0][0], sw_tables_blend(&s_t, 2));
   TEST_ASSERT_EQUAL_PTR(&s_t.add[0][0], sw_tables_blend(&s_t, 3));
   TEST_ASSERT_EQUAL_PTR(&s_t.mul[0][0], sw_tables_blend(&s_t, 4));
   TEST_ASSERT_NULL(sw_tables_blend(&s_t, 5));
   TEST_ASSERT_EQUAL_PTR(&s_t.screen[0][0], sw_tables_blend(&s_t, 6));
   TEST_ASSERT_NULL(sw_tables_blend(&s_t, 7));
   TEST_ASSERT_NULL(sw_tables_blend(&s_t, -1));
   TEST_ASSERT_NULL(sw_tables_blend(NULL, 0));
}

/* ---- game-data lane (local only) ----------------------------------------- */

static size_t count_diff(const uint8_t *a, const uint8_t *b, size_t n)
{
   size_t i, d = 0;
   for (i = 0; i < n; i++)
      if (a[i] != b[i])
         d++;
   return d;
}

void test_vanilla_pl2_tables_match_the_formulas(void)
{
   const char *dir = getenv("DS1EDIT_TEST_PL2_DIR");
   int act;

   if ((dir == NULL) || (dir[0] == 0))
      TEST_IGNORE_MESSAGE("DS1EDIT_TEST_PL2_DIR not set: vanilla 1.13c Pal.PL2 check not run");

   for (act = 0; act < 5; act++)
   {
      char path[600], msg[200];
      FILE *f;
      uint8_t *pl2 = (uint8_t *) malloc(SW_PL2_FULL_SIZE + 1);
      size_t n, d;
      RGBA_PALETTE raw;
      int L;

      snprintf(path, sizeof(path), "%s/pal%d.bin", dir, act);
      f = fopen(path, "rb");
      TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
      n = fread(pl2, 1, SW_PL2_FULL_SIZE + 1, f);
      fclose(f);
      snprintf(msg, sizeof(msg), "%s is %u bytes, not a full Pal.PL2", path, (unsigned) n);
      TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t) SW_PL2_FULL_SIZE, n, msg);

      palette_d2_to_rgba(pl2, (long) n, NULL, &raw);
      sw_tables_generate(&s_t, &raw);

      d = 0;
      for (L = 1; L <= 32; L++)
         d += count_diff(s_t.dark[L], pl2 + SW_PL2_OFF_DARK + (L - 1) * 256, 256);
      snprintf(msg, sizeof(msg), "act %d darkness mismatches", act + 1);
      TEST_ASSERT_EQUAL_size_t_MESSAGE(0, d, msg);

      d = count_diff(&s_t.alpha25[0][0], pl2 + SW_PL2_OFF_ALPHA25, 65536)
        + count_diff(&s_t.alpha50[0][0], pl2 + SW_PL2_OFF_ALPHA50, 65536)
        + count_diff(&s_t.alpha75[0][0], pl2 + SW_PL2_OFF_ALPHA75, 65536)
        + count_diff(&s_t.add[0][0], pl2 + SW_PL2_OFF_ADD, 65536)
        + count_diff(&s_t.mul[0][0], pl2 + SW_PL2_OFF_MUL, 65536);
      snprintf(msg, sizeof(msg), "act %d alpha/add/multiply mismatches", act + 1);
      TEST_ASSERT_EQUAL_size_t_MESSAGE(0, d, msg);

      /* Screen is "Located" only in the guide: report, do not assert. */
      d = count_diff(&s_t.screen[0][0], pl2 + SW_PL2_OFF_SCREEN, 65536);
      snprintf(msg, sizeof(msg), "act %d screen-blend formula differs from the file in %u of 65536 entries (the renderer uses the file's bytes)",
               act + 1, (unsigned) d);
      TEST_MESSAGE(msg);
      free(pl2);
   }
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_generated_alpha_tables_follow_the_d2cmp_formula);
   RUN_TEST(test_generated_add_multiply_screen_follow_the_formulas);
   RUN_TEST(test_generated_darkness_follows_the_formula_at_every_level);
   RUN_TEST(test_full_pl2_tables_are_copied_byte_for_byte);
   RUN_TEST(test_short_pl2_falls_back_to_the_formulas);
   RUN_TEST(test_blend_selector_maps_every_trans_b);
   RUN_TEST(test_vanilla_pl2_tables_match_the_formulas);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 5: D2CMP colour tables --------------------------------------------
ds1edit_sw_pure_test(test_render_sw_tables test_render_sw_tables.c
    ${_sw}/sw_tables.c ${_core}/palette.c)
ds1edit_sw_add_test(render_sw_tables test_render_sw_tables)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/sw_tables.c`.

- [ ] **Step 3: Write the implementation**

Create `src/render/sw_tables.h`:

```c
/* D2CMP colour tables for the software renderer: the COF blend modes
 * (trans_b 0-6) and the 32 distance-darkness levels, as 8-bit index lookups.
 *
 * With a full-length Pal.PL2 (0x6C327 bytes) the tables are copied from the
 * file -- they are BuildPaletteTransformTables' memoized output, so this is
 * exactly what the game blits through. A shorter palette falls back to the
 * formulas read out of D2CMP.dll (docs/guides/palettes-and-colour.md, which
 * byte-verified darkness, the three alpha tables, additive and multiply
 * against ACT1; screen is located but not byte-verified). No Allegro. */

#ifndef DS1EDIT_RENDER_SW_TABLES_H
#define DS1EDIT_RENDER_SW_TABLES_H

#include <stdint.h>

#include "core/palette.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SW_PL2_FULL_SIZE   0x6C327L
#define SW_PL2_OFF_DARK    0x00400L   /* 32 levels x 256, level L at (L-1)*256 */
#define SW_PL2_OFF_ALPHA75 0x03500L   /* source 191/255; COF_25TRANS * 256     */
#define SW_PL2_OFF_ALPHA50 0x13500L   /* source 127/255; COF_50TRANS * 256     */
#define SW_PL2_OFF_ALPHA25 0x23500L   /* source  63/255; COF_75TRANS * 256     */
#define SW_PL2_OFF_ADD     0x33500L   /* COF_ALPHA * 256                       */
#define SW_PL2_OFF_MUL     0x43500L   /* COF_LUMINANCE * 256                   */
#define SW_PL2_OFF_SCREEN  0x5B500L   /* COF_ALPHABRIGHT * 256                 */

typedef struct SW_TABLES
{
   uint8_t alpha25[256][256];   /* trans_b 0: [src][dst] */
   uint8_t alpha50[256][256];   /* trans_b 1 */
   uint8_t alpha75[256][256];   /* trans_b 2 */
   uint8_t add[256][256];       /* trans_b 3 */
   uint8_t mul[256][256];       /* trans_b 4 */
   uint8_t screen[256][256];    /* trans_b 6 */
   uint8_t dark[33][256];       /* level 0 (black) .. 32 (full light) */
   int     from_pl2;            /* 1 when copied from the file */
} SW_TABLES;

/* Copy from a full PL2, else generate from its first 1024 bytes (or a grey
   ramp when pl2 is NULL). 1 on success. */
int  sw_tables_build(SW_TABLES *t, const uint8_t *pl2, long pl2_size);

/* The formulas only, from an un-gamma'd palette. */
void sw_tables_generate(SW_TABLES *t, const RGBA_PALETTE *raw);

/* The flat [src * 256 + dst] table for a COF layer's trans_b; NULL for 5
   (unused), out-of-range values, or t == NULL. */
const uint8_t *sw_tables_blend(const SW_TABLES *t, int trans_b);

#ifdef __cplusplus
}
#endif

#endif
```

Create `src/render/sw_tables.c`:

```c
/* See sw_tables.h. */

#include <string.h>

#include "render/sw_tables.h"

static uint8_t nearest(const RGBA_PALETTE *pal, int r, int g, int b)
{
   if (r > 255) r = 255;
   if (g > 255) g = 255;
   if (b > 255) b = 255;
   if (r < 0) r = 0;
   if (g < 0) g = 0;
   if (b < 0) b = 0;
   return palette_find_closest(pal, (uint8_t) r, (uint8_t) g, (uint8_t) b);
}

void sw_tables_generate(SW_TABLES *t, const RGBA_PALETTE *raw)
{
   static const int alpha[3] = { 63, 127, 191 };
   uint8_t (*dst[3])[256];
   int a, s, d, L, i;

   if ((t == NULL) || (raw == NULL))
      return;
   dst[0] = t->alpha25;
   dst[1] = t->alpha50;
   dst[2] = t->alpha75;

   for (s = 0; s < 256; s++)
   {
      const RGBA_COLOR *cs = &raw->colors[s];
      for (d = 0; d < 256; d++)
      {
         const RGBA_COLOR *cd = &raw->colors[d];
         for (a = 0; a < 3; a++)
            dst[a][s][d] = nearest(raw,
               (cd->r * (255 - alpha[a]) + cs->r * alpha[a]) / 255,
               (cd->g * (255 - alpha[a]) + cs->g * alpha[a]) / 255,
               (cd->b * (255 - alpha[a]) + cs->b * alpha[a]) / 255);
         t->add[s][d] = nearest(raw, cs->r + cd->r, cs->g + cd->g, cs->b + cd->b);
         t->mul[s][d] = nearest(raw, cs->r * cd->r / 255, cs->g * cd->g / 255,
                                cs->b * cd->b / 255);
         t->screen[s][d] = nearest(raw, cs->r + cd->r - cs->r * cd->r / 255,
                                   cs->g + cd->g - cs->g * cd->g / 255,
                                   cs->b + cd->b - cs->b * cd->b / 255);
      }
   }
   for (L = 1; L <= 32; L++)
      for (i = 0; i < 256; i++)
         t->dark[L][i] = nearest(raw, (raw->colors[i].r * L) >> 5,
                                 (raw->colors[i].g * L) >> 5,
                                 (raw->colors[i].b * L) >> 5);
   memset(t->dark[0], nearest(raw, 0, 0, 0), 256);
   t->from_pl2 = 0;
}

int sw_tables_build(SW_TABLES *t, const uint8_t *pl2, long pl2_size)
{
   RGBA_PALETTE raw;
   int L;

   if (t == NULL)
      return 0;
   palette_d2_to_rgba(pl2, pl2_size, NULL, &raw);
   if ((pl2 != NULL) && (pl2_size >= SW_PL2_FULL_SIZE))
   {
      memcpy(t->alpha25, pl2 + SW_PL2_OFF_ALPHA25, sizeof(t->alpha25));
      memcpy(t->alpha50, pl2 + SW_PL2_OFF_ALPHA50, sizeof(t->alpha50));
      memcpy(t->alpha75, pl2 + SW_PL2_OFF_ALPHA75, sizeof(t->alpha75));
      memcpy(t->add, pl2 + SW_PL2_OFF_ADD, sizeof(t->add));
      memcpy(t->mul, pl2 + SW_PL2_OFF_MUL, sizeof(t->mul));
      memcpy(t->screen, pl2 + SW_PL2_OFF_SCREEN, sizeof(t->screen));
      for (L = 1; L <= 32; L++)
         memcpy(t->dark[L], pl2 + SW_PL2_OFF_DARK + (long) (L - 1) * 256, 256);
      memset(t->dark[0], nearest(&raw, 0, 0, 0), 256);
      t->from_pl2 = 1;
      return 1;
   }
   sw_tables_generate(t, &raw);
   return 1;
}

const uint8_t *sw_tables_blend(const SW_TABLES *t, int trans_b)
{
   if (t == NULL)
      return NULL;
   switch (trans_b)
   {
   case 0: return &t->alpha25[0][0];
   case 1: return &t->alpha50[0][0];
   case 2: return &t->alpha75[0][0];
   case 3: return &t->add[0][0];
   case 4: return &t->mul[0][0];
   case 6: return &t->screen[0][0];
   default: return NULL;
   }
}
```

Note: `misc_pl2_correct()` (`src/misc.c`) rewrites row 0 of each in-memory blend table to the identity; row 0 is the transparent source index, which no blit ever reads, so copying the corrected bytes changes nothing drawn.

Add `src/render/sw_tables.c` to `SOURCES` in `CMakeLists.txt` after `src/render/spritecache.c`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_sw_tables
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_sw_tables$" --output-on-failure
```
Expected: `7 Tests 0 Failures 1 Ignored` (the game-data case, with `DS1EDIT_TEST_PL2_DIR not set`), `100% tests passed`.

- [ ] **Step 5: Game-data lane (local only, vanilla 1.13c)**

The editor writes the raw `Pal.PL2` of each act to `assets/editor/pal<N>.bin` in its runtime directory (`misc_save_pal_on_disk`). Produce a vanilla set once Task 11's `sw_runtime smoke` exists, or now with any runtime whose `Ds1edit.ini` points at `F:\D2VersionChanger\VersionChanger\LoD\1.13c` and that has opened a map from each act. Then:

```bash
DS1EDIT_TEST_PL2_DIR="tmp/sw-smoke/runtime/assets/editor" "$CTEST" --test-dir tmp/build-sw -C Release -R "^render_sw_tables$" --output-on-failure -V
```
Expected: `7 Tests 0 Failures 0 Ignored`, and five `act N screen-blend formula differs ... in K of 65536 entries` messages recording K (evidence for the screen formula; not a gate). If darkness/alpha/add/multiply report mismatches, stop and report them: the guide claims 0, and the renderer uses the file bytes either way.

- [ ] **Step 6: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/sw_tables.h src/render/sw_tables.c test/test_render_sw_tables.c test/cmake/software_renderer.cmake CMakeLists.txt
git commit -m "Build the D2CMP blend and darkness tables the software renderer blits through"
```

---

### Task 6: Present (palette expansion, one write-only lock, fallback, pacing)

**Files:**
- Create: `src/render/present.h`, `src/render/present.c`, `test/test_render_present.c`
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: `DS1_CANVAS` (Task 1); `RGBA_PALETTE` (`src/core/palette.h`); Allegro 5 bitmaps and locking; `visual_allegro_init()` (`test/visual_test_support.h`, makes new bitmaps `ALLEGRO_MEMORY_BITMAP` + `ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE`).
- Produces (`src/render/present.h`):
  ```c
  typedef enum PRESENT_LAYOUT { PRESENT_ABGR = 0, PRESENT_ARGB = 1, PRESENT_RGBA = 2 } PRESENT_LAYOUT;  /* 32-bit value layouts */
  enum { PRESENT_LOCKED = 1, PRESENT_MEMORY_TARGET = 2, PRESENT_FALLBACK = 3 };
  uint32_t present_pack_rgb(PRESENT_LAYOUT layout, uint8_t r, uint8_t g, uint8_t b);
  void   present_build_table(uint32_t table[256], const RGBA_PALETTE *pal, PRESENT_LAYOUT layout);  /* [0] = opaque black */
  void   present_expand(const DS1_CANVAS *cv, const uint32_t table[256], uint8_t *dst, int32_t dst_pitch, int32_t w, int32_t h);
  double present_pace_delay(double now_s, double *next_s, double period_s);
  int    present_canvas_to_bitmap(struct ALLEGRO_BITMAP *dst, const DS1_CANVAS *cv, const RGBA_PALETTE *pal);  /* PRESENT_* or -1 */
  const char *present_warning(void);      /* NULL until a fallback/slow path was taken */
  const char *present_last_path(void);    /* "locked" | "memory-target" | "fallback" | "none" */
  void   present_force_fallback(int on);  /* test hook */
  void   present_reset(void);             /* test hook: forget table, warning, fallback bitmap */
  ```
  `dst_pitch` is signed: Allegro's OpenGL lock can return a bottom-up region with a negative pitch.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_present.c`:

```c
/* Present (present.c): packing per pixel layout, palette table with index 0
 * forced to opaque black, expansion with positive and negative pitch, the
 * locked upload into a bitmap, the forced fallback with its one-time warning,
 * table rebuild on a palette (gamma) change, and pacing arithmetic. Memory
 * bitmaps only: no display. The video-bitmap path is covered by
 * test_render_present_display (Task 14). */

#include <stdint.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "render/canvas.h"
#include "render/present.h"
#include "visual_test_support.h"

static RGBA_PALETTE s_pal;

void setUp(void)
{
   int i;
   TEST_ASSERT_EQUAL_INT(1, visual_allegro_init());
   present_reset();
   for (i = 0; i < 256; i++)
   {
      s_pal.colors[i].r = (uint8_t) i;
      s_pal.colors[i].g = (uint8_t) (255 - i);
      s_pal.colors[i].b = (uint8_t) (i * 3);
      s_pal.colors[i].a = (i == 0) ? 0 : 255;
   }
   s_pal.colors[0].r = 10;    /* index 0 must still come out black */
}

void tearDown(void) { present_reset(); }

void test_pack_follows_each_layout(void)
{
   TEST_ASSERT_EQUAL_HEX32(0xFF030201u, present_pack_rgb(PRESENT_ABGR, 1, 2, 3));
   TEST_ASSERT_EQUAL_HEX32(0xFF010203u, present_pack_rgb(PRESENT_ARGB, 1, 2, 3));
   TEST_ASSERT_EQUAL_HEX32(0x010203FFu, present_pack_rgb(PRESENT_RGBA, 1, 2, 3));
}

void test_table_forces_index_zero_to_opaque_black(void)
{
   uint32_t t[256];
   present_build_table(t, &s_pal, PRESENT_ABGR);
   TEST_ASSERT_EQUAL_HEX32(0xFF000000u, t[0]);
   TEST_ASSERT_EQUAL_HEX32(present_pack_rgb(PRESENT_ABGR, 5, 250, 15), t[5]);
}

void test_expand_handles_a_negative_pitch(void)
{
   uint8_t idx[2 * 3] = { 1, 2, 3, 4, 5, 6 };
   uint32_t out[3 * 2], t[256];
   DS1_CANVAS cv;
   int i;

   for (i = 0; i < 256; i++)
      t[i] = (uint32_t) i;
   canvas_init_view(&cv, idx, NULL, 2, 3, 2);
   /* bottom-up: row 0 of the image is the last row of the buffer */
   present_expand(&cv, t, (uint8_t *) (out + 4), -(int32_t) (2 * sizeof(uint32_t)), 2, 3);
   TEST_ASSERT_EQUAL_UINT32(1, out[4]);
   TEST_ASSERT_EQUAL_UINT32(2, out[5]);
   TEST_ASSERT_EQUAL_UINT32(3, out[2]);
   TEST_ASSERT_EQUAL_UINT32(6, out[1]);
}

static void assert_bitmap_shows(ALLEGRO_BITMAP *bmp, const DS1_CANVAS *cv, const RGBA_PALETTE *pal)
{
   ALLEGRO_LOCKED_REGION *lr = al_lock_bitmap(bmp, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                                              ALLEGRO_LOCK_READONLY);
   int32_t x, y;

   TEST_ASSERT_NOT_NULL(lr);
   for (y = 0; y < cv->h; y++)
      for (x = 0; x < cv->w; x++)
      {
         const uint8_t *p = (const uint8_t *) lr->data + (ptrdiff_t) y * lr->pitch + x * 4;
         uint8_t i = cv->pixels[y * cv->stride + x];
         TEST_ASSERT_EQUAL_HEX8(i ? pal->colors[i].r : 0, p[0]);
         TEST_ASSERT_EQUAL_HEX8(i ? pal->colors[i].g : 0, p[1]);
         TEST_ASSERT_EQUAL_HEX8(i ? pal->colors[i].b : 0, p[2]);
         TEST_ASSERT_EQUAL_HEX8(255, p[3]);
      }
   al_unlock_bitmap(bmp);
}

static void make_canvas(DS1_CANVAS *cv)
{
   int32_t x, y;
   TEST_ASSERT_EQUAL_INT(1, canvas_create(cv, 8, 4, 0));
   for (y = 0; y < 4; y++)
      for (x = 0; x < 8; x++)
         cv->pixels[y * 8 + x] = (uint8_t) (y * 8 + x);   /* includes index 0 */
}

void test_upload_into_a_memory_bitmap(void)
{
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = al_create_bitmap(8, 4);

   make_canvas(&cv);
   TEST_ASSERT_EQUAL_INT(PRESENT_MEMORY_TARGET, present_canvas_to_bitmap(bmp, &cv, &s_pal));
   TEST_ASSERT_EQUAL_STRING("memory-target", present_last_path());
   assert_bitmap_shows(bmp, &cv, &s_pal);
   TEST_ASSERT_NULL(present_warning());          /* no display: not a slow path */
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

void test_forced_fallback_draws_the_same_picture_and_warns(void)
{
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = al_create_bitmap(8, 4);

   make_canvas(&cv);
   present_force_fallback(1);
   TEST_ASSERT_EQUAL_INT(PRESENT_FALLBACK, present_canvas_to_bitmap(bmp, &cv, &s_pal));
   TEST_ASSERT_EQUAL_STRING("fallback", present_last_path());
   assert_bitmap_shows(bmp, &cv, &s_pal);
   TEST_ASSERT_NOT_NULL(present_warning());
   TEST_ASSERT_NOT_NULL(strstr(present_warning(), "fallback"));
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

void test_palette_change_rebuilds_the_table(void)
{
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = al_create_bitmap(8, 4);

   make_canvas(&cv);
   present_canvas_to_bitmap(bmp, &cv, &s_pal);
   s_pal.colors[5].r = 99;                      /* what a gamma change does in place */
   present_canvas_to_bitmap(bmp, &cv, &s_pal);
   assert_bitmap_shows(bmp, &cv, &s_pal);
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

void test_canvas_larger_than_the_bitmap_copies_the_overlap(void)
{
   DS1_CANVAS cv, small;
   ALLEGRO_BITMAP *bmp = al_create_bitmap(4, 2);

   make_canvas(&cv);
   TEST_ASSERT_EQUAL_INT(PRESENT_MEMORY_TARGET, present_canvas_to_bitmap(bmp, &cv, &s_pal));
   canvas_init_view(&small, cv.pixels, NULL, 4, 2, cv.stride);
   assert_bitmap_shows(bmp, &small, &s_pal);
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

void test_bad_input_is_refused(void)
{
   DS1_CANVAS cv;
   make_canvas(&cv);
   TEST_ASSERT_EQUAL_INT(-1, present_canvas_to_bitmap(NULL, &cv, &s_pal));
   TEST_ASSERT_EQUAL_INT(-1, present_canvas_to_bitmap((ALLEGRO_BITMAP *) NULL, NULL, &s_pal));
   canvas_destroy(&cv);
}

/* This Unity build has double assertions disabled (UNITY_EXCLUDE_DOUBLE is
   the default), so compare by hand. */
static int near(double a, double b) { return (a - b < 1e-9) && (b - a < 1e-9); }

void test_pacing_schedules_every_period_and_resyncs_when_late(void)
{
   double next = 0.0;
   TEST_ASSERT_TRUE(near(0.0, present_pace_delay(10.0, &next, 0.02)));
   TEST_ASSERT_TRUE(near(10.02, next));
   TEST_ASSERT_TRUE(near(0.015, present_pace_delay(10.005, &next, 0.02)));
   TEST_ASSERT_TRUE(near(10.04, next));
   TEST_ASSERT_TRUE(near(0.0, present_pace_delay(11.0, &next, 0.02)));   /* fell behind */
   TEST_ASSERT_TRUE(near(11.02, next));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_pack_follows_each_layout);
   RUN_TEST(test_table_forces_index_zero_to_opaque_black);
   RUN_TEST(test_expand_handles_a_negative_pitch);
   RUN_TEST(test_upload_into_a_memory_bitmap);
   RUN_TEST(test_forced_fallback_draws_the_same_picture_and_warns);
   RUN_TEST(test_palette_change_rebuilds_the_table);
   RUN_TEST(test_canvas_larger_than_the_bitmap_copies_the_overlap);
   RUN_TEST(test_bad_input_is_refused);
   RUN_TEST(test_pacing_schedules_every_period_and_resyncs_when_late);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 6: present ---------------------------------------------------------
ds1edit_visual_test(test_render_present ALLEGRO SOURCES test_render_present.c
    ${_sw}/present.c ${_sw}/canvas.c ${_vsup} ${_vglobals})
ds1edit_sw_add_test(render_present test_render_present)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/present.c`.

- [ ] **Step 3: Write the implementation**

Create `src/render/present.h`:

```c
/* Present: the only unit on the software path that touches Allegro.
 *
 * The canvas holds palette indices only. Each frame, present expands it
 * through a 256-entry index -> packed-RGB table (rebuilt only when the palette
 * bytes change: an act change or a gamma change) into ONE write-only lock of
 * the target bitmap. If the target cannot be locked it falls back to one
 * memory-bitmap draw -- a single software upload -- and says so once, on
 * stderr and through present_warning() for the status bar. It never draws
 * pixel by pixel. */

#ifndef DS1EDIT_RENDER_PRESENT_H
#define DS1EDIT_RENDER_PRESENT_H

#include <stdint.h>

#include "core/palette.h"
#include "render/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Layout of one 32-bit pixel *value* (little-endian hosts: every ship target). */
typedef enum PRESENT_LAYOUT
{
   PRESENT_ABGR = 0,    /* 0xAABBGGRR: Allegro ABGR_8888 / ABGR_8888_LE / XBGR_8888 */
   PRESENT_ARGB = 1,    /* 0xAARRGGBB: Allegro ARGB_8888 / XRGB_8888               */
   PRESENT_RGBA = 2     /* 0xRRGGBBAA: Allegro RGBA_8888                           */
} PRESENT_LAYOUT;

enum
{
   PRESENT_LOCKED        = 1,   /* video bitmap locked write-only and filled  */
   PRESENT_MEMORY_TARGET = 2,   /* the target is itself a memory bitmap       */
   PRESENT_FALLBACK      = 3    /* lock failed: one memory-bitmap draw        */
};

uint32_t present_pack_rgb(PRESENT_LAYOUT layout, uint8_t r, uint8_t g, uint8_t b);
void     present_build_table(uint32_t table[256], const RGBA_PALETTE *pal, PRESENT_LAYOUT layout);
void     present_expand(const DS1_CANVAS *cv, const uint32_t table[256],
                        uint8_t *dst, int32_t dst_pitch, int32_t w, int32_t h);

/* Seconds to wait before the next present so presents land every period_s.
   *next_s holds the schedule (start it at 0). When more than one period late
   it resynchronises instead of bursting. */
double   present_pace_delay(double now_s, double *next_s, double period_s);

struct ALLEGRO_BITMAP;
int      present_canvas_to_bitmap(struct ALLEGRO_BITMAP *dst, const DS1_CANVAS *cv,
                                  const RGBA_PALETTE *pal);
const char *present_warning(void);
const char *present_last_path(void);
void     present_force_fallback(int on);
void     present_reset(void);

#ifdef __cplusplus
}
#endif

#endif
```

Create `src/render/present.c`:

```c
/* See present.h. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>

#include "render/present.h"

static uint32_t        s_table[256];
static int             s_table_layout = -1;
static uint32_t        s_table_sum;
static const RGBA_PALETTE *s_table_pal;
static int             s_force_fallback;
static int             s_warned;
static char            s_warning[200];
static const char     *s_last_path = "none";
static ALLEGRO_BITMAP *s_fallback_bmp;

uint32_t present_pack_rgb(PRESENT_LAYOUT layout, uint8_t r, uint8_t g, uint8_t b)
{
   switch (layout)
   {
   case PRESENT_ARGB:
      return 0xFF000000u | ((uint32_t) r << 16) | ((uint32_t) g << 8) | b;
   case PRESENT_RGBA:
      return ((uint32_t) r << 24) | ((uint32_t) g << 16) | ((uint32_t) b << 8) | 0xFFu;
   case PRESENT_ABGR:
   default:
      return 0xFF000000u | ((uint32_t) b << 16) | ((uint32_t) g << 8) | r;
   }
}

void present_build_table(uint32_t table[256], const RGBA_PALETTE *pal, PRESENT_LAYOUT layout)
{
   int i;
   for (i = 0; i < 256; i++)
   {
      if (pal != NULL)
         table[i] = present_pack_rgb(layout, pal->colors[i].r, pal->colors[i].g,
                                     pal->colors[i].b);
      else
         table[i] = present_pack_rgb(layout, (uint8_t) i, (uint8_t) i, (uint8_t) i);
   }
   /* Index 0 is the transparency key; the GPU path clears to black and copies
      screen_buff with blending off, so an empty pixel is opaque black there. */
   table[0] = present_pack_rgb(layout, 0, 0, 0);
}

void present_expand(const DS1_CANVAS *cv, const uint32_t table[256],
                    uint8_t *dst, int32_t dst_pitch, int32_t w, int32_t h)
{
   int32_t x, y;

   if ((cv == NULL) || (cv->pixels == NULL) || (table == NULL) || (dst == NULL))
      return;
   if (w > cv->w)
      w = cv->w;
   if (h > cv->h)
      h = cv->h;
   for (y = 0; y < h; y++)
   {
      const uint8_t *s = cv->pixels + (size_t) y * cv->stride;
      uint32_t      *d = (uint32_t *) (dst + (ptrdiff_t) y * dst_pitch);
      for (x = 0; x < w; x++)
         d[x] = table[s[x]];
   }
}

double present_pace_delay(double now_s, double *next_s, double period_s)
{
   double delay;

   if (next_s == NULL)
      return 0.0;
   if ((*next_s <= 0.0) || (now_s - *next_s > period_s))
   {
      *next_s = now_s + period_s;
      return 0.0;
   }
   delay = *next_s - now_s;
   *next_s += period_s;
   return (delay > 0.0) ? delay : 0.0;
}

static uint32_t palette_sum(const RGBA_PALETTE *pal)
{
   const uint8_t *p = (const uint8_t *) pal;
   uint32_t h = 2166136261u;
   size_t i;

   for (i = 0; i < sizeof(*pal); i++)
      h = (h ^ p[i]) * 16777619u;
   return h;
}

static const uint32_t *table_for(const RGBA_PALETTE *pal, PRESENT_LAYOUT layout)
{
   uint32_t sum = (pal != NULL) ? palette_sum(pal) : 0u;

   if ((s_table_layout != (int) layout) || (s_table_pal != pal) || (s_table_sum != sum))
   {
      present_build_table(s_table, pal, layout);
      s_table_layout = (int) layout;
      s_table_pal    = pal;
      s_table_sum    = sum;
   }
   return s_table;
}

static int layout_for_format(int format, PRESENT_LAYOUT *out)
{
   switch (format)
   {
   case ALLEGRO_PIXEL_FORMAT_ARGB_8888:
   case ALLEGRO_PIXEL_FORMAT_XRGB_8888:
      *out = PRESENT_ARGB;
      return 1;
   case ALLEGRO_PIXEL_FORMAT_ABGR_8888:
   case ALLEGRO_PIXEL_FORMAT_XBGR_8888:
   case ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE:
      *out = PRESENT_ABGR;
      return 1;
   case ALLEGRO_PIXEL_FORMAT_RGBA_8888:
      *out = PRESENT_RGBA;
      return 1;
   default:
      return 0;
   }
}

static void warn_once(const char *msg)
{
   if (s_warned)
      return;
   s_warned = 1;
   snprintf(s_warning, sizeof(s_warning), "%s", msg);
   fprintf(stderr, "%s\n", msg);
   fflush(stderr);
}

/* One write-only lock of the w x h region, expanded straight into it. */
static int upload_locked(ALLEGRO_BITMAP *bmp, const DS1_CANVAS *cv, const RGBA_PALETTE *pal)
{
   ALLEGRO_LOCKED_REGION *lr;
   PRESENT_LAYOUT layout;
   int32_t w = al_get_bitmap_width(bmp), h = al_get_bitmap_height(bmp);

   if (w > cv->w)
      w = cv->w;
   if (h > cv->h)
      h = cv->h;
   if ((w <= 0) || (h <= 0))
      return 0;
   lr = al_lock_bitmap_region(bmp, 0, 0, w, h, ALLEGRO_PIXEL_FORMAT_ANY,
                              ALLEGRO_LOCK_WRITEONLY);
   if (lr == NULL)
      return 0;
   if (!layout_for_format(lr->format, &layout))
   {
      /* An unusual native format: let Allegro convert once on unlock. */
      al_unlock_bitmap(bmp);
      lr = al_lock_bitmap_region(bmp, 0, 0, w, h, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                                 ALLEGRO_LOCK_WRITEONLY);
      if (lr == NULL)
         return 0;
      layout = PRESENT_ABGR;
   }
   present_expand(cv, table_for(pal, layout), (uint8_t *) lr->data, lr->pitch, w, h);
   al_unlock_bitmap(bmp);
   return 1;
}

static int upload_fallback(ALLEGRO_BITMAP *dst, const DS1_CANVAS *cv, const RGBA_PALETTE *pal)
{
   ALLEGRO_BITMAP *prev_target = al_get_target_bitmap();
   int prev_flags = al_get_new_bitmap_flags();
   int prev_format = al_get_new_bitmap_format();
   int op, src, dstb;

   if ((s_fallback_bmp != NULL) &&
       ((al_get_bitmap_width(s_fallback_bmp) != cv->w) ||
        (al_get_bitmap_height(s_fallback_bmp) != cv->h)))
   {
      al_destroy_bitmap(s_fallback_bmp);
      s_fallback_bmp = NULL;
   }
   if (s_fallback_bmp == NULL)
   {
      al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
      al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE);
      s_fallback_bmp = al_create_bitmap(cv->w, cv->h);
      al_set_new_bitmap_flags(prev_flags);
      al_set_new_bitmap_format(prev_format);
      if (s_fallback_bmp == NULL)
         return -1;
   }
   if (!upload_locked(s_fallback_bmp, cv, pal))
      return -1;

   al_set_target_bitmap(dst);
   al_get_blender(&op, &src, &dstb);
   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO);
   al_draw_bitmap(s_fallback_bmp, 0, 0, 0);
   al_set_blender(op, src, dstb);
   if (prev_target != NULL)
      al_set_target_bitmap(prev_target);
   return PRESENT_FALLBACK;
}

int present_canvas_to_bitmap(ALLEGRO_BITMAP *dst, const DS1_CANVAS *cv, const RGBA_PALETTE *pal)
{
   int memory_target, rc;

   if ((dst == NULL) || (cv == NULL) || (cv->pixels == NULL))
      return -1;
   memory_target = (al_get_bitmap_flags(dst) & ALLEGRO_MEMORY_BITMAP) != 0;

   if (!s_force_fallback && upload_locked(dst, cv, pal))
   {
      if (memory_target)
      {
         /* Headless renders target memory on purpose; with a display it means
            screen_buff never became a video bitmap. */
         if (al_get_current_display() != NULL)
            warn_once("present: the frame buffer is a MEMORY bitmap, so every frame "
                      "is one software upload to the display (slow path)");
         s_last_path = "memory-target";
         return PRESENT_MEMORY_TARGET;
      }
      s_last_path = "locked";
      return PRESENT_LOCKED;
   }

   rc = upload_fallback(dst, cv, pal);
   if (rc == PRESENT_FALLBACK)
   {
      warn_once(s_force_fallback
                ? "present: fallback forced; presenting through one memory-bitmap "
                  "upload per frame"
                : "present: could not lock the video frame buffer; presenting "
                  "through one memory-bitmap upload per frame (fallback)");
      s_last_path = "fallback";
   }
   return rc;
}

const char *present_warning(void)
{
   return s_warned ? s_warning : NULL;
}

const char *present_last_path(void)
{
   return s_last_path;
}

void present_force_fallback(int on)
{
   s_force_fallback = on ? 1 : 0;
}

void present_reset(void)
{
   if (s_fallback_bmp != NULL)
      al_destroy_bitmap(s_fallback_bmp);
   s_fallback_bmp   = NULL;
   s_table_layout   = -1;
   s_table_pal      = NULL;
   s_table_sum      = 0;
   s_force_fallback = 0;
   s_warned         = 0;
   s_warning[0]     = 0;
   s_last_path      = "none";
}
```

The forced-fallback warning text contains "fallback" in both branches (the test checks it). Add `src/render/present.c` to `SOURCES` in `CMakeLists.txt` after `src/render/sw_tables.c`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_present
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_present$" --output-on-failure
```
Expected: `9 Tests 0 Failures 0 Ignored`, `100% tests passed`.

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/present.h src/render/present.c test/test_render_present.c test/cmake/software_renderer.cmake CMakeLists.txt
git commit -m "Present an index canvas in one write-only lock, falling back once and loudly"
```

---

### Task 7: `renderer = software | gpu | auto` and the bound device

**Files:**
- Create: `src/render/render_select.h`, `src/render/render_select.c`, `src/render/display_device.h`, `src/render/display_device.c`, `test/test_render_select.c`
- Modify: `src/structs.h` (`CONFIG_S`), `src/config.c` (`ini_create` text, `ini_read` table), `Ds1edit.ini.sample`, `src/main.c`
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces (`src/render/render_select.h`):
  ```c
  enum { DS1EDIT_RENDERER_GPU = 0, DS1EDIT_RENDERER_SOFTWARE = 1, DS1EDIT_RENDERER_AUTO = 2 };
  int         render_select_parse(const char *text);                  /* -1 when not software/gpu/auto */
  const char *render_select_name(int renderer);                       /* "gpu" | "software" | "auto" | "?" */
  int         render_select_is_software_device(const char *device);   /* 1 for GDI Generic, llvmpipe, WARP, ... */
  int         render_select_resolve(int setting, const char *device); /* GPU or SOFTWARE; device may be NULL */
  void        render_select_set_active(int renderer);
  int         render_select_active(void);                             /* GPU until set */
  ```
  (`src/render/display_device.h`): `int display_device_name(struct ALLEGRO_DISPLAY *d, char *out, size_t cap);` returns 1 and the `GL_RENDERER` string (OpenGL) or the Direct3D 9 adapter description (Windows), else 0 and `""`.
  `CONFIG_S.renderer` (int, a `DS1EDIT_RENDERER_*` value). `main.c` prints exactly one line `renderer: <active> (renderer = <setting>, device "<device or unknown>")` to stdout and stderr; the Python lanes parse it.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_select.c`:

```c
/* renderer = software | gpu | auto (render_select.c). */

#include <stddef.h>

#include "unity/unity.h"
#include "render/render_select.h"

void setUp(void) {}
void tearDown(void) {}

void test_parse_accepts_the_three_values_in_any_case_with_spaces(void)
{
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_SOFTWARE, render_select_parse("software"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_SOFTWARE, render_select_parse("  Software "));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU, render_select_parse("GPU"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_AUTO, render_select_parse("auto"));
}

void test_parse_rejects_everything_else(void)
{
   TEST_ASSERT_EQUAL_INT(-1, render_select_parse(""));
   TEST_ASSERT_EQUAL_INT(-1, render_select_parse(NULL));
   TEST_ASSERT_EQUAL_INT(-1, render_select_parse("vulkan"));
   TEST_ASSERT_EQUAL_INT(-1, render_select_parse("soft"));
   TEST_ASSERT_EQUAL_INT(-1, render_select_parse("software2"));
}

void test_names(void)
{
   TEST_ASSERT_EQUAL_STRING("gpu", render_select_name(DS1EDIT_RENDERER_GPU));
   TEST_ASSERT_EQUAL_STRING("software", render_select_name(DS1EDIT_RENDERER_SOFTWARE));
   TEST_ASSERT_EQUAL_STRING("auto", render_select_name(DS1EDIT_RENDERER_AUTO));
   TEST_ASSERT_EQUAL_STRING("?", render_select_name(7));
}

void test_software_devices_are_recognised(void)
{
   TEST_ASSERT_TRUE(render_select_is_software_device("GDI Generic"));
   TEST_ASSERT_TRUE(render_select_is_software_device("llvmpipe (LLVM 15.0.7, 256 bits)"));
   TEST_ASSERT_TRUE(render_select_is_software_device("D3D12 (Microsoft Basic Render Driver)"));
   TEST_ASSERT_TRUE(render_select_is_software_device("Microsoft Basic Render Driver"));
   TEST_ASSERT_TRUE(render_select_is_software_device("softpipe"));
   TEST_ASSERT_FALSE(render_select_is_software_device("NVIDIA GeForce RTX 3080/PCIe/SSE2"));
   TEST_ASSERT_FALSE(render_select_is_software_device("Intel(R) UHD Graphics 630"));
   TEST_ASSERT_FALSE(render_select_is_software_device(NULL));
}

void test_resolve(void)
{
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_SOFTWARE,
                         render_select_resolve(DS1EDIT_RENDERER_SOFTWARE, "NVIDIA GeForce"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU,
                         render_select_resolve(DS1EDIT_RENDERER_GPU, "llvmpipe"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_SOFTWARE,
                         render_select_resolve(DS1EDIT_RENDERER_AUTO, "llvmpipe"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU,
                         render_select_resolve(DS1EDIT_RENDERER_AUTO, "NVIDIA GeForce"));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU,
                         render_select_resolve(DS1EDIT_RENDERER_AUTO, NULL));
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU, render_select_resolve(42, "llvmpipe"));
}

void test_active_defaults_to_gpu_and_can_be_set(void)
{
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU, render_select_active());
   render_select_set_active(DS1EDIT_RENDERER_SOFTWARE);
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_SOFTWARE, render_select_active());
   render_select_set_active(DS1EDIT_RENDERER_AUTO);      /* not a concrete renderer */
   TEST_ASSERT_EQUAL_INT(DS1EDIT_RENDERER_GPU, render_select_active());
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_accepts_the_three_values_in_any_case_with_spaces);
   RUN_TEST(test_parse_rejects_everything_else);
   RUN_TEST(test_names);
   RUN_TEST(test_software_devices_are_recognised);
   RUN_TEST(test_resolve);
   RUN_TEST(test_active_defaults_to_gpu_and_can_be_set);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 7: renderer selection ----------------------------------------------
ds1edit_sw_pure_test(test_render_select test_render_select.c ${_sw}/render_select.c)
ds1edit_sw_add_test(render_select test_render_select)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/render_select.c`.

- [ ] **Step 3: Write `render_select`**

Create `src/render/render_select.h`:

```c
/* Which map renderer runs: the INI's `renderer = software | gpu | auto`,
 * resolved against the device the display bound. During the transition
 * (spec, Selection) `auto` picks software only on a known software
 * rasterizer. No Allegro dependency. */

#ifndef DS1EDIT_RENDER_SELECT_H
#define DS1EDIT_RENDER_SELECT_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
   DS1EDIT_RENDERER_GPU      = 0,
   DS1EDIT_RENDERER_SOFTWARE = 1,
   DS1EDIT_RENDERER_AUTO     = 2
};

int         render_select_parse(const char *text);
const char *render_select_name(int renderer);
int         render_select_is_software_device(const char *device);
int         render_select_resolve(int setting, const char *device);
void        render_select_set_active(int renderer);
int         render_select_active(void);

#ifdef __cplusplus
}
#endif

#endif
```

Create `src/render/render_select.c`:

```c
/* See render_select.h. */

#include <ctype.h>
#include <string.h>

#include "render/render_select.h"

/* Renderer strings of rasterizers that run on the CPU. "GDI Generic" is
   Microsoft's OpenGL 1.1 fallback; llvmpipe/softpipe/swrast are Mesa's;
   "Microsoft Basic Render Driver" is WARP behind Direct3D (and behind Mesa's
   d3d12 driver, whose string wraps it). */
static const char *const k_software_devices[] = {
   "GDI Generic", "llvmpipe", "softpipe", "swrast", "Software Rasterizer",
   "Microsoft Basic Render Driver", "WARP"
};

static int s_active = DS1EDIT_RENDERER_GPU;

static int word_is(const char *start, size_t len, const char *word)
{
   size_t i;
   if (strlen(word) != len)
      return 0;
   for (i = 0; i < len; i++)
      if (tolower((unsigned char) start[i]) != word[i])
         return 0;
   return 1;
}

int render_select_parse(const char *text)
{
   const char *start, *end;

   if (text == NULL)
      return -1;
   start = text;
   while (isspace((unsigned char) *start))
      start++;
   end = start + strlen(start);
   while ((end > start) && isspace((unsigned char) end[-1]))
      end--;
   if (word_is(start, (size_t) (end - start), "software"))
      return DS1EDIT_RENDERER_SOFTWARE;
   if (word_is(start, (size_t) (end - start), "gpu"))
      return DS1EDIT_RENDERER_GPU;
   if (word_is(start, (size_t) (end - start), "auto"))
      return DS1EDIT_RENDERER_AUTO;
   return -1;
}

const char *render_select_name(int renderer)
{
   switch (renderer)
   {
   case DS1EDIT_RENDERER_GPU:      return "gpu";
   case DS1EDIT_RENDERER_SOFTWARE: return "software";
   case DS1EDIT_RENDERER_AUTO:     return "auto";
   default:                        return "?";
   }
}

int render_select_is_software_device(const char *device)
{
   size_t i;

   if (device == NULL)
      return 0;
   for (i = 0; i < sizeof(k_software_devices) / sizeof(k_software_devices[0]); i++)
      if (strstr(device, k_software_devices[i]) != NULL)
         return 1;
   return 0;
}

int render_select_resolve(int setting, const char *device)
{
   switch (setting)
   {
   case DS1EDIT_RENDERER_SOFTWARE:
      return DS1EDIT_RENDERER_SOFTWARE;
   case DS1EDIT_RENDERER_AUTO:
      return render_select_is_software_device(device) ? DS1EDIT_RENDERER_SOFTWARE
                                                      : DS1EDIT_RENDERER_GPU;
   default:
      return DS1EDIT_RENDERER_GPU;
   }
}

void render_select_set_active(int renderer)
{
   s_active = (renderer == DS1EDIT_RENDERER_SOFTWARE) ? DS1EDIT_RENDERER_SOFTWARE
                                                      : DS1EDIT_RENDERER_GPU;
}

int render_select_active(void)
{
   return s_active;
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_select
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_select$" --output-on-failure
```
Expected: `6 Tests 0 Failures 0 Ignored`, `100% tests passed`.

- [ ] **Step 5: Name the bound device**

Create `src/render/display_device.h`:

```c
/* The device a display actually bound: GL_RENDERER on OpenGL, the Direct3D 9
 * adapter description on Windows Direct3D. Used to resolve renderer = auto
 * and to report software rasterizers. */

#ifndef DS1EDIT_RENDER_DISPLAY_DEVICE_H
#define DS1EDIT_RENDER_DISPLAY_DEVICE_H

#include <stddef.h>

struct ALLEGRO_DISPLAY;

/* 1 and the name in out (truncated to cap), or 0 and "" when unknown. On
   OpenGL this makes the display's backbuffer the target bitmap. */
int display_device_name(struct ALLEGRO_DISPLAY *d, char *out, size_t cap);

#endif
```

Create `src/render/display_device.c`:

```c
/* See display_device.h. */

#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#ifdef DS1EDIT_HAVE_GL
#include <allegro5/allegro_opengl.h>
#endif
#if defined(_WIN32)
#define COBJMACROS
#include <d3d9.h>
#include <allegro5/allegro_direct3d.h>
#endif

#include "render/display_device.h"

int display_device_name(ALLEGRO_DISPLAY *d, char *out, size_t cap)
{
   int flags;

   if ((out == NULL) || (cap == 0))
      return 0;
   out[0] = 0;
   if (d == NULL)
      return 0;
   flags = al_get_display_flags(d);

#ifdef DS1EDIT_HAVE_GL
   if (flags & ALLEGRO_OPENGL)
   {
      const char *r;
      /* glGetString needs this display's context current. */
      al_set_target_backbuffer(d);
      r = (const char *) glGetString(GL_RENDERER);
      if (r == NULL)
         return 0;
      snprintf(out, cap, "%s", r);
      return 1;
   }
#endif

#if defined(_WIN32)
   if (flags & ALLEGRO_DIRECT3D_INTERNAL)
   {
      LPDIRECT3DDEVICE9              dev = al_get_d3d_device(d);
      IDirect3D9                    *d3d = NULL;
      D3DDEVICE_CREATION_PARAMETERS  cp;
      D3DADAPTER_IDENTIFIER9         id;

      if (dev == NULL)
         return 0;
      if (FAILED(IDirect3DDevice9_GetDirect3D(dev, &d3d)) || (d3d == NULL))
         return 0;
      if (SUCCEEDED(IDirect3DDevice9_GetCreationParameters(dev, &cp)) &&
          SUCCEEDED(IDirect3D9_GetAdapterIdentifier(d3d, cp.AdapterOrdinal, 0, &id)))
         snprintf(out, cap, "%s", id.Description);
      IDirect3D9_Release(d3d);
      return out[0] != 0;
   }
#endif

   (void) flags;
   return 0;
}
```

The COM calls go through the vtable macros, so no `d3d9.lib` import is needed. Add both new `.c` files to `SOURCES` in `CMakeLists.txt` after `src/render/present.c`:

```cmake
    src/render/render_select.c
    src/render/display_device.c
```

- [ ] **Step 6: Add the INI setting**

In `src/structs.h`, in `CONFIG_S`, add after `int      fullscreen;`:

```c
   int      renderer;     // DS1EDIT_RENDERER_* (render/render_select.h): renderer = software|gpu|auto
```

In `src/config.c`:

1. Add `#include "render/render_select.h"` after `#include "core/compose_presets.h"`.
2. In `ini_create()`, replace

```c
       "screen_height = 600\n"
       "\n"
```

with

```c
       "screen_height = 600\n"
       "\n"
       "\n"
       "; map renderer: software composes the map in 8-bit palette indices on\n"
       "; the CPU and uploads one finished frame (no 3D acceleration needed);\n"
       "; gpu draws every tile through the graphics driver; auto picks software\n"
       "; when the driver is a software rasterizer (WARP, llvmpipe, GDI Generic)\n"
       "; ======================================================================\n"
       "renderer = gpu\n"
       "\n"
```

3. In `ini_read()`, add `T_RENDERER` as the last member of the local `TYPE_E` enum (after `T_ZOOM`), add the table row after the `full_screen` row:

```c
           {"renderer", T_RENDERER, &glb_config.renderer, "gpu"},
```

and add this case after the `case T_ZOOM:` block:

```c
      // renderer = software | gpu | auto
      case T_RENDERER:
         n = render_select_parse(str);
         if (n < 0)
         {
            fprintf(stderr, "ini_read(): renderer = %s is not software, gpu or auto; "
                            "using gpu\n", str);
            n = DS1EDIT_RENDERER_GPU;
         }
         *((int *)datas[i].data_ptr) = n;
         break;
```

In `Ds1edit.ini.sample`, add after the `screen_height = 720` line (and its following blank line):

```ini

; Map renderer. software composes the map in 8-bit palette indices on the
; CPU and uploads one finished frame per refresh -- use it on virtual
; machines, Remote Desktop, or any machine without a working 3D driver.
; gpu draws every tile through the graphics driver. auto picks software
; when the driver is a software rasterizer (WARP, llvmpipe, GDI Generic).
; Valid values: software, gpu, auto. Default: gpu.
renderer = gpu
```

- [ ] **Step 7: Resolve and report the renderer in `main.c`**

In `src/main.c`:

1. Add after `#include "render/preview.h"`:

```c
#include "render/render_select.h"
#include "render/display_device.h"
```

2. Delete the static function `ds1edit_renderer_is_software()` (its list now lives in `render_select.c`), and in `ds1edit_report_display_driver()` replace the whole `#ifdef DS1EDIT_HAVE_GL ... #endif /* DS1EDIT_HAVE_GL */` block with:

```c
   {
      char device[256];

      if (display_device_name(d, device, sizeof(device)))
      {
         fprintf(stderr, "display: renderer \"%s\"\n", device);
         if (render_select_is_software_device(device))
            fprintf(stderr,
                    "display: WARNING -- that is a software rasterizer, not a "
                    "GPU. With renderer = gpu expect well under one frame per "
                    "second; set renderer = software (or auto) in Ds1edit.ini, "
                    "or install the graphics vendor's driver.\n");
      }
   }
```

3. Immediately before the line `// headless mode : render one frame and save to file, then exit`, add:

```c
   /* No display yet (and none at all in headless mode): auto cannot see the
      device here, so it means gpu until the display exists. */
   render_select_set_active(render_select_resolve(glb_config.renderer, NULL));
```

4. After the `if (a5_display == NULL) { ... }` display-creation block (the one ending with `ds1edit_error(tmp);` / `}` / `}` just before `sprintf(tmp, "DS1 Editor v%s (%s), Allegro %i.%i.%i", ...)`), add:

```c
   /* Pick the map renderer now that the bound device is known. */
   {
      char device[256];
      int  active;

      if (!display_device_name(a5_display, device, sizeof(device)))
         device[0] = 0;
      active = render_select_resolve(glb_config.renderer, device[0] ? device : NULL);
      render_select_set_active(active);
      printf("renderer: %s (renderer = %s, device \"%s\")\n",
             render_select_name(active), render_select_name(glb_config.renderer),
             device[0] ? device : "unknown");
      fprintf(stderr, "renderer: %s (renderer = %s, device \"%s\")\n",
              render_select_name(active), render_select_name(glb_config.renderer),
              device[0] ? device : "unknown");
      fflush(stdout);
   }
```

- [ ] **Step 8: Build the editor and re-run the unit tests**

```bash
"$CMAKE" --build tmp/build-sw --config Release --target ds1edit test_render_select
"$CTEST" --test-dir tmp/build-sw -C Release -L portable --output-on-failure
```
Expected: `ds1edit` links with no new warnings from `display_device.c`; every `portable` test passes (the full portable suite guards against the `structs.h` change). The runtime check of the INI value happens in Task 11's smoke run (the line `renderer: software (renderer = software, ...)`).

- [ ] **Step 9: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/render_select.h src/render/render_select.c src/render/display_device.h src/render/display_device.c test/test_render_select.c test/cmake/software_renderer.cmake src/structs.h src/config.c Ds1edit.ini.sample src/main.c CMakeLists.txt
git commit -m "Add renderer = software | gpu | auto and say which device the display bound"
```

---

### Task 8: Share the view geometry (move, no behaviour change)

The spec has `map_sw` "reuse `wpreview_get_visible_tile_bounds` and `wpreview_sort_order_data` so draw order and depth sorting are identical to the GPU path". Both are `static` in `preview.c`, which also holds Allegro drawing, so this task moves them, unchanged, into an Allegro-free file both paths call. The concurrent GPU-performance work edits `preview.c`; this is a pure cut-and-paste of five functions it is unlikely to touch.

**Files:**
- Create: `src/render/view_geom.h`, `src/render/view_geom.c`, `test/test_render_view_geom.c`
- Modify: `src/render/preview.c` (delete the moved bodies), `src/render/preview.h` (declarations move)
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: `ORDER_DATA_S`, `glb_ds1`, `glb_ds1edit` (`src/structs.h`).
- Produces (`src/render/view_geom.h`, same names and signatures the GPU path already uses):
  ```c
  int  wpreview_light_dist(int x0, int y0, int mx, int my);
  void wpreview_sort_order_data(ORDER_DATA_S *order_data, int count);
  void wpreview_get_visible_tile_bounds(int ds1_idx, int *min_x, int *max_x, int *min_y, int *max_y);
  void coord_to_tile(int ds1_idx, int ax, int ay, int *layer_x, int *layer_y);
  ```

- [ ] **Step 1: Write the characterization test**

Create `test/test_render_view_geom.c`:

```c
/* The view geometry shared by the GPU and software paths (view_geom.c). These
 * pin the behaviour the move out of preview.c must not change: stable layer
 * ordering, the light distance, tile picking, and clamped visible bounds. */

#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "render/view_geom.h"

static DS1_S s_ds1[1];
DS1_S *glb_ds1 = s_ds1;

void setUp(void)
{
   memset(s_ds1, 0, sizeof(s_ds1));
   s_ds1[0].width = 10;
   s_ds1[0].height = 10;
   s_ds1[0].tile_w = 160;
   s_ds1[0].tile_h = 80;
   glb_ds1edit.mode = MOD_T;
   glb_ds1edit.win_preview.x0 = -800;
   glb_ds1edit.win_preview.y0 = -100;
   glb_ds1edit.win_preview.w = 1600;
   glb_ds1edit.win_preview.h = 1000;
}

void tearDown(void) {}

void test_sort_is_by_height_then_original_index(void)
{
   ORDER_DATA_S o[4] = { { 0, 5 }, { 1, 1 }, { 2, 5 }, { 3, 0 } };
   wpreview_sort_order_data(o, 4);
   TEST_ASSERT_EQUAL_INT(3, o[0].idx);
   TEST_ASSERT_EQUAL_INT(1, o[1].idx);
   TEST_ASSERT_EQUAL_INT(0, o[2].idx);
   TEST_ASSERT_EQUAL_INT(2, o[3].idx);
}

void test_light_distance_doubles_the_vertical_axis(void)
{
   TEST_ASSERT_EQUAL_INT(5, wpreview_light_dist(0, 0, 3, 2));    /* sqrt(9 + 16) */
   TEST_ASSERT_EQUAL_INT(0, wpreview_light_dist(7, 7, 7, 7));
}

void test_coord_to_tile_picks_the_diamond(void)
{
   int cx, cy;
   coord_to_tile(0, 80, 40, &cx, &cy);       /* centre of tile (0,0) */
   TEST_ASSERT_EQUAL_INT(0, cx);
   TEST_ASSERT_EQUAL_INT(0, cy);
   coord_to_tile(0, 160, 80, &cx, &cy);      /* centre of tile (1,0) */
   TEST_ASSERT_EQUAL_INT(1, cx);
   TEST_ASSERT_EQUAL_INT(0, cy);
}

void test_visible_bounds_are_clamped_to_the_map(void)
{
   int min_x, max_x, min_y, max_y;
   wpreview_get_visible_tile_bounds(0, &min_x, &max_x, &min_y, &max_y);
   TEST_ASSERT_EQUAL_INT(0, min_x);
   TEST_ASSERT_EQUAL_INT(9, max_x);
   TEST_ASSERT_EQUAL_INT(0, min_y);
   TEST_ASSERT_EQUAL_INT(9, max_y);
}

void test_visible_bounds_contain_the_tile_under_the_view_centre(void)
{
   int min_x, max_x, min_y, max_y, cx, cy;

   s_ds1[0].width = 200;
   s_ds1[0].height = 200;
   glb_ds1edit.win_preview.x0 = 0;
   glb_ds1edit.win_preview.y0 = 4000;
   glb_ds1edit.win_preview.w = 640;
   glb_ds1edit.win_preview.h = 480;
   wpreview_get_visible_tile_bounds(0, &min_x, &max_x, &min_y, &max_y);
   coord_to_tile(0, 320, 4240, &cx, &cy);
   TEST_ASSERT_TRUE(min_x <= cx && cx <= max_x);
   TEST_ASSERT_TRUE(min_y <= cy && cy <= max_y);
   TEST_ASSERT_TRUE(max_x - min_x < 40);
   TEST_ASSERT_TRUE(min_x > 0);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_sort_is_by_height_then_original_index);
   RUN_TEST(test_light_distance_doubles_the_vertical_axis);
   RUN_TEST(test_coord_to_tile_picks_the_diamond);
   RUN_TEST(test_visible_bounds_are_clamped_to_the_map);
   RUN_TEST(test_visible_bounds_contain_the_tile_under_the_view_centre);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 8: shared view geometry --------------------------------------------
ds1edit_visual_test(test_render_view_geom ALLEGRO SOURCES test_render_view_geom.c
    ${_sw}/view_geom.c ${_vsup} ${_vglobals})
ds1edit_sw_add_test(render_view_geom test_render_view_geom)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/view_geom.c`.

- [ ] **Step 3: Move the functions**

Create `src/render/view_geom.h`:

```c
/* View geometry shared by the GPU path (preview.c) and the software path
 * (map_sw.c): moved out of preview.c unchanged so both draw the same tiles in
 * the same order. Reads glb_ds1 / glb_ds1edit; makes no Allegro calls. */

#ifndef DS1EDIT_RENDER_VIEW_GEOM_H
#define DS1EDIT_RENDER_VIEW_GEOM_H

#include "structs.h"

int  wpreview_light_dist(int x0, int y0, int mx, int my);
void wpreview_sort_order_data(ORDER_DATA_S *order_data, int count);
void wpreview_get_visible_tile_bounds(int ds1_idx, int *min_x, int *max_x,
                                      int *min_y, int *max_y);
void coord_to_tile(int ds1_idx, int ax, int ay, int *layer_x, int *layer_y);

#endif
```

Create `src/render/view_geom.c` (the five functions are the current text of `preview.c`, with `static` removed from `wpreview_sort_order_data` and `wpreview_get_visible_tile_bounds`; re-copy from `preview.c` at execution time if the concurrent work changed any of them, and note it):

```c
/* Derived from win_ds1edit by Paul Siramy (originally wPreview.c).
 * See NOTICE at the repository root for attribution and license status.
 *
 * See view_geom.h. Moved out of preview.c without change. */

#include <math.h>

#include "structs.h"
#include "render/view_geom.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)   /* ceil() returns double: as in preview.c */
#endif

// ==========================================================================
// compute the distance between 2 points
int wpreview_light_dist(int x0, int y0, int mx, int my)
{
   double dx, dy;

   dx = mx - x0;
   dx *= dx;

   dy = (my - y0) * 2;
   dy *= dy;

   return ceil(sqrt(dx + dy));
}

// ==========================================================================
// The draw passes sort at most 4 layers per tile; a tiny insertion sort is
// much cheaper here than calling qsort thousands of times per frame.
void wpreview_sort_order_data(ORDER_DATA_S *order_data, int count)
{
   int i, j;
   ORDER_DATA_S key;

   for (i = 1; i < count; i++)
   {
      key = order_data[i];
      j = i - 1;
      while (j >= 0)
      {
         if (order_data[j].height < key.height)
            break;
         if ((order_data[j].height == key.height) &&
             (order_data[j].idx <= key.idx))
            break;
         order_data[j + 1] = order_data[j];
         j--;
      }
      order_data[j + 1] = key;
   }
}

// ==========================================================================
// Convert absolute pixel coordinates to major tile coordinates without the
// mode-specific 5x scaling used for object/path sub-cells.
static void wpreview_coord_to_major_tile(int ds1_idx, int ax, int ay,
                                         int *layer_x, int *layer_y)
{
   int bx, by, cx, cy, rx, ry;

   if (ax < 0)
      ax -= glb_ds1[ds1_idx].tile_w;
   if (ay < 0)
      ay -= glb_ds1[ds1_idx].tile_h;

   bx = ax / glb_ds1[ds1_idx].tile_w;
   by = ay / glb_ds1[ds1_idx].tile_h;
   cx = bx + by;
   cy = -bx + by;

   rx = ax % glb_ds1[ds1_idx].tile_w;
   ry = ay % glb_ds1[ds1_idx].tile_h;
   if (ax < 0)
      rx = glb_ds1[ds1_idx].tile_w + rx;
   if (ay < 0)
      ry = glb_ds1[ds1_idx].tile_h + ry;

   if (ry >= -rx / 2 + glb_ds1[ds1_idx].tile_w / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cx++;
   else if (ry < -rx / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cx--;
   else if (glb_ds1[ds1_idx].tile_w / 2 - ry >=
            -rx / 2 + glb_ds1[ds1_idx].tile_w / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cy--;
   else if (glb_ds1[ds1_idx].tile_w / 2 - ry < -rx / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cy++;

   *layer_x = cx;
   *layer_y = cy;
}

// ==========================================================================
// Estimate the major-tile bounds that can contribute to the current view.
void wpreview_get_visible_tile_bounds(int ds1_idx,
                                             int *min_x, int *max_x,
                                             int *min_y, int *max_y)
{
   int px, py, pw, ph;
   int corners[4][2];
   int i, cx, cy;
   int x_pad = 2;
   int y_pad = 4;

   px = glb_ds1edit.win_preview.x0;
   py = glb_ds1edit.win_preview.y0;
   pw = glb_ds1edit.win_preview.w;
   ph = glb_ds1edit.win_preview.h;

   corners[0][0] = px - glb_ds1[ds1_idx].tile_w;
   corners[0][1] = py - glb_ds1[ds1_idx].tile_h * y_pad;
   corners[1][0] = px + pw + glb_ds1[ds1_idx].tile_w;
   corners[1][1] = py - glb_ds1[ds1_idx].tile_h * y_pad;
   corners[2][0] = px - glb_ds1[ds1_idx].tile_w;
   corners[2][1] = py + ph + glb_ds1[ds1_idx].tile_h * y_pad;
   corners[3][0] = px + pw + glb_ds1[ds1_idx].tile_w;
   corners[3][1] = py + ph + glb_ds1[ds1_idx].tile_h * y_pad;

   *min_x = glb_ds1[ds1_idx].width - 1;
   *max_x = 0;
   *min_y = glb_ds1[ds1_idx].height - 1;
   *max_y = 0;

   for (i = 0; i < 4; i++)
   {
      wpreview_coord_to_major_tile(ds1_idx, corners[i][0], corners[i][1], &cx, &cy);
      if (cx < *min_x)
         *min_x = cx;
      if (cx > *max_x)
         *max_x = cx;
      if (cy < *min_y)
         *min_y = cy;
      if (cy > *max_y)
         *max_y = cy;
   }

   *min_x -= x_pad;
   *max_x += x_pad;
   *min_y -= y_pad;
   *max_y += y_pad;

   if (*min_x < 0)
      *min_x = 0;
   if (*min_y < 0)
      *min_y = 0;
   if (*max_x >= glb_ds1[ds1_idx].width)
      *max_x = glb_ds1[ds1_idx].width - 1;
   if (*max_y >= glb_ds1[ds1_idx].height)
      *max_y = glb_ds1[ds1_idx].height - 1;
}

// ==========================================================================
// which tile (or sub-tile) is at this coordinates ?
void coord_to_tile(int ds1_idx, int ax, int ay, int *layer_x, int *layer_y)
{
   int bx, by, cx, cy, rx, ry;

   // ax & ay adjustement, depending of the current mode
   if ((glb_ds1edit.mode == MOD_O) ||
       (glb_ds1edit.mode == MOD_P) ||
       (glb_ds1edit.mode == MOD_L))
   {
      ax *= 5;
      ay *= 5;
   }

   // negative tile coordinates adjustment
   if (ax < 0)
      ax -= glb_ds1[ds1_idx].tile_w;
   if (ay < 0)
      ay -= glb_ds1[ds1_idx].tile_h;

   // search the Major tile's Coordinates
   bx = ax / glb_ds1[ds1_idx].tile_w;
   by = ay / glb_ds1[ds1_idx].tile_h;
   cx = bx + by;
   cy = -bx + by;

   // fine adjustements (Minor tile's Coordinates)
   rx = ax % glb_ds1[ds1_idx].tile_w;
   ry = ay % glb_ds1[ds1_idx].tile_h;
   if (ax < 0)
      rx = glb_ds1[ds1_idx].tile_w + rx;
   if (ay < 0)
      ry = glb_ds1[ds1_idx].tile_h + ry;

   if (ry >= -rx / 2 + glb_ds1[ds1_idx].tile_w / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cx++;
   else if (ry < -rx / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cx--;
   else if (glb_ds1[ds1_idx].tile_w / 2 - ry >=
            -rx / 2 + glb_ds1[ds1_idx].tile_w / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cy--;
   else if (glb_ds1[ds1_idx].tile_w / 2 - ry < -rx / 2 + glb_ds1[ds1_idx].tile_h / 2)
      cy++;

   // end
   *layer_x = cx;
   *layer_y = cy;
}
```

Delete those five definitions from `src/render/preview.c`: `wpreview_light_dist` (under `// compute the distance between 2 points`), `wpreview_sort_order_data`, `wpreview_coord_to_major_tile`, `wpreview_get_visible_tile_bounds` and `coord_to_tile`, each with its `// ====` banner comment. Keep `qsort_helper_order_data` in `preview.c`.

In `src/render/preview.h`, delete the two lines

```c
int  wpreview_light_dist                (int x0, int y0, int mx, int my);
void coord_to_tile                      (int ds1_idx, int ax, int ay, int * layer_x, int * layer_y);
```

and add after `#define _WPREVIEW_H_`:

```c
#include "render/view_geom.h"
```

Add `src/render/view_geom.c` to `SOURCES` in `CMakeLists.txt` after `src/render/display_device.c`.

- [ ] **Step 4: Run the test, build the editor, run the golden check**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_view_geom ds1edit
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_view_geom$" --output-on-failure
```
Expected: `5 Tests 0 Failures 0 Ignored`; `ds1edit` links (no duplicate or missing `coord_to_tile`).

Game-data check that the GPU path is unchanged (local lane; `bin/` must hold a `Ds1edit.ini` pointing at the vanilla install `F:\D2VersionChanger\VersionChanger\LoD\1.13c` with `full_screen = NO`):

```bash
python scripts/run_golden_tests.py --core
```
Expected: the same per-map result as before this task (the goldens are pending regeneration per the spec; this compares against whatever `test/golden` holds now, so record the before/after summaries and confirm they are identical rather than expecting all-pass).

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/view_geom.h src/render/view_geom.c src/render/preview.c src/render/preview.h test/test_render_view_geom.c test/cmake/software_renderer.cmake CMakeLists.txt
git commit -m "Move the view geometry both renderers share out of preview.c, unchanged"
```

---

### Task 9: The software map compositor — tiles, lighting, markers, grid, full-map bounds

**Files:**
- Create: `src/render/map_sw.h`, `src/render/map_sw.c`, `test/test_render_map_sw.c`
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: canvas API (Tasks 1, 3); `spritecache_get`, `spritecache_placeholder`, `spritecache_log_failure_once`, `sc_zoom_ratio`, `SC_ZOOM_*` (Tasks 2, 4); `SW_TABLES`, `sw_tables_build`, `SW_PL2_OFF_*` (Task 5); `wpreview_light_dist`, `wpreview_sort_order_data`, `wpreview_get_visible_tile_bounds`, `coord_to_tile` (Task 8); `palette_find_closest`; editor data in `structs.h` (`glb_ds1`, `glb_dt1`, `glb_ds1edit`, `glb_config`, `CACHED_TILE`, `BLOCK_TABLE_S`, `BLOCK_S`, `CELL_*`).
- Produces (`src/render/map_sw.h`):
  ```c
  typedef struct MAP_SW_TEXT { int32_t x, y; uint8_t color; char text[48]; } MAP_SW_TEXT;
  typedef struct MAP_SW_CONTEXT {
     const SW_TABLES *tables;      /* blend + darkness tables of the active act palette */
     const uint8_t   *trans;       /* glb_ds1edit.cmap[CM_TRANS][pal], flat [src*256+dst] */
     uint8_t          placeholder; /* nearest index to magenta in the active palette */
     int32_t          light_x, light_y;  /* MOD_L light source in map pixels */
  } MAP_SW_CONTEXT;
  int  map_sw_prepare_context(MAP_SW_CONTEXT *ctx, int pal_idx, int32_t light_x, int32_t light_y); /* 1 ok */
  DS1_CANVAS       *map_sw_frame_canvas(int32_t w, int32_t h, int with_tags);  /* reused between frames */
  const DS1_CANVAS *map_sw_last_canvas(void);
  void map_sw_draw_view(DS1_CANVAS *cv, int ds1_idx, const MAP_SW_CONTEXT *ctx, int cx, int cy);
  int  map_sw_full_bounds(int ds1_idx, int32_t *minx, int32_t *miny, int32_t *maxx, int32_t *maxy);  /* 1 when non-empty */
  int  map_sw_text_count(void);
  const MAP_SW_TEXT *map_sw_text_at(int i);
  ```
  `map_sw_draw_view` draws passes 1-6 of `wpreview_draw_tiles()` for the view in `glb_ds1edit.win_preview` (canvas pixel (0,0) = map pixel (x0, y0)); `(cx, cy)` is the tile under the mouse (`MOD_T` selection), `-1, -1` for none. It clears the canvas to index 0 and the label queue first. Draw-operation mapping (spec table): `wpreview_draw_bitmap` -> `canvas_blit_rle`; `wpreview_draw_trans_bitmap` / `wpreview_shape` -> `canvas_blit_rle_cmap(ctx->trans)` (the GPU path's selection tint is a 50 % alpha of the sprite, `a5_trans_alpha`, so the 50 % colormap is the faithful match); `wpreview_draw_gouraud_bitmap` -> `canvas_blit_rle_lit(tables->dark[level])` at the cell's light level; `wpreview_draw_line` -> `canvas_line`; `wpreview_textprintf` -> the label queue.

The per-cell functions below are line-for-line ports of `wpreview_draw_f / _w / _s / _r / _sp` and `wpreview_draw_tile_grid` in `preview.c`; when in doubt, that file is the specification. The GPU culling tests that use `al_get_bitmap_height()` are kept with the RLE copy's height (culling only skips work; the canvas clips anyway).

- [ ] **Step 1: Write the failing test**

Create `test/test_render_map_sw.c`:

```c
/* Software map compositor, tile layers (map_sw.c).
 *
 * A one-cell map with authored blocks: every block is a small solid sprite
 * whose index says which block it is, and the colour tables are filled with
 * marker values, so each expected pixel names the layer, pass and table that
 * produced it -- game-faithful index expectations, written by hand from the
 * pass order in preview.c's wpreview_draw_tiles(). */

#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "render/canvas.h"
#include "render/map_sw.h"
#include "render/spritecache.h"
#include "render/sw_tables.h"

static DS1_S s_ds1[1];
DS1_S *glb_ds1 = s_ds1;

#define CW 200
#define CH 120
#define TRANS_MARK 99            /* every CM_TRANS entry */
#define DARK_BASE  100           /* dark[L][*] = DARK_BASE + L */

static BLOCK_TABLE_S s_bt[8];
static BLOCK_S       s_bh[8];
static CACHED_TILE  *s_tiles[8];
static CELL_F_S      s_floor[1];
static CELL_W_S      s_wall[2];
static CELL_S_S      s_shadow[1];
static uint8_t      *s_pl2;
static DS1_CANVAS    s_cv;
static MAP_SW_CONTEXT s_ctx;

static CACHED_TILE *solid_tile(int w, int h, uint8_t v)
{
   CACHED_TILE *t = (CACHED_TILE *) calloc(1, sizeof(*t));
   t->width = w;
   t->height = h;
   t->indices = (uint8_t *) malloc((size_t) w * h);
   memset(t->indices, v, (size_t) w * h);
   return t;
}

/* block b: type, 1:1 sprite w x h of index v, zero_line */
static void block(int b, BLK_TYP_E type, int w, int h, uint8_t v, int zero_line)
{
   s_bt[b].type = type;
   s_bt[b].dt1_idx = 1;
   s_bt[b].block_idx = b;
   s_bt[b].zero_line = zero_line;
   s_bt[b].corner_pair_idx = -1;
   s_bh[b].size_x = w;
   s_bh[b].size_y = -h;
   s_bh[b].tiles_number = 1;
   s_tiles[b] = solid_tile(w, h, v);
}

static void free_tiles(void)
{
   int b;
   for (b = 0; b < 8; b++)
      if (s_tiles[b] != NULL)
      {
         free(s_tiles[b]->indices);
         free(s_tiles[b]);
         s_tiles[b] = NULL;
      }
}

void setUp(void)
{
   long off;
   int L;

   spritecache_clear();
   memset(s_ds1, 0, sizeof(s_ds1));
   memset(s_bt, 0, sizeof(s_bt));
   memset(s_bh, 0, sizeof(s_bh));
   memset(s_floor, 0, sizeof(s_floor));
   memset(s_wall, 0, sizeof(s_wall));
   memset(s_shadow, 0, sizeof(s_shadow));

   block(1, BT_STATIC, 4, 2, 10, 0);       /* floor               */
   block(2, BT_WALL_DOWN, 4, 2, 20, 80);   /* lower wall at my     */
   block(3, BT_SHADOW, 4, 2, 30, 80);      /* shadow at my         */
   block(4, BT_WALL_UP, 2, 1, 40, 80);     /* upper wall at my     */
   block(5, BT_ROOF, 1, 1, 50, 0);         /* roof                 */
   block(6, BT_SPECIAL, 1, 1, 60, 80);     /* special (orient. 10) */

   memset(&glb_dt1[1], 0, sizeof(glb_dt1[1]));
   strcpy(glb_dt1[1].name, "authored.dt1");
   glb_dt1[1].block_num = 8;
   glb_dt1[1].bh_buffer = s_bh;
   glb_dt1[1].block_cache[ZM_11] = s_tiles;

   s_ds1[0].width = 1;
   s_ds1[0].height = 1;
   s_ds1[0].tile_w = 160;
   s_ds1[0].tile_h = 80;
   s_ds1[0].height_mul = 1;
   s_ds1[0].height_div = 1;
   s_ds1[0].cur_zoom = ZM_11;
   s_ds1[0].block_table = s_bt;
   s_ds1[0].bt_num = 8;
   s_ds1[0].floor_buff = s_floor;
   s_ds1[0].floor_num = 1;
   s_ds1[0].floor_line = 1;
   s_ds1[0].wall_buff = s_wall;
   s_ds1[0].wall_num = 2;
   s_ds1[0].wall_line = 2;
   s_ds1[0].shadow_buff = s_shadow;
   s_ds1[0].shadow_num = 1;
   s_ds1[0].shadow_line = 1;
   s_ds1[0].floor_layer_mask[0] = 1;
   s_ds1[0].wall_layer_mask[0] = 1;
   s_ds1[0].wall_layer_mask[1] = 1;
   s_ds1[0].shadow_layer_mask[0] = 3;      /* the editor's default: transparent */

   glb_ds1edit.mode = MOD_T;
   glb_ds1edit.display_tile_grid = TG_OFF;
   glb_ds1edit.win_preview.x0 = -10;
   glb_ds1edit.win_preview.y0 = -10;
   glb_ds1edit.win_preview.w = CW;
   glb_ds1edit.win_preview.h = CH;
   glb_config.screen.width = CW;
   glb_config.screen.height = CH;

   /* A full-size PL2 whose tables are marker values. */
   s_pl2 = (uint8_t *) calloc(SW_PL2_FULL_SIZE, 1);
   for (L = 1; L <= 32; L++)
      memset(s_pl2 + SW_PL2_OFF_DARK + (L - 1) * 256, DARK_BASE + L, 256);
   for (off = 0; off < 65536; off++)
   {
      s_pl2[SW_PL2_OFF_ALPHA25 + off] = 0xA1;
      s_pl2[SW_PL2_OFF_ALPHA50 + off] = 0xA2;
      s_pl2[SW_PL2_OFF_ALPHA75 + off] = 0xA3;
      s_pl2[SW_PL2_OFF_ADD + off] = 0xA4;
      s_pl2[SW_PL2_OFF_MUL + off] = 0xA5;
      s_pl2[SW_PL2_OFF_SCREEN + off] = 0xA6;
   }
   glb_ds1edit.d2_pal[0] = s_pl2;
   glb_ds1edit.pal_size[0] = SW_PL2_FULL_SIZE;
   memset(&glb_ds1edit.cmap[CM_TRANS][0], TRANS_MARK, sizeof(INDEX_COLORMAP));
   memset(&glb_ds1edit.vga_pal[0], 0, sizeof(RGBA_PALETTE));

   TEST_ASSERT_EQUAL_INT(1, map_sw_prepare_context(&s_ctx, 0, 100000, 100000));
   TEST_ASSERT_EQUAL_INT(1, canvas_create(&s_cv, CW, CH, 1));
}

void tearDown(void)
{
   canvas_destroy(&s_cv);
   spritecache_clear();
   free_tiles();
   glb_dt1[1].block_cache[ZM_11] = NULL;
   glb_dt1[1].bh_buffer = NULL;
   glb_ds1edit.d2_pal[0] = NULL;
   free(s_pl2);
}

static uint8_t px(int x, int y) { return s_cv.pixels[y * s_cv.stride + x]; }
static uint8_t tg(int x, int y) { return s_cv.tags[y * s_cv.stride + x]; }

static void draw(void) { map_sw_draw_view(&s_cv, 0, &s_ctx, -1, -1); }

/* Cell (0,0) has map anchor (0,0), i.e. canvas (10,10). */

void test_floor_draws_at_the_cell_anchor(void)
{
   s_floor[0].bt_idx = 1;
   draw();
   TEST_ASSERT_EQUAL_HEX8(10, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(10, px(13, 11));
   TEST_ASSERT_EQUAL_HEX8(0, px(14, 10));
   TEST_ASSERT_EQUAL_HEX8(0, px(9, 10));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_EXACT, tg(10, 10));
}

void test_floor_is_drawn_after_the_lower_wall_in_pass_one(void)
{
   s_floor[0].bt_idx = 1;
   s_wall[0].bt_idx = 2;
   s_wall[0].orientation = 16;            /* lower wall */
   draw();
   TEST_ASSERT_EQUAL_HEX8(10, px(10, 10));
   s_floor[0].flags = FLG_HIDE;
   draw();
   TEST_ASSERT_EQUAL_HEX8(20, px(10, 10));
}

void test_shadow_layer_modes(void)
{
   s_floor[0].bt_idx = 1;
   s_shadow[0].bt_idx = 3;
   draw();                                       /* mode 3: 50% colormap */
   TEST_ASSERT_EQUAL_HEX8(TRANS_MARK, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_TABLE, tg(10, 10));
   s_ds1[0].shadow_layer_mask[0] = 1;            /* normal sprite */
   draw();
   TEST_ASSERT_EQUAL_HEX8(30, px(10, 10));
   s_ds1[0].shadow_layer_mask[0] = 2;            /* "white" shape: the GPU path's 50% tint */
   draw();
   TEST_ASSERT_EQUAL_HEX8(TRANS_MARK, px(10, 10));
   s_ds1[0].shadow_layer_mask[0] = 0;            /* off */
   draw();
   TEST_ASSERT_EQUAL_HEX8(10, px(10, 10));
}

void test_upper_wall_draws_over_the_floor(void)
{
   s_floor[0].bt_idx = 1;
   s_wall[1].bt_idx = 4;
   s_wall[1].orientation = 1;            /* upper wall */
   draw();
   TEST_ASSERT_EQUAL_HEX8(40, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(10, px(12, 10));
}

void test_roof_uses_its_roof_height_and_draws_last(void)
{
   s_wall[0].bt_idx = 5;
   s_wall[0].orientation = 15;
   s_bt[5].roof_y = 3;
   draw();
   TEST_ASSERT_EQUAL_HEX8(50, px(10, 7));
   TEST_ASSERT_EQUAL_HEX8(0, px(10, 10));
}

void test_selected_or_flagged_cells_are_tinted(void)
{
   s_floor[0].bt_idx = 1;
   map_sw_draw_view(&s_cv, 0, &s_ctx, 0, 0);     /* tile under the mouse */
   TEST_ASSERT_EQUAL_HEX8(TRANS_MARK, px(10, 10));
   s_floor[0].flags = FLG_SELECTED;
   draw();
   TEST_ASSERT_EQUAL_HEX8(TRANS_MARK, px(10, 10));
}

void test_layer_mask_hides_a_floor(void)
{
   s_floor[0].bt_idx = 1;
   s_ds1[0].floor_layer_mask[0] = 0;
   draw();
   TEST_ASSERT_EQUAL_HEX8(0, px(10, 10));
}

void test_unknown_tile_draws_a_marker_and_queues_its_label(void)
{
   const MAP_SW_TEXT *t;

   s_floor[0].bt_idx = 1;
   s_floor[0].prop4 = 0x80;              /* hidden: shown as "unknown" with F9 on */
   s_ds1[0].special_layer_mask = 1;
   draw();
   TEST_ASSERT_EQUAL_HEX8(168, px(10, 49));     /* (ux1, uy2) */
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_LINE, tg(10, 49));
   TEST_ASSERT_EQUAL_INT(2, map_sw_text_count());
   t = map_sw_text_at(0);
   TEST_ASSERT_EQUAL_STRING("0 0 0 128", t->text);
   TEST_ASSERT_EQUAL_INT32(18, t->x);
   TEST_ASSERT_EQUAL_INT32(47, t->y);
   TEST_ASSERT_EQUAL_HEX8(0, t->color);
   TEST_ASSERT_EQUAL_HEX8(255, map_sw_text_at(1)->color);
   TEST_ASSERT_NULL(map_sw_text_at(2));
}

void test_zoom_one_half_uses_the_half_size_copy(void)
{
   s_floor[0].bt_idx = 1;
   s_ds1[0].cur_zoom = ZM_12;
   s_ds1[0].height_div = 2;
   s_ds1[0].tile_w = 80;
   s_ds1[0].tile_h = 40;
   draw();
   TEST_ASSERT_EQUAL_HEX8(10, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(10, px(11, 10));
   TEST_ASSERT_EQUAL_HEX8(0, px(12, 10));
   TEST_ASSERT_EQUAL_HEX8(0, px(10, 11));
}

void test_light_mode_uses_the_cell_light_level(void)
{
   s_floor[0].bt_idx = 1;
   glb_ds1edit.mode = MOD_L;
   draw();                                         /* light far away: level 0, skipped */
   TEST_ASSERT_EQUAL_HEX8(0, px(10, 10));
   TEST_ASSERT_EQUAL_INT(1, map_sw_prepare_context(&s_ctx, 0, 80, 40));
   draw();                                         /* light on the cell centre: full */
   TEST_ASSERT_EQUAL_HEX8(10, px(10, 10));
   TEST_ASSERT_EQUAL_INT(1, map_sw_prepare_context(&s_ctx, 0, 80 + 350, 40));
   draw();                                         /* distance 350: c = 128, level 16 */
   TEST_ASSERT_EQUAL_HEX8(DARK_BASE + 16, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_TABLE, tg(10, 10));
}

void test_full_bounds_cover_every_sprite(void)
{
   int32_t minx, miny, maxx, maxy;

   s_floor[0].bt_idx = 1;
   s_wall[0].bt_idx = 2;
   s_wall[0].orientation = 16;
   TEST_ASSERT_EQUAL_INT(1, map_sw_full_bounds(0, &minx, &miny, &maxx, &maxy));
   TEST_ASSERT_EQUAL_INT32(0, minx);
   TEST_ASSERT_EQUAL_INT32(0, miny);
   TEST_ASSERT_EQUAL_INT32(4, maxx);
   TEST_ASSERT_EQUAL_INT32(2, maxy);
   s_floor[0].bt_idx = 0;
   s_wall[0].bt_idx = 0;
   TEST_ASSERT_EQUAL_INT(0, map_sw_full_bounds(0, &minx, &miny, &maxx, &maxy));
}

void test_missing_tile_data_draws_a_placeholder_and_logs_once(void)
{
   SPRITECACHE_STATS st;
   CACHED_TILE *keep = s_tiles[1];

   s_floor[0].bt_idx = 1;
   s_tiles[1] = NULL;                       /* header says pixels, no index data */
   draw();
   draw();
   TEST_ASSERT_EQUAL_HEX8(s_ctx.placeholder, px(10, 10));
   TEST_ASSERT_EQUAL_HEX8(s_ctx.placeholder, px(10 + 159, 10 + 79));
   spritecache_stats(&st);
   TEST_ASSERT_EQUAL_UINT32(1, st.failures_logged);
   s_tiles[1] = keep;
}

void test_an_empty_block_draws_nothing(void)
{
   CACHED_TILE *keep = s_tiles[1];

   s_floor[0].bt_idx = 1;
   s_tiles[1] = NULL;
   s_bh[1].size_x = 0;                      /* the decoder skipped it as empty */
   draw();
   TEST_ASSERT_EQUAL_HEX8(0, px(10, 10));
   s_tiles[1] = keep;
}

void test_tile_grid_over_floor_is_drawn_with_lines(void)
{
   glb_ds1edit.display_tile_grid = TG_OVERFLOOR;
   draw();
   TEST_ASSERT_EQUAL_HEX8(255, px(89, 9));        /* first grid line starts at (base_x, base_y) */
   TEST_ASSERT_EQUAL_HEX8(CANVAS_TAG_LINE, tg(89, 9));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_floor_draws_at_the_cell_anchor);
   RUN_TEST(test_floor_is_drawn_after_the_lower_wall_in_pass_one);
   RUN_TEST(test_shadow_layer_modes);
   RUN_TEST(test_upper_wall_draws_over_the_floor);
   RUN_TEST(test_roof_uses_its_roof_height_and_draws_last);
   RUN_TEST(test_selected_or_flagged_cells_are_tinted);
   RUN_TEST(test_layer_mask_hides_a_floor);
   RUN_TEST(test_unknown_tile_draws_a_marker_and_queues_its_label);
   RUN_TEST(test_zoom_one_half_uses_the_half_size_copy);
   RUN_TEST(test_light_mode_uses_the_cell_light_level);
   RUN_TEST(test_full_bounds_cover_every_sprite);
   RUN_TEST(test_missing_tile_data_draws_a_placeholder_and_logs_once);
   RUN_TEST(test_an_empty_block_draws_nothing);
   RUN_TEST(test_tile_grid_over_floor_is_drawn_with_lines);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Tasks 9-10: the map compositor ------------------------------------------
set(_map_sw_sources ${_sw}/map_sw.c ${_sw}/view_geom.c ${_sw}/canvas.c
    ${_sw}/spritecache.c ${_sw}/sw_tables.c ${_core}/palette.c ${_core}/dt1_decode.c
    ${_vsup} ${_vglobals})
ds1edit_visual_test(test_render_map_sw ALLEGRO SOURCES test_render_map_sw.c ${_map_sw_sources})
ds1edit_sw_add_test(render_map_sw test_render_map_sw)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/map_sw.c`.

- [ ] **Step 3: Write `map_sw.h`**

```c
/* Software map compositor: draws one view of a DS1 into an 8-bit canvas.
 *
 * Passes, visible-tile bounds, per-cell layer order and editing state
 * (selection, hidden layers, layer masks, F9 markers) are the GPU path's --
 * preview.c's wpreview_draw_tiles() passes 1-6 -- read from the same cell
 * data. Sprites come from the sprite cache; blends go through the D2CMP
 * tables. No Allegro calls: labels are queued (map_sw_text_*) for the caller
 * to draw, which is how phase 1 layers the existing Allegro UI on top. */

#ifndef DS1EDIT_RENDER_MAP_SW_H
#define DS1EDIT_RENDER_MAP_SW_H

#include <stdint.h>

#include "render/canvas.h"
#include "render/sw_tables.h"

typedef struct MAP_SW_TEXT
{
   int32_t x, y;
   uint8_t color;
   char    text[48];
} MAP_SW_TEXT;

typedef struct MAP_SW_CONTEXT
{
   const SW_TABLES *tables;          /* blend + darkness tables, active act    */
   const uint8_t   *trans;           /* CM_TRANS colormap, flat [src*256+dst]  */
   uint8_t          placeholder;     /* nearest index to magenta               */
   int32_t          light_x, light_y;/* MOD_L light source, map pixels         */
} MAP_SW_CONTEXT;

int  map_sw_prepare_context(MAP_SW_CONTEXT *ctx, int pal_idx,
                            int32_t light_x, int32_t light_y);

DS1_CANVAS       *map_sw_frame_canvas(int32_t w, int32_t h, int with_tags);
const DS1_CANVAS *map_sw_last_canvas(void);

void map_sw_draw_view(DS1_CANVAS *cv, int ds1_idx, const MAP_SW_CONTEXT *ctx,
                      int cx, int cy);

int  map_sw_full_bounds(int ds1_idx, int32_t *minx, int32_t *miny,
                        int32_t *maxx, int32_t *maxy);

int                map_sw_text_count(void);
const MAP_SW_TEXT *map_sw_text_at(int i);

#endif
```

- [ ] **Step 4: Write `map_sw.c`**

```c
/* See map_sw.h. The per-cell functions are ports of wpreview_draw_f / _w /
 * _s / _r / _sp and wpreview_draw_tile_grid (preview.c); keep them in step. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "core/palette.h"
#include "render/canvas.h"
#include "render/map_sw.h"
#include "render/spritecache.h"
#include "render/sw_tables.h"
#include "render/view_geom.h"

/* The sprite cache's zoom codes are ZOOM_E's values, and sw_tables' PL2
   offsets are structs.h's COF_* colormap numbers times 256. */
typedef char map_sw_zoom_codes_match[(SC_ZOOM_21 == ZM_21 && SC_ZOOM_11 == ZM_11 &&
                                      SC_ZOOM_12 == ZM_12 && SC_ZOOM_14 == ZM_14 &&
                                      SC_ZOOM_18 == ZM_18 && SC_ZOOM_116 == ZM_116 &&
                                      SC_ZOOM_MAX == ZM_MAX) ? 1 : -1];
typedef char map_sw_cof_offsets_match[(SW_PL2_OFF_ALPHA75 == COF_25TRANS * 256L &&
                                       SW_PL2_OFF_ALPHA50 == COF_50TRANS * 256L &&
                                       SW_PL2_OFF_ALPHA25 == COF_75TRANS * 256L &&
                                       SW_PL2_OFF_ADD == COF_ALPHA * 256L &&
                                       SW_PL2_OFF_MUL == COF_LUMINANCE * 256L &&
                                       SW_PL2_OFF_SCREEN == COF_ALPHABRIGHT * 256L) ? 1 : -1];

/* ---- labels --------------------------------------------------------------- */

#define MAP_SW_TEXT_MAX 1024
static MAP_SW_TEXT s_text[MAP_SW_TEXT_MAX];
static int         s_text_count;

static void sw_text(int32_t x, int32_t y, uint8_t color, const char *text)
{
   MAP_SW_TEXT *t;

   if ((text == NULL) || (s_text_count >= MAP_SW_TEXT_MAX))
      return;
   t = &s_text[s_text_count++];
   t->x = x;
   t->y = y;
   t->color = color;
   snprintf(t->text, sizeof(t->text), "%s", text);
}

int map_sw_text_count(void)
{
   return s_text_count;
}

const MAP_SW_TEXT *map_sw_text_at(int i)
{
   return ((i >= 0) && (i < s_text_count)) ? &s_text[i] : NULL;
}

/* ---- context and frame canvas ------------------------------------------- */

static SW_TABLES   *s_tables[ACT_MAX];
static const UBYTE *s_tables_src[ACT_MAX];
static long         s_tables_len[ACT_MAX];

int map_sw_prepare_context(MAP_SW_CONTEXT *ctx, int pal_idx, int32_t light_x, int32_t light_y)
{
   if (ctx == NULL)
      return 0;
   memset(ctx, 0, sizeof(*ctx));
   if ((pal_idx < 0) || (pal_idx >= ACT_MAX))
      return 0;
   if ((s_tables[pal_idx] == NULL) ||
       (s_tables_src[pal_idx] != glb_ds1edit.d2_pal[pal_idx]) ||
       (s_tables_len[pal_idx] != glb_ds1edit.pal_size[pal_idx]))
   {
      if (s_tables[pal_idx] == NULL)
      {
         s_tables[pal_idx] = (SW_TABLES *) malloc(sizeof(SW_TABLES));
         if (s_tables[pal_idx] == NULL)
            return 0;
      }
      if (!sw_tables_build(s_tables[pal_idx], glb_ds1edit.d2_pal[pal_idx],
                           glb_ds1edit.pal_size[pal_idx]))
         return 0;
      s_tables_src[pal_idx] = glb_ds1edit.d2_pal[pal_idx];
      s_tables_len[pal_idx] = glb_ds1edit.pal_size[pal_idx];
   }
   ctx->tables      = s_tables[pal_idx];
   ctx->trans       = &glb_ds1edit.cmap[CM_TRANS][pal_idx].data[0][0];
   ctx->placeholder = palette_find_closest(&glb_ds1edit.vga_pal[pal_idx], 255, 0, 255);
   ctx->light_x     = light_x;
   ctx->light_y     = light_y;
   return 1;
}

static DS1_CANVAS s_frame;
static int        s_frame_tags;

DS1_CANVAS *map_sw_frame_canvas(int32_t w, int32_t h, int with_tags)
{
   with_tags = with_tags ? 1 : 0;
   if ((s_frame.pixels == NULL) || (s_frame.w != w) || (s_frame.h != h) ||
       (s_frame_tags != with_tags))
   {
      canvas_destroy(&s_frame);
      if (!canvas_create(&s_frame, w, h, with_tags))
         return NULL;
      s_frame_tags = with_tags;
   }
   return &s_frame;
}

const DS1_CANVAS *map_sw_last_canvas(void)
{
   return (s_frame.pixels != NULL) ? &s_frame : NULL;
}

/* ---- sprites ------------------------------------------------------------- */

static int sw_zoom(int ds1_idx)
{
   int z = (int) glb_ds1[ds1_idx].cur_zoom;
   return ((z < ZM_21) || (z >= ZM_MAX)) ? ZM_11 : z;
}

static void sw_frame_view(const CACHED_TILE *ct, DS1EDIT_INDEX_FRAME *f)
{
   memset(f, 0, sizeof(*f));
   f->width      = ct->width;
   f->height     = ct->height;
   f->stride     = ct->width;
   f->pixels     = ct->indices;
   f->pixels_len = (size_t) ct->width * (size_t) ct->height;
}

static CACHED_TILE *sw_block_tile(int dt1_idx, int block_idx)
{
   CACHED_TILE *ct;

   if ((dt1_idx < 0) || (dt1_idx >= DT1_MAX) || (glb_dt1[dt1_idx].block_cache[ZM_11] == NULL) ||
       (block_idx < 0) || (block_idx >= glb_dt1[dt1_idx].block_num))
      return NULL;
   ct = glb_dt1[dt1_idx].block_cache[ZM_11][block_idx];
   if ((ct == NULL) || (ct->indices == NULL) || (ct->width <= 0) || (ct->height <= 0))
      return NULL;
   return ct;
}

/* Does the block header promise pixels? dt1_all_zoom_make() skips blocks with
   no size (or an absurd one) and leaves them without a bitmap or index data;
   those draw nothing on either path. Anything else without data is a decode
   failure and gets the placeholder. size_y is the in-place adjusted value. */
static const BLOCK_S *sw_block_header_with_pixels(int dt1_idx, int block_idx)
{
   const BLOCK_S *b;
   long w, h;

   if ((dt1_idx < 0) || (dt1_idx >= DT1_MAX) || (glb_dt1[dt1_idx].bh_buffer == NULL) ||
       (block_idx < 0) || (block_idx >= glb_dt1[dt1_idx].block_num))
      return NULL;
   b = (const BLOCK_S *) glb_dt1[dt1_idx].bh_buffer + block_idx;
   w = ((b->orientation == 10) || (b->orientation == 11)) ? 160 : b->size_x;
   h = -b->size_y;
   if ((w <= 0) || (h <= 0) || (w > 2048) || (h > 2048) || (b->tiles_number <= 0))
      return NULL;
   return b;
}

static const DS1_RLE *sw_block_sprite(const MAP_SW_CONTEXT *ctx, int ds1_idx,
                                      int dt1_idx, int block_idx)
{
   CACHED_TILE        *ct = sw_block_tile(dt1_idx, block_idx);
   DS1EDIT_INDEX_FRAME f;
   const DS1_RLE      *r;
   const BLOCK_S      *hdr;
   int                 mul, div;
   char                what[120];

   if (ct != NULL)
   {
      sw_frame_view(ct, &f);
      r = spritecache_get(ct, &f, sw_zoom(ds1_idx));
      return r;                       /* NULL here = decimated to nothing */
   }
   hdr = sw_block_header_with_pixels(dt1_idx, block_idx);
   if (hdr == NULL)
      return NULL;
   snprintf(what, sizeof(what), "%s block %d", glb_dt1[dt1_idx].name, block_idx);
   spritecache_log_failure_once(hdr, what);
   if (!sc_zoom_ratio(sw_zoom(ds1_idx), &mul, &div))
      return NULL;
   return spritecache_placeholder(160 * mul / div, 80 * mul / div, ctx->placeholder);
}

/* ---- per-cell drawing ------------------------------------------------------ */

static int sw_tinted(UBYTE flags, int selected)
{
   return selected || IS_COPY_INFO(flags) || IS_TMP_SELECT(flags) || IS_SELECTED(flags);
}

static int sw_light_level(const MAP_SW_CONTEXT *ctx, int px, int py)
{
   int dist = wpreview_light_dist(px, py, ctx->light_x, ctx->light_y), c;

   if (dist < 200)
      c = 255;
   else if (dist > 500)
      c = 0;
   else
      c = 255 - ((dist - 200) * 255 / (500 - 200));
   return (c * 32 + 127) / 255;
}

static void sw_draw_lit(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, const DS1_RLE *spr,
                        int32_t x, int32_t y, int level, int skip_dark)
{
   if ((level <= 0) && skip_dark)
      return;
   if (level >= 32)
   {
      canvas_blit_rle(cv, spr, x, y, 0);
      return;
   }
   if (level < 0)
      level = 0;
   canvas_blit_rle_lit(cv, spr, x, y, 0, ctx->tables->dark[level]);
}

static void sw_draw_plain(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, const DS1_RLE *spr,
                          int32_t x, int32_t y, int tinted)
{
   if (tinted)
      canvas_blit_rle_cmap(cv, spr, x, y, 0, ctx->trans);
   else
      canvas_blit_rle(cv, spr, x, y, 0);
}

static int sw_off_view(const DS1_RLE *spr, int32_t y1)
{
   return ((y1 + spr->h) < 0) || (y1 >= glb_ds1edit.win_preview.h);
}

/* The "unknown tile" diamond in index 168, plus its label at 1:1. */
static void sw_marker(DS1_CANVAS *cv, int ds1_idx, int mx, int my, const char *label)
{
   const DS1_S *d = &glb_ds1[ds1_idx];
   int32_t ux1 = mx - glb_ds1edit.win_preview.x0;
   int32_t ux2 = ux1 + d->tile_w / 2 - 1;
   int32_t ux3 = ux1 + d->tile_w / 2;
   int32_t ux4 = ux1 + d->tile_w - 1;
   int32_t uy1 = my - glb_ds1edit.win_preview.y0;
   int32_t uy2 = uy1 + d->tile_h / 2 - 1;
   int32_t uy3 = uy1 + d->tile_h - 2;

   canvas_line(cv, ux1, uy2, ux2, uy1, 168);
   canvas_line(cv, ux3, uy1, ux4, uy2, 168);
   canvas_line(cv, ux3, uy3, ux4, uy2, 168);
   canvas_line(cv, ux1, uy2, ux2, uy3, 168);
   if ((d->cur_zoom == ZM_11) && (label != NULL))
   {
      sw_text(ux1 + 8, uy2 - 2, 0, label);
      sw_text(ux1 + 7, uy2 - 3, 255, label);
   }
}

static void sw_draw_s(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                      int x, int y, int mx, int my, int selected)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   CELL_S_S *s_ptr = d->shadow_buff + (y * d->shadow_line) + (x * d->shadow_num);
   BLOCK_TABLE_S *bt_ptr;
   const DS1_RLE *spr;
   int n, bt_idx;
   int32_t y1, sx;
   char label[48];

   for (n = 0; n < d->shadow_num; n++)
   {
      if (d->shadow_layer_mask[n] == 0)
         continue;
      if (IS_HIDE(s_ptr[n].flags))
         continue;
      bt_idx = s_ptr[n].bt_idx;
      if (s_ptr[n].prop4 & 0x80)
         bt_idx = -1;
      if (bt_idx == -1)
      {
         if (!d->special_layer_mask)
            continue;
         snprintf(label, sizeof(label), "%u %u %u %u", s_ptr[n].prop1, s_ptr[n].prop2,
                  s_ptr[n].prop3, s_ptr[n].prop4);
         sw_marker(cv, ds1_idx, mx, my, label);
      }
      if (bt_idx <= 0)
         continue;
      bt_ptr = d->block_table + bt_idx;
      if (bt_ptr->type != BT_SHADOW)
         continue;
      spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
      if (spr == NULL)
         continue;
      y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
      y1 += d->tile_h;
      if (sw_off_view(spr, y1))
         continue;
      sx = mx - glb_ds1edit.win_preview.x0;

      if (sw_tinted(s_ptr[n].flags, selected) && (glb_ds1edit.mode != MOD_L))
         canvas_blit_rle_cmap(cv, spr, sx, y1, 0, ctx->trans);
      else
      {
         switch (d->shadow_layer_mask[0])
         {
         case 1:  canvas_blit_rle(cv, spr, sx, y1, 0);                   break;
         case 2:  canvas_blit_rle_cmap(cv, spr, sx, y1, 0, ctx->trans);  break;
         case 3:  canvas_blit_rle_cmap(cv, spr, sx, y1, 0, ctx->trans);  break;
         default:                                                        break;
         }
      }
   }
}

static void sw_draw_f(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                      int x, int y, int mx, int my, int selected)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   CELL_F_S *f_ptr = d->floor_buff + (y * d->floor_line) + (x * d->floor_num);
   ORDER_DATA_S order_data[WALL_MAX_LAYER];
   BLOCK_TABLE_S *bt_ptr;
   const DS1_RLE *spr;
   int n, idx, bt_idx;
   int32_t y1, sx;
   char label[48];

   for (n = 0; n < d->floor_num; n++)
   {
      order_data[n].idx = n;
      order_data[n].height = f_ptr[n].prop1;
   }
   wpreview_sort_order_data(order_data, d->floor_num);

   for (n = 0; n < d->floor_num; n++)
   {
      idx = order_data[n].idx;
      if (d->floor_layer_mask[idx] == 0)
         continue;
      if (IS_HIDE(f_ptr[idx].flags))
         continue;
      bt_idx = f_ptr[idx].bt_idx;
      if (bt_idx == 0)
         continue;
      if (f_ptr[idx].prop4 & 0x80)
         bt_idx = -1;
      if (bt_idx == -1)
      {
         if (!d->special_layer_mask)
            continue;
         snprintf(label, sizeof(label), "%u %u %u %u", f_ptr[idx].prop1, f_ptr[idx].prop2,
                  f_ptr[idx].prop3, f_ptr[idx].prop4);
         sw_marker(cv, ds1_idx, mx, my, label);
      }
      if (bt_idx <= 0)
         continue;
      bt_ptr = d->block_table + bt_idx;
      if ((bt_ptr->type != BT_STATIC) && (bt_ptr->type != BT_ANIMATED))
         continue;
      if (bt_ptr->type == BT_ANIMATED)
      {
         if (bt_ptr->updated == FALSE)
         {
            bt_ptr->curr_frame = (d->cur_anim_floor_frame / 5) % (bt_ptr->rarity + 1);
            bt_ptr->updated = TRUE;
         }
         bt_ptr += bt_ptr->curr_frame - bt_ptr->rarity;
      }
      spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
      if (spr == NULL)
         continue;
      y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
      if (sw_off_view(spr, y1))
         continue;
      sx = mx - glb_ds1edit.win_preview.x0;

      if (glb_ds1edit.mode == MOD_L)
         sw_draw_lit(cv, ctx, spr, sx, y1,
                     sw_light_level(ctx, mx + d->tile_w / 2, my + d->tile_h / 2), 1);
      else
         sw_draw_plain(cv, ctx, spr, sx, y1, sw_tinted(f_ptr[idx].flags, selected));
   }
}

static void sw_draw_w(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                      int x, int y, int mx, int my, int selected, int upper)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   CELL_W_S *w_ptr = d->wall_buff + (y * d->wall_line) + (x * d->wall_num);
   ORDER_DATA_S order_data[WALL_MAX_LAYER];
   BLOCK_TABLE_S *bt_ptr;
   const DS1_RLE *spr;
   int n, idx, bt_idx, o, level = 32, tinted;
   int32_t y1, sx;
   char label[48];

   for (n = 0; n < d->wall_num; n++)
   {
      order_data[n].idx = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   wpreview_sort_order_data(order_data, d->wall_num);

   for (n = 0; n < d->wall_num; n++)
   {
      idx = order_data[n].idx;
      if (d->wall_layer_mask[idx] == 0)
         continue;
      if (IS_HIDE(w_ptr[idx].flags))
         continue;
      o = w_ptr[idx].orientation;
      if ((upper == TRUE) && (o >= 15))
         continue;
      if ((upper == FALSE) && (o <= 15))
         continue;
      bt_idx = w_ptr[idx].bt_idx;
      if (bt_idx == 0)
         continue;
      if ((o == 10) || (o == 11))
         continue;                         /* specials: pass 6 */
      if (w_ptr[idx].prop4 & 0x80)
         bt_idx = -1;
      if (bt_idx == -1)
      {
         if (!d->special_layer_mask)
            continue;
         snprintf(label, sizeof(label), "(%u) %u %u %u %u", w_ptr[idx].orientation,
                  w_ptr[idx].prop1, w_ptr[idx].prop2, w_ptr[idx].prop3, w_ptr[idx].prop4);
         sw_marker(cv, ds1_idx, mx, my, label);
         continue;
      }
      bt_ptr = d->block_table + bt_idx;
      if ((bt_ptr->type != BT_WALL_UP) && (bt_ptr->type != BT_WALL_DOWN) &&
          (bt_ptr->type != BT_SPECIAL))
         continue;
      spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
      if (spr == NULL)
         continue;
      y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
      y1 += d->tile_h;
      if (sw_off_view(spr, y1))
         continue;
      sx = mx - glb_ds1edit.win_preview.x0;

      tinted = 0;
      if (glb_ds1edit.mode == MOD_L)
      {
         level = sw_light_level(ctx, mx + d->tile_w / 2, my + d->tile_h / 2);
         sw_draw_lit(cv, ctx, spr, sx, y1, level, 0);   /* walls have no dark skip */
      }
      else
      {
         tinted = sw_tinted(w_ptr[idx].flags, selected);
         sw_draw_plain(cv, ctx, spr, sx, y1, tinted);
      }

      /* upper-left corner: draw the paired block too */
      if ((bt_ptr->orientation == 3) && (bt_ptr->corner_pair_idx >= 0))
      {
         bt_ptr = d->block_table + bt_ptr->corner_pair_idx;
         spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
         if (spr == NULL)
            continue;
         y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
         y1 += d->tile_h;
         if (sw_off_view(spr, y1))
            continue;
         if (glb_ds1edit.mode == MOD_L)
            sw_draw_lit(cv, ctx, spr, sx, y1, level, 0);
         else
            sw_draw_plain(cv, ctx, spr, sx, y1, tinted);
      }
   }
}

static void sw_draw_r(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                      int x, int y, int mx, int my, int selected)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   CELL_W_S *r_ptr = d->wall_buff + (y * d->wall_line) + (x * d->wall_num);
   ORDER_DATA_S order_data[WALL_MAX_LAYER];
   BLOCK_TABLE_S *bt_ptr;
   const DS1_RLE *spr;
   int n, idx, bt_idx;
   int32_t y1, sx;

   for (n = 0; n < d->wall_num; n++)
   {
      order_data[n].idx = n;
      order_data[n].height = r_ptr[n].prop1;
   }
   wpreview_sort_order_data(order_data, d->wall_num);

   for (n = 0; n < d->wall_num; n++)
   {
      idx = order_data[n].idx;
      if (d->wall_layer_mask[idx] == 0)
         continue;
      if (IS_HIDE(r_ptr[idx].flags))
         continue;
      bt_idx = r_ptr[idx].bt_idx;
      if (bt_idx <= 0)
         continue;
      if (r_ptr[idx].prop4 & 0x80)
         continue;
      bt_ptr = d->block_table + bt_idx;
      if (bt_ptr->type != BT_ROOF)
         continue;
      spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
      if (spr == NULL)
         continue;
      y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
      y1 -= bt_ptr->roof_y * d->height_mul / d->height_div;
      if (sw_off_view(spr, y1))
         continue;
      sx = mx - glb_ds1edit.win_preview.x0;

      if (glb_ds1edit.mode == MOD_L)
         canvas_blit_rle_cmap(cv, spr, sx, y1, 0, ctx->trans);
      else
         sw_draw_plain(cv, ctx, spr, sx, y1, sw_tinted(r_ptr[idx].flags, selected));
   }
}

static void sw_draw_sp(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                       int x, int y, int mx, int my, int selected)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   CELL_W_S *w_ptr = d->wall_buff + (y * d->wall_line) + (x * d->wall_num);
   ORDER_DATA_S order_data[WALL_MAX_LAYER];
   BLOCK_TABLE_S *bt_ptr;
   const DS1_RLE *spr;
   int n, idx, bt_idx, o;
   int32_t y1, sx;
   char label[48];

   for (n = 0; n < d->wall_num; n++)
   {
      order_data[n].idx = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   wpreview_sort_order_data(order_data, d->wall_num);

   for (n = 0; n < d->wall_num; n++)
   {
      idx = order_data[n].idx;
      if (d->wall_layer_mask[idx] == 0)
         continue;
      if (IS_HIDE(w_ptr[idx].flags))
         continue;
      bt_idx = w_ptr[idx].bt_idx;
      if (bt_idx == 0)
         continue;
      o = w_ptr[idx].orientation;
      if ((o != 10) && (o != 11))
         continue;
      if (bt_idx <= 0)
      {
         snprintf(label, sizeof(label), "(%u) %u %u %u %u", w_ptr[idx].orientation,
                  w_ptr[idx].prop1, w_ptr[idx].prop2, w_ptr[idx].prop3, w_ptr[idx].prop4);
         sw_marker(cv, ds1_idx, mx, my, label);
         continue;
      }
      bt_ptr = d->block_table + bt_idx;
      spr = sw_block_sprite(ctx, ds1_idx, bt_ptr->dt1_idx, bt_ptr->block_idx);
      if (spr == NULL)
         continue;
      y1 = my - glb_ds1edit.win_preview.y0 - bt_ptr->zero_line * d->height_mul / d->height_div;
      y1 += d->tile_h;
      if (sw_off_view(spr, y1))
         continue;
      sx = mx - glb_ds1edit.win_preview.x0;

      if (glb_ds1edit.mode == MOD_L)
         sw_draw_lit(cv, ctx, spr, sx, y1,
                     sw_light_level(ctx, mx + d->tile_w / 2, my + d->tile_h / 2), 0);
      else
         sw_draw_plain(cv, ctx, spr, sx, y1, sw_tinted(w_ptr[idx].flags, selected));
   }
}

/* Port of wpreview_draw_tile_grid (white lines along both tile axes). */
static void sw_tile_grid(DS1_CANVAS *cv, int ds1_idx)
{
   const DS1_S *d = &glb_ds1[ds1_idx];
   int tw = d->tile_w, th = d->tile_h, dw = (int) d->width, dh = (int) d->height;
   int px = glb_ds1edit.win_preview.x0, py = glb_ds1edit.win_preview.y0;
   int base_x, base_y, mx, my, i, min_x, min_y, max_x, max_y, dummy;

   base_x = tw / 2 - px - 1;
   base_y = -py - 1;
   mx = base_x - (tw * dh / 2);
   my = base_y + (th * dh / 2);

   coord_to_tile(ds1_idx, px, py, &min_x, &dummy);
   coord_to_tile(ds1_idx, px + glb_config.screen.width, py, &dummy, &min_y);
   coord_to_tile(ds1_idx, px, py + glb_config.screen.height, &dummy, &max_y);
   coord_to_tile(ds1_idx, px + glb_config.screen.width, py + glb_config.screen.height,
                 &max_x, &dummy);
   if ((glb_ds1edit.mode == MOD_O) || (glb_ds1edit.mode == MOD_P) ||
       (glb_ds1edit.mode == MOD_L))
   {
      min_x /= 5;
      min_y /= 5;
      max_x /= 5;
      max_y /= 5;
   }
   if (min_x < 0)
      min_x = 0;
   if (min_y < 0)
      min_y = 0;
   if (max_x > dw)
      max_x = dw;
   if (max_y > dh)
      max_y = dh;

   for (i = min_x; i <= max_x; i++)
      canvas_line(cv, base_x + i * tw / 2, base_y + i * th / 2,
                  mx + i * tw / 2, my + i * th / 2, 255);

   base_x = tw / 2 - px - 1;
   base_y = -py - 1;
   mx = base_x + (tw * dw / 2);
   my = base_y + (th * dw / 2);
   for (i = min_y; i <= max_y; i++)
      canvas_line(cv, base_x - i * tw / 2, base_y + i * th / 2,
                  mx - i * tw / 2, my + i * th / 2, 255);
}

/* ---- passes ---------------------------------------------------------------- */

enum
{
   SW_PASS_BASE,      /* 1: lower walls, floors, tile shadows */
   SW_PASS_UPPER,     /* 4: upper walls                       */
   SW_PASS_ROOFS,     /* 5                                    */
   SW_PASS_SPECIAL    /* 6: orientation 10/11                 */
};

static int sw_cell_visible(int ds1_idx, int mx, int my)
{
   const DS1_S *d = &glb_ds1[ds1_idx];
   const WIN_PREVIEW_S *v = &glb_ds1edit.win_preview;

   return (mx >= v->x0 - d->tile_w) && (mx < v->x0 + v->w) &&
          (my >= v->y0 - d->tile_h * 2) && (my < v->y0 + v->h + d->tile_h * 2);
}

static void sw_pass(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx, int pass,
                    const int b[4], int cx, int cy)
{
   const DS1_S *d = &glb_ds1[ds1_idx];
   int x, y, mx, my, base_x, base_y, select;

   for (y = b[2]; y <= b[3]; y++)
   {
      base_x = y * -d->tile_w / 2;
      base_y = y * d->tile_h / 2;
      for (x = b[0]; x <= b[1]; x++)
      {
         select = (glb_ds1edit.mode == MOD_T) && (x == cx) && (y == cy);
         mx = base_x + x * d->tile_w / 2;
         my = base_y + x * d->tile_h / 2;
         if (!sw_cell_visible(ds1_idx, mx, my))
            continue;
         switch (pass)
         {
         case SW_PASS_BASE:
            sw_draw_w(cv, ctx, ds1_idx, x, y, mx, my, select, FALSE);
            sw_draw_f(cv, ctx, ds1_idx, x, y, mx, my, select);
            sw_draw_s(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         case SW_PASS_UPPER:
            sw_draw_w(cv, ctx, ds1_idx, x, y, mx, my, select, TRUE);
            break;
         case SW_PASS_ROOFS:
            sw_draw_r(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         case SW_PASS_SPECIAL:
            sw_draw_sp(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         }
      }
   }
}

void map_sw_draw_view(DS1_CANVAS *cv, int ds1_idx, const MAP_SW_CONTEXT *ctx, int cx, int cy)
{
   int b[4];

   s_text_count = 0;
   if ((cv == NULL) || (cv->pixels == NULL) || (ctx == NULL) ||
       (ctx->tables == NULL) || (ctx->trans == NULL))
      return;
   canvas_reset_clip(cv);
   canvas_fill(cv, 0);
   wpreview_get_visible_tile_bounds(ds1_idx, &b[0], &b[1], &b[2], &b[3]);

   sw_pass(cv, ctx, ds1_idx, SW_PASS_BASE, b, cx, cy);
   if (glb_ds1edit.display_tile_grid == TG_OVERFLOOR)
      sw_tile_grid(cv, ds1_idx);
   sw_pass(cv, ctx, ds1_idx, SW_PASS_UPPER, b, cx, cy);
   sw_pass(cv, ctx, ds1_idx, SW_PASS_ROOFS, b, cx, cy);
   if (glb_ds1[ds1_idx].special_layer_mask)
      sw_pass(cv, ctx, ds1_idx, SW_PASS_SPECIAL, b, cx, cy);
}

/* ---- whole-map bounds (headless) -------------------------------------------
   Port of the bounds loop at the top of wpreview_draw_tiles_big_screenshot,
   with the 1:1 index tiles' sizes in place of the bitmaps'. */

static void sw_bounds_add(long *minx, long *miny, long *maxx, long *maxy,
                          long x1, long y1, const CACHED_TILE *ct, int mul, int div)
{
   long x2 = x1 + (long) ct->width * mul / div;
   long y2 = y1 + (long) ct->height * mul / div;

   if (x1 < *minx) *minx = x1;
   if (x2 > *maxx) *maxx = x2;
   if (y1 < *miny) *miny = y1;
   if (y2 > *maxy) *maxy = y2;
}

int map_sw_full_bounds(int ds1_idx, int32_t *minx, int32_t *miny, int32_t *maxx, int32_t *maxy)
{
   const DS1_S *d = &glb_ds1[ds1_idx];
   long lminx = LONG_MAX, lminy = LONG_MAX, lmaxx = LONG_MIN, lmaxy = LONG_MIN;
   int x, y, n, mx, my, bt_idx, mul = d->height_mul, div = d->height_div;
   const BLOCK_TABLE_S *bt_ptr;
   const CACHED_TILE *ct;

   if ((minx == NULL) || (miny == NULL) || (maxx == NULL) || (maxy == NULL) || (div == 0))
      return 0;
   for (y = 0; y < d->height; y++)
   {
      int base_x = y * -d->tile_w / 2, base_y = y * d->tile_h / 2;
      for (x = 0; x < d->width; x++)
      {
         const CELL_S_S *s_ptr = d->shadow_buff + (y * d->shadow_line) + (x * d->shadow_num);
         const CELL_F_S *f_ptr = d->floor_buff + (y * d->floor_line) + (x * d->floor_num);
         const CELL_W_S *w_ptr = d->wall_buff + (y * d->wall_line) + (x * d->wall_num);

         mx = base_x + x * d->tile_w / 2;
         my = base_y + x * d->tile_h / 2;

         for (n = 0; n < d->shadow_num; n++)
         {
            bt_idx = (s_ptr[n].prop4 & 0x80) ? -1 : s_ptr[n].bt_idx;
            if (bt_idx == -1)
               continue;
            bt_ptr = d->block_table + bt_idx;
            if (bt_ptr->type != BT_SHADOW)
               continue;
            ct = sw_block_tile(bt_ptr->dt1_idx, bt_ptr->block_idx);
            if (ct != NULL)
               sw_bounds_add(&lminx, &lminy, &lmaxx, &lmaxy, mx,
                             my - bt_ptr->zero_line * mul / div + d->tile_h, ct, mul, div);
         }
         for (n = 0; n < d->floor_num; n++)
         {
            bt_idx = f_ptr[n].bt_idx;
            if (bt_idx == -1)
               continue;
            bt_ptr = d->block_table + bt_idx;
            if ((bt_ptr->type != BT_STATIC) && (bt_ptr->type != BT_ANIMATED))
               continue;
            ct = sw_block_tile(bt_ptr->dt1_idx, bt_ptr->block_idx);
            if (ct != NULL)
               sw_bounds_add(&lminx, &lminy, &lmaxx, &lmaxy, mx,
                             my - bt_ptr->zero_line * mul / div, ct, mul, div);
         }
         for (n = 0; n < d->wall_num; n++)
         {
            bt_idx = w_ptr[n].bt_idx;
            if (bt_idx == -1)
               continue;
            bt_ptr = d->block_table + bt_idx;
            if ((bt_ptr->type != BT_WALL_UP) && (bt_ptr->type != BT_WALL_DOWN) &&
                (bt_ptr->type != BT_ROOF) && (bt_ptr->type != BT_SPECIAL) &&
                (bt_ptr->type != BT_WALL_ANIMATED))
               continue;
            ct = sw_block_tile(bt_ptr->dt1_idx, bt_ptr->block_idx);
            if (ct != NULL)
               sw_bounds_add(&lminx, &lminy, &lmaxx, &lmaxy, mx,
                             my - bt_ptr->zero_line * mul / div + d->tile_h, ct, mul, div);
         }
      }
   }
   if ((lminx > lmaxx) || (lminy > lmaxy))
      return 0;
   *minx = (int32_t) lminx;
   *miny = (int32_t) lminy;
   *maxx = (int32_t) lmaxx;
   *maxy = (int32_t) lmaxy;
   return 1;
}
```

`test_full_bounds_cover_every_sprite` expects `maxy == 2`: the lower wall (zero_line 80) lands at `my - 80 + 80 = 0`, the floor at 0, both 2 rows high. Add `src/render/map_sw.c` to `SOURCES` in `CMakeLists.txt` after `src/render/view_geom.c`.

- [ ] **Step 5: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_map_sw ds1edit
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_map_sw$" --output-on-failure
```
Expected: `14 Tests 0 Failures 0 Ignored`, `100% tests passed`; `ds1edit` still links (map_sw is compiled but not yet called).

- [ ] **Step 6: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/map_sw.h src/render/map_sw.c test/test_render_map_sw.c test/cmake/software_renderer.cmake CMakeLists.txt
git commit -m "Compose the map's tile layers into the 8-bit canvas, in the GPU path's order"
```

---

### Task 10: The software map compositor — objects, COF blend modes 0-6, object shadows

**Files:**
- Modify: `src/render/map_sw.c`
- Create: `test/test_render_map_sw_objects.c`
- Modify: `test/cmake/software_renderer.cmake`

**Interfaces:**
- Consumes: everything Task 9 consumes, plus `COF_S`, `LAY_INF_S`, `OBJ_S`, `OBJ_DESC_S` (`structs.h`); `sw_tables_blend` (Task 5).
- Produces: no new public symbols. `map_sw_draw_view` now also draws pass 2 (object shadows), pass 3 (objects with `orderflag` 1) and the objects with `orderflag` 0/2 inside pass 4, exactly where `wpreview_draw_tiles()` draws them, and queues the `OL_SPEED` labels. Mapping: COF layer with `trans_a` and `trans_b` in 0-4 or 6 -> `canvas_blit_rle_cmap(sw_tables_blend(trans_b))`; `trans_b` 5 or `trans_a == 0` -> opaque; object shadow (`shad_a`) -> `canvas_blit_rle_shade(tables->dark[trans_a ? 28 : 10])`, the D2 darkness levels `a5_draw_shadow_sprite` approximates with an alpha.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_map_sw_objects.c`:

```c
/* Software map compositor, objects (map_sw.c): pass placement by orderflag,
 * every COF trans_b mode through its own D2CMP table, the two shadow darkness
 * levels, the speed label, last_good_frame, zoomed offsets, and the
 * placeholder for a frame whose index data is missing. Tables hold marker
 * values (0xA1..0xA6, DARK_BASE + level) so each pixel names its table. */

#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "render/canvas.h"
#include "render/map_sw.h"
#include "render/spritecache.h"
#include "render/sw_tables.h"

static DS1_S s_ds1[1];
DS1_S *glb_ds1 = s_ds1;

#define CW 200
#define CH 120
#define DARK_BASE 100

static BLOCK_TABLE_S s_bt[8];
static BLOCK_S       s_bh[8];
static CACHED_TILE  *s_tiles[8];
static CELL_F_S      s_floor[1];
static CELL_W_S      s_wall[2];
static CELL_S_S      s_shadow[1];
static uint8_t      *s_pl2;
static DS1_CANVAS    s_cv;
static MAP_SW_CONTEXT s_ctx;

static COF_S         s_cof;
static OBJ_DESC_S    s_desc[1];
static OBJ_S         s_obj[1];
static int           s_order[1];
static UBYTE         s_priority[2];
static CACHED_TILE  *s_lay_cache[2];
static ALLEGRO_BITMAP *s_lay_bmp[2];
static int           s_dummy_bitmap;

static CACHED_TILE *solid_tile(int w, int h, uint8_t v)
{
   CACHED_TILE *t = (CACHED_TILE *) calloc(1, sizeof(*t));
   t->width = w;
   t->height = h;
   t->indices = (uint8_t *) malloc((size_t) w * h);
   memset(t->indices, v, (size_t) w * h);
   return t;
}

static void free_tile(CACHED_TILE **t)
{
   if (*t != NULL)
   {
      free((*t)->indices);
      free(*t);
      *t = NULL;
   }
}

void setUp(void)
{
   long off;
   int L;

   spritecache_clear();
   memset(s_ds1, 0, sizeof(s_ds1));
   memset(s_bt, 0, sizeof(s_bt));
   memset(s_bh, 0, sizeof(s_bh));
   memset(s_floor, 0, sizeof(s_floor));
   memset(s_wall, 0, sizeof(s_wall));
   memset(s_shadow, 0, sizeof(s_shadow));

   /* block 4: a large upper wall covering the object's spot */
   s_bt[4].type = BT_WALL_UP;
   s_bt[4].dt1_idx = 1;
   s_bt[4].block_idx = 4;
   s_bt[4].zero_line = 80;
   s_bt[4].corner_pair_idx = -1;
   s_bh[4].size_x = 100;
   s_bh[4].size_y = -30;
   s_bh[4].tiles_number = 1;
   s_tiles[4] = solid_tile(100, 30, 40);

   memset(&glb_dt1[1], 0, sizeof(glb_dt1[1]));
   strcpy(glb_dt1[1].name, "authored.dt1");
   glb_dt1[1].block_num = 8;
   glb_dt1[1].bh_buffer = s_bh;
   glb_dt1[1].block_cache[ZM_11] = s_tiles;

   s_ds1[0].width = 1;
   s_ds1[0].height = 1;
   s_ds1[0].tile_w = 160;
   s_ds1[0].tile_h = 80;
   s_ds1[0].height_mul = 1;
   s_ds1[0].height_div = 1;
   s_ds1[0].cur_zoom = ZM_11;
   s_ds1[0].block_table = s_bt;
   s_ds1[0].bt_num = 8;
   s_ds1[0].floor_buff = s_floor;
   s_ds1[0].floor_num = 1;
   s_ds1[0].floor_line = 1;
   s_ds1[0].wall_buff = s_wall;
   s_ds1[0].wall_num = 2;
   s_ds1[0].wall_line = 2;
   s_ds1[0].shadow_buff = s_shadow;
   s_ds1[0].shadow_num = 1;
   s_ds1[0].shadow_line = 1;
   s_ds1[0].floor_layer_mask[0] = 1;
   s_ds1[0].wall_layer_mask[0] = 1;
   s_ds1[0].wall_layer_mask[1] = 1;
   s_ds1[0].shadow_layer_mask[0] = 3;
   s_ds1[0].animations_layer_mask = 1;
   s_ds1[0].objects_layer_mask = OL_NONE;

   /* one object at sub-cell (0,0): dx0 = 80, dy0 = 12 -> canvas (90, 22) */
   memset(&s_cof, 0, sizeof(s_cof));
   s_cof.lay = 1;
   s_cof.fpd = 1;
   s_cof.dir = 1;
   s_cof.priority = s_priority;
   s_priority[0] = 0;
   s_priority[1] = 0;
   s_cof.orderflag = 0;
   s_lay_cache[0] = solid_tile(2, 2, 70);
   s_lay_cache[1] = NULL;
   s_lay_bmp[0] = (ALLEGRO_BITMAP *) &s_dummy_bitmap;   /* never dereferenced by map_sw */
   s_lay_bmp[1] = NULL;
   s_cof.lay_inf[0].bmp_num = 1;
   s_cof.lay_inf[0].cache = s_lay_cache;
   s_cof.lay_inf[0].bmp = s_lay_bmp;

   memset(s_desc, 0, sizeof(s_desc));
   s_desc[0].usage_count = 1;
   s_desc[0].cof = &s_cof;
   glb_ds1edit.obj_desc = s_desc;

   memset(s_obj, 0, sizeof(s_obj));
   s_obj[0].desc_idx = 0;
   s_order[0] = 0;
   s_ds1[0].obj = s_obj;
   s_ds1[0].obj_num = 1;
   s_ds1[0].drawing_order = s_order;

   glb_ds1edit.mode = MOD_T;
   glb_ds1edit.display_tile_grid = TG_OFF;
   glb_ds1edit.win_preview.x0 = -10;
   glb_ds1edit.win_preview.y0 = -10;
   glb_ds1edit.win_preview.w = CW;
   glb_ds1edit.win_preview.h = CH;
   glb_config.screen.width = CW;
   glb_config.screen.height = CH;
   glb_config.stretch_sprites = TRUE;

   s_pl2 = (uint8_t *) calloc(SW_PL2_FULL_SIZE, 1);
   for (L = 1; L <= 32; L++)
      memset(s_pl2 + SW_PL2_OFF_DARK + (L - 1) * 256, DARK_BASE + L, 256);
   for (off = 0; off < 65536; off++)
   {
      s_pl2[SW_PL2_OFF_ALPHA25 + off] = 0xA1;
      s_pl2[SW_PL2_OFF_ALPHA50 + off] = 0xA2;
      s_pl2[SW_PL2_OFF_ALPHA75 + off] = 0xA3;
      s_pl2[SW_PL2_OFF_ADD + off] = 0xA4;
      s_pl2[SW_PL2_OFF_MUL + off] = 0xA5;
      s_pl2[SW_PL2_OFF_SCREEN + off] = 0xA6;
   }
   glb_ds1edit.d2_pal[0] = s_pl2;
   glb_ds1edit.pal_size[0] = SW_PL2_FULL_SIZE;
   memset(&glb_ds1edit.cmap[CM_TRANS][0], 99, sizeof(INDEX_COLORMAP));
   memset(&glb_ds1edit.vga_pal[0], 0, sizeof(RGBA_PALETTE));

   TEST_ASSERT_EQUAL_INT(1, map_sw_prepare_context(&s_ctx, 0, 100000, 100000));
   TEST_ASSERT_EQUAL_INT(1, canvas_create(&s_cv, CW, CH, 1));
}

void tearDown(void)
{
   canvas_destroy(&s_cv);
   spritecache_clear();
   free_tile(&s_tiles[4]);
   free_tile(&s_lay_cache[0]);
   free_tile(&s_lay_cache[1]);
   glb_dt1[1].block_cache[ZM_11] = NULL;
   glb_dt1[1].bh_buffer = NULL;
   glb_ds1edit.d2_pal[0] = NULL;
   glb_ds1edit.obj_desc = NULL;
   free(s_pl2);
}

static uint8_t px(int x, int y) { return s_cv.pixels[y * s_cv.stride + x]; }
static void draw(void) { map_sw_draw_view(&s_cv, 0, &s_ctx, -1, -1); }

void test_object_draws_at_its_sub_cell_position(void)
{
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(90, 22));
   TEST_ASSERT_EQUAL_HEX8(70, px(91, 23));
   TEST_ASSERT_EQUAL_HEX8(0, px(92, 22));
}

void test_orderflag_decides_before_or_after_upper_walls(void)
{
   s_wall[1].bt_idx = 4;
   s_wall[1].orientation = 1;                  /* upper wall over (10..109, 10..39) */
   s_cof.orderflag = 1;                        /* pass 3: behind the walls */
   draw();
   TEST_ASSERT_EQUAL_HEX8(40, px(90, 22));
   s_cof.orderflag = 0;                        /* pass 4, after this tile's walls */
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(90, 22));
   s_cof.orderflag = 2;
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(90, 22));
}

void test_each_trans_b_mode_uses_its_d2cmp_table(void)
{
   static const uint8_t expect[7] = { 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 70, 0xA6 };
   int mode;

   s_cof.lay_inf[0].trans_a = 1;
   for (mode = 0; mode <= 6; mode++)
   {
      s_cof.lay_inf[0].trans_b = (UBYTE) mode;
      draw();
      TEST_ASSERT_EQUAL_HEX8_MESSAGE(expect[mode], px(90, 22), "trans_b table");
   }
   s_cof.lay_inf[0].trans_a = 0;                /* trans_b ignored without trans_a */
   s_cof.lay_inf[0].trans_b = 3;
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(90, 22));
}

void test_object_shadows_use_darkness_10_or_28(void)
{
   s_cof.orderflag = 3;                         /* body never drawn: only the shadow pass */
   s_cof.lay_inf[0].shad_a = 1;
   draw();
   TEST_ASSERT_EQUAL_HEX8(DARK_BASE + 10, px(90, 22));
   s_cof.lay_inf[0].trans_a = 1;
   draw();
   TEST_ASSERT_EQUAL_HEX8(DARK_BASE + 28, px(90, 22));
   s_cof.lay_inf[0].shad_a = 0;
   draw();
   TEST_ASSERT_EQUAL_HEX8(0, px(90, 22));
}

void test_speed_label_is_queued_in_speed_mode(void)
{
   s_ds1[0].objects_layer_mask = OL_SPEED;
   s_cof.spd_mul = 7;
   draw();
   TEST_ASSERT_EQUAL_INT(2, map_sw_text_count());
   TEST_ASSERT_EQUAL_STRING("speed =   7", map_sw_text_at(0)->text);
   TEST_ASSERT_EQUAL_INT32(47, map_sw_text_at(0)->x);
   TEST_ASSERT_EQUAL_INT32(15, map_sw_text_at(0)->y);
   TEST_ASSERT_EQUAL_HEX8(0, map_sw_text_at(0)->color);
   TEST_ASSERT_EQUAL_HEX8(255, map_sw_text_at(1)->color);
}

void test_frame_beyond_the_layer_uses_last_good_frame(void)
{
   s_cof.fpd = 2;
   s_cof.cur_frame = 1;                          /* the layer has only frame 0 */
   s_cof.lay_inf[0].last_good_frame = 0;
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(90, 22));
}

void test_zoomed_offsets_follow_stretch_sprite(void)
{
   s_ds1[0].cur_zoom = ZM_12;
   s_ds1[0].height_div = 2;
   s_ds1[0].tile_w = 80;
   s_ds1[0].tile_h = 40;
   s_cof.xoffset = 3;
   s_cof.lay_inf[0].off_x = 3;
   /* dx0 = 40, dy0 = 8. stretch: 3/2 + 3/2 = 2; combined: 6/2 = 3 */
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(52, 18));
   TEST_ASSERT_EQUAL_HEX8(0, px(53, 18));
   glb_config.stretch_sprites = FALSE;
   draw();
   TEST_ASSERT_EQUAL_HEX8(70, px(53, 18));
}

void test_frame_without_index_data_draws_a_placeholder(void)
{
   free_tile(&s_lay_cache[0]);                  /* bitmap present, index data missing */
   draw();
   TEST_ASSERT_EQUAL_HEX8(s_ctx.placeholder, px(90, 22));
   TEST_ASSERT_EQUAL_HEX8(s_ctx.placeholder, px(90 + 15, 22 + 15));
   s_lay_bmp[0] = NULL;                         /* nothing at all: nothing drawn, as on the GPU path */
   draw();
   TEST_ASSERT_EQUAL_HEX8(0, px(90, 22));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_object_draws_at_its_sub_cell_position);
   RUN_TEST(test_orderflag_decides_before_or_after_upper_walls);
   RUN_TEST(test_each_trans_b_mode_uses_its_d2cmp_table);
   RUN_TEST(test_object_shadows_use_darkness_10_or_28);
   RUN_TEST(test_speed_label_is_queued_in_speed_mode);
   RUN_TEST(test_frame_beyond_the_layer_uses_last_good_frame);
   RUN_TEST(test_zoomed_offsets_follow_stretch_sprite);
   RUN_TEST(test_frame_without_index_data_draws_a_placeholder);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
ds1edit_visual_test(test_render_map_sw_objects ALLEGRO SOURCES
    test_render_map_sw_objects.c ${_map_sw_sources})
ds1edit_sw_add_test(render_map_sw_objects test_render_map_sw_objects)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_map_sw_objects
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_map_sw_objects$" --output-on-failure
```
Expected: FAIL — `test_object_draws_at_its_sub_cell_position: Expected 0x46 Was 0x00` (objects are not drawn yet).

- [ ] **Step 3: Add the object passes to `map_sw.c`**

Insert before the `/* ---- passes ---- */` section:

```c
/* ---- objects ------------------------------------------------------------------
   Ports of wpreview_draw_an_object, wpreview_draw_an_object_shad and the
   wpreview_draw_obj_tile_* walkers (preview.c). */

static const DS1_RLE *sw_layer_sprite(const MAP_SW_CONTEXT *ctx, int ds1_idx,
                                      LAY_INF_S *lay, int f)
{
   CACHED_TILE        *ct = NULL;
   DS1EDIT_INDEX_FRAME fr;
   int                 mul, div;

   if ((f < 0) || (f >= lay->bmp_num))
      return NULL;
   if (lay->cache != NULL)
      ct = lay->cache[f];
   if ((ct != NULL) && (ct->indices != NULL) && (ct->width > 0) && (ct->height > 0))
   {
      sw_frame_view(ct, &fr);
      return spritecache_get(ct, &fr, sw_zoom(ds1_idx));
   }
   /* The GPU path draws this frame only if it has a bitmap; with a bitmap
      but no index data the decode failed half-way: placeholder. */
   if ((lay->bmp == NULL) || (lay->bmp[f] == NULL))
      return NULL;
   spritecache_log_failure_once(&lay->bmp[f], "a COF layer frame");
   if (!sc_zoom_ratio(sw_zoom(ds1_idx), &mul, &div))
      return NULL;
   return spritecache_placeholder(16 * mul / div > 0 ? 16 * mul / div : 1,
                                  16 * mul / div > 0 ? 16 * mul / div : 1, ctx->placeholder);
}

static void sw_draw_object(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx, int o,
                           int shadow)
{
   DS1_S         *d = &glb_ds1[ds1_idx];
   COF_S         *cof;
   LAY_INF_S     *lay;
   const DS1_RLE *spr;
   const uint8_t *tbl;
   int            c, f, p, x, y, desc, stretch;
   int            mul = d->height_mul, div = d->height_div;
   int32_t        dx0, dy0, dx, dy;
   UBYTE          new_frame;
   char           label[48];

   desc = d->obj[o].desc_idx;
   if (desc == -1)
      return;
   if (glb_ds1edit.obj_desc[desc].usage_count == 0)
      return;
   cof = glb_ds1edit.obj_desc[desc].cof;
   if ((cof == NULL) || (cof->fpd == 0))
      return;

   new_frame = (UBYTE) (cof->cur_frame + d->obj[o].frame_delta);
   while (new_frame >= cof->fpd)
      new_frame -= cof->fpd;

   x = (int) d->obj[o].x;
   y = (int) d->obj[o].y;
   dx0 = ((y - 2) * -d->tile_w / 10) + ((x + 3) * d->tile_w / 10);
   dy0 = 4 + ((y - 2) * d->tile_h / 10) + ((x + 3) * d->tile_h / 10);
   stretch = !((d->cur_zoom == ZM_11) || (d->cur_zoom == ZM_21) ||
               (glb_config.stretch_sprites != TRUE));

   for (c = 0; c < cof->lay; c++)
   {
      p = (cof->cur_dir * cof->lay * cof->fpd);
      p += c + (new_frame * cof->lay);
      p = cof->priority[p];
      if (p >= COMPOSIT_NB)
         continue;
      lay = &cof->lay_inf[p];
      if (shadow && (lay->shad_a == 0))
         continue;
      if ((lay->bmp == NULL) && (lay->cache == NULL))
         continue;
      f = (new_frame >= lay->bmp_num) ? lay->last_good_frame : new_frame;
      spr = sw_layer_sprite(ctx, ds1_idx, lay, f);
      if (spr == NULL)
         continue;

      if (shadow || stretch)
      {
         /* separate rounding of the two offsets, as the GPU stretch and
            shadow paths do */
         dx = dx0 - glb_ds1edit.win_preview.x0 + (cof->xoffset * mul / div) + (lay->off_x * mul / div);
         dy = dy0 - glb_ds1edit.win_preview.y0 + (cof->yoffset * mul / div) + (lay->off_y * mul / div);
      }
      else
      {
         dx = dx0 - glb_ds1edit.win_preview.x0 + (cof->xoffset + lay->off_x) * mul / div;
         dy = dy0 - glb_ds1edit.win_preview.y0 + (cof->yoffset + lay->off_y) * mul / div;
      }

      if (shadow)
         canvas_blit_rle_shade(cv, spr, dx, dy, 0, ctx->tables->dark[lay->trans_a ? 28 : 10]);
      else
      {
         tbl = NULL;
         if (lay->trans_a && (lay->trans_b <= 6) && (lay->trans_b != 5))
            tbl = sw_tables_blend(ctx->tables, lay->trans_b);
         if (tbl != NULL)
            canvas_blit_rle_cmap(cv, spr, dx, dy, 0, tbl);
         else
            canvas_blit_rle(cv, spr, dx, dy, 0);
         if (new_frame < lay->bmp_num)
            lay->last_good_frame = f;
      }
   }

   if (shadow || (glb_ds1edit.mode == MOD_O))
      return;
   if (d->objects_layer_mask == OL_SPEED)
   {
      dx = dx0 - glb_ds1edit.win_preview.x0 - 44;
      dy = dy0 - glb_ds1edit.win_preview.y0 - 8;
      snprintf(label, sizeof(label), "speed = %3li", cof->spd_mul);
      sw_text(dx + 1, dy + 1, 0, label);
      sw_text(dx, dy, 255, label);
   }
}

/* Which objects a walker draws (see wpreview_draw_obj_tile_*). */
enum { SW_OBJ_SHADOW, SW_OBJ_ORDER_1, SW_OBJ_ORDER_0_2 };

static void sw_obj_tile(DS1_CANVAS *cv, const MAP_SW_CONTEXT *ctx, int ds1_idx,
                        int x, int y, int *cur_idx, int which)
{
   DS1_S *d = &glb_ds1[ds1_idx];
   OBJ_S *obj;
   COF_S *cof;
   int    o, desc;

   if (*cur_idx >= d->obj_num)
      return;
   o = d->drawing_order[*cur_idx];
   obj = &d->obj[o];
   while (obj->ty < y)
   {
      (*cur_idx)++;
      if (*cur_idx >= d->obj_num)
         return;
      o = d->drawing_order[*cur_idx];
      obj = &d->obj[o];
   }
   if (obj->ty != y)
      return;
   while (obj->tx < x)
   {
      (*cur_idx)++;
      if (*cur_idx >= d->obj_num)
         return;
      o = d->drawing_order[*cur_idx];
      obj = &d->obj[o];
   }

   while ((obj->tx == x) && (obj->ty == y))
   {
      if (which == SW_OBJ_SHADOW)
         sw_draw_object(cv, ctx, ds1_idx, o, 1);
      else
      {
         desc = obj->desc_idx;
         cof = ((desc != -1) && glb_ds1edit.obj_desc[desc].usage_count)
               ? glb_ds1edit.obj_desc[desc].cof : NULL;
         if (cof != NULL)
         {
            if ((which == SW_OBJ_ORDER_1) && (cof->orderflag == 1))
               sw_draw_object(cv, ctx, ds1_idx, o, 0);
            if ((which == SW_OBJ_ORDER_0_2) &&
                ((cof->orderflag == 0) || (cof->orderflag == 2)))
               sw_draw_object(cv, ctx, ds1_idx, o, 0);
         }
      }
      (*cur_idx)++;
      if (*cur_idx >= d->obj_num)
         return;
      o = d->drawing_order[*cur_idx];
      obj = &d->obj[o];
   }
}
```

Replace the pass enum with:

```c
enum
{
   SW_PASS_BASE,          /* 1: lower walls, floors, tile shadows       */
   SW_PASS_OBJ_SHADOWS,   /* 2: object shadows                          */
   SW_PASS_OBJ_BEHIND,    /* 3: objects with orderflag 1                */
   SW_PASS_UPPER,         /* 4: upper walls, then objects 0/2 per tile  */
   SW_PASS_ROOFS,         /* 5                                          */
   SW_PASS_SPECIAL        /* 6: orientation 10/11                       */
};
```

In `sw_pass`, add `int cur_idx = 0;` to the declarations (each pass walks the drawing order from the start, as each GPU pass resets `objdraw_cur_idx`), and change the switch to:

```c
         switch (pass)
         {
         case SW_PASS_BASE:
            sw_draw_w(cv, ctx, ds1_idx, x, y, mx, my, select, FALSE);
            sw_draw_f(cv, ctx, ds1_idx, x, y, mx, my, select);
            sw_draw_s(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         case SW_PASS_OBJ_SHADOWS:
            sw_obj_tile(cv, ctx, ds1_idx, x, y, &cur_idx, SW_OBJ_SHADOW);
            break;
         case SW_PASS_OBJ_BEHIND:
            sw_obj_tile(cv, ctx, ds1_idx, x, y, &cur_idx, SW_OBJ_ORDER_1);
            break;
         case SW_PASS_UPPER:
            sw_draw_w(cv, ctx, ds1_idx, x, y, mx, my, select, TRUE);
            if (d->animations_layer_mask)
               sw_obj_tile(cv, ctx, ds1_idx, x, y, &cur_idx, SW_OBJ_ORDER_0_2);
            break;
         case SW_PASS_ROOFS:
            sw_draw_r(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         case SW_PASS_SPECIAL:
            sw_draw_sp(cv, ctx, ds1_idx, x, y, mx, my, select);
            break;
         }
```

In `map_sw_draw_view`, between the `SW_PASS_BASE` call and the grid, insert:

```c
   if (glb_ds1[ds1_idx].animations_layer_mask)
   {
      sw_pass(cv, ctx, ds1_idx, SW_PASS_OBJ_SHADOWS, b, cx, cy);
      sw_pass(cv, ctx, ds1_idx, SW_PASS_OBJ_BEHIND, b, cx, cy);
   }
```

- [ ] **Step 4: Run both compositor tests**

```bash
"$CMAKE" --build tmp/build-sw --config Release --target test_render_map_sw test_render_map_sw_objects ds1edit
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_map_sw(_objects)?$" --output-on-failure
```
Expected: `8 Tests 0 Failures 0 Ignored` for objects, `14 Tests 0 Failures` for tiles, `100% tests passed, 0 tests failed out of 2`.

- [ ] **Step 5: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/map_sw.c test/test_render_map_sw_objects.c test/cmake/software_renderer.cmake
git commit -m "Compose objects through the D2CMP blend tables, with darkness-table shadows"
```

---

### Task 11: Wire the software path into the frame loop, headless mode and a windowed runtime

**Files:**
- Modify: `src/render/preview.c`, `src/render/preview.h`, `src/main.c`, `src/misc.c`, `src/structs.h` (`CMD_LINE_S`)
- Create: `scripts/equivalence/sw_runtime.py`, `test/python/test_sw_runtime.py`

**Interfaces:**
- Consumes: `map_sw_*` (Tasks 9-10), `present_*` (Task 6), `render_select_*` (Task 7), `canvas_create`, `canvas_destroy`, `canvas_write_dump` (Task 1).
- Produces:
  - `src/render/preview.h`: `extern double glb_sw_compose_ms; extern double glb_sw_upload_ms;` (last software frame's compose and upload times, 0 on the GPU path) and `int wpreview_draw_tiles_big_screenshot_sw(int ds1_idx, const char *out_image, const char *out_indices);` (0 ok, -1 failure).
  - `CMD_LINE_S`: `char *selftest_indices; char *headless_indices; int uncapped;` and the flags `--selftest-indices=<path>`, `--headless-indices=<path>`, `--uncapped` (vsync off and no 60 Hz pacing; benchmarks only).
  - `scripts/equivalence/sw_runtime.py`: `RuntimeSetupError`, `DEFAULT_GAME_DIR`, `INI_NAME`, `RENDERERS`, `DEVICES`, `ZOOMS`, `suppress_error_dialogs()`, `find_archives(game_dir) -> dict`, `ini_text(*, archives, renderer, zoom="1:1", width=1280, height=720, gamma="1.30") -> str`, `assert_windowed(text)`, `build(runtime, exe, ini, *, device="native", soft_gl_dir=None, assets_dir=REPO_ROOT/"assets") -> (exe_path, env)`, `first_map_entry(map_name, assets_dir) -> (lvltype, lvlprest, ds1)`, `selftest_args(map_name, frames, shot, *, indices=None, deterministic=False, uncapped=False, zoom=None) -> list`, `headless_args(map_name, zoom, out_image, *, indices=None, assets_dir=...) -> list`, `run(exe, runtime, args, env, timeout_s=600.0) -> CompletedProcess`, `parse_renderer(text) -> dict|None` (`{"active","setting","device"}`), `device_ok(device, info) -> (bool, str)`; CLI `smoke` and `headless`.

What the frame looks like on the software path in phase 1: `wpreview_draw_tiles()` computes the mouse tile and animates objects as before, then — instead of GPU passes 1-6 — composes them into the canvas, presents the canvas into `screen_buff` with one write-only lock, draws the queued labels, and falls through to the unchanged Allegro code from pass 7 on (walkable info, grid over walls, cursor, paths, object labels, help window, HUD). `misc_draw_screen()` then draws `screen_buff`, the sidebar and the properties panel, and flips, as before.

- [ ] **Step 1: Write the failing Python tests**

Create `test/python/test_sw_runtime.py`:

```python
"""Tests for scripts/equivalence/sw_runtime.py (fake trees; no game data, no editor)."""

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.equivalence import sw_runtime as sw  # noqa: E402


class IniTests(unittest.TestCase):
    def test_ini_is_windowed_and_names_the_renderer(self):
        t = sw.ini_text(archives={"d2data": r"C:\x\d2data.mpq"}, renderer="software",
                        zoom="1:16", width=1920, height=1080)
        self.assertIn("full_screen = NO\n", t)
        self.assertIn("renderer = software\n", t)
        self.assertIn("default_zoom = 1:16\n", t)
        self.assertIn("screen_width = 1920\n", t)
        self.assertIn("screen_height = 1080\n", t)
        self.assertIn("d2data = C:\\x\\d2data.mpq\n", t)
        self.assertIn("d2exp = \n", t)
        sw.assert_windowed(t)

    def test_assert_windowed_refuses_fullscreen_or_missing(self):
        for bad in ("full_screen = YES\n", "screen_width = 800\n",
                    "full_screen = NO\nfull_screen = YES\n", "full_screen =\n"):
            with self.assertRaises(sw.RuntimeSetupError, msg=bad):
                sw.assert_windowed(bad)

    def test_bad_renderer_or_zoom_is_refused(self):
        with self.assertRaises(sw.RuntimeSetupError):
            sw.ini_text(archives={}, renderer="vulkan")
        with self.assertRaises(sw.RuntimeSetupError):
            sw.ini_text(archives={}, renderer="gpu", zoom="3:1")


class BuildTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.bin = base / "bin"
        self.bin.mkdir()
        self.exe = self.bin / "ds1edit.exe"
        self.exe.write_bytes(b"MZ")
        (self.bin / "a.dll").write_bytes(b"dll")
        (self.bin / "notes.txt").write_text("not copied")
        (self.bin / "allegro5.cfg").write_text("[graphics]\ndriver=direct3d\n")
        self.assets = base / "assets"
        (self.assets / "editor").mkdir(parents=True)
        (self.assets / "editor" / "ds1edit.dt1").write_bytes(b"dt1")
        (self.assets / "editor" / "pal0.bin").write_bytes(b"stale")
        (self.assets / "editor" / "cmap2_0_0_x_g22.bin").write_bytes(b"stale")
        (self.assets / "tiles" / "ACT1").mkdir(parents=True)
        (self.assets / "tiles" / "ACT1" / "x.ds1").write_bytes(b"big")
        (self.assets / "act1_town.ini").write_text("; comment\n1 1 assets/tiles/ACT1/TOWN/townE1.ds1\n")
        self.rt = base / "rt"
        self.ini = sw.ini_text(archives={}, renderer="software")

    def tearDown(self):
        self.tmp.cleanup()

    def test_build_copies_exe_libs_assets_and_writes_the_ini(self):
        exe, env = sw.build(self.rt, self.exe, self.ini, assets_dir=self.assets)
        self.assertEqual(self.rt / "ds1edit.exe", exe)
        self.assertTrue((self.rt / "a.dll").is_file())
        self.assertFalse((self.rt / "notes.txt").exists())
        self.assertTrue((self.rt / "assets" / "editor" / "ds1edit.dt1").is_file())
        self.assertFalse((self.rt / "assets" / "editor" / "pal0.bin").exists())
        self.assertFalse((self.rt / "assets" / "editor" / "cmap2_0_0_x_g22.bin").exists())
        self.assertFalse((self.rt / "assets" / "tiles").exists())
        self.assertEqual(self.ini, (self.rt / sw.INI_NAME).read_text(encoding="ascii"))
        self.assertIn("direct3d", (self.rt / "allegro5.cfg").read_text())
        self.assertEqual(str(self.rt / "appdata"), env["APPDATA"])

    def test_build_refuses_a_fullscreen_ini_before_touching_disk(self):
        with self.assertRaises(sw.RuntimeSetupError):
            sw.build(self.rt, self.exe, "full_screen = YES\n", assets_dir=self.assets)
        self.assertFalse(self.rt.exists())

    def test_soft_device_forces_opengl(self):
        if os.name == "nt":
            with self.assertRaises(sw.RuntimeSetupError):
                sw.build(self.rt, self.exe, self.ini, device="llvmpipe", assets_dir=self.assets)
            gl = Path(self.tmp.name) / "mesa"
            gl.mkdir()
            (gl / "opengl32.dll").write_bytes(b"mesa")
            (gl / "libgallium_wgl.dll").write_bytes(b"mesa")
            _, env = sw.build(self.rt, self.exe, self.ini, device="llvmpipe",
                              soft_gl_dir=gl, assets_dir=self.assets)
            self.assertEqual(b"mesa", (self.rt / "opengl32.dll").read_bytes())
            self.assertTrue((self.rt / "libgallium_wgl.dll").is_file())
        else:
            _, env = sw.build(self.rt, self.exe, self.ini, device="llvmpipe",
                              assets_dir=self.assets)
            self.assertEqual("1", env["LIBGL_ALWAYS_SOFTWARE"])
        self.assertIn("driver=opengl", (self.rt / "allegro5.cfg").read_text())
        self.assertEqual("llvmpipe", env["GALLIUM_DRIVER"])

    def test_headless_args_use_the_first_map_entry(self):
        args = sw.headless_args("act1_town", "1:1", "out.bmp", indices="out.idx",
                                assets_dir=self.assets)
        self.assertEqual(["assets/tiles/ACT1/TOWN/townE1.ds1", "1", "1", "--zoom=1:1",
                          "--headless", "out.bmp", "--headless-indices=out.idx"], args)


class ParseTests(unittest.TestCase):
    def test_parse_renderer_line(self):
        text = 'x\nrenderer: software (renderer = auto, device "llvmpipe (LLVM 15, 256 bits)")\ny\n'
        self.assertEqual({"active": "software", "setting": "auto",
                          "device": "llvmpipe (LLVM 15, 256 bits)"}, sw.parse_renderer(text))
        self.assertIsNone(sw.parse_renderer("nothing here"))

    def test_device_ok(self):
        info = {"active": "software", "setting": "software", "device": "llvmpipe (LLVM 15)"}
        self.assertTrue(sw.device_ok("native", None)[0])
        self.assertTrue(sw.device_ok("llvmpipe", info)[0])
        self.assertFalse(sw.device_ok("warp", info)[0])
        self.assertFalse(sw.device_ok("llvmpipe", None)[0])
        warp = dict(info, device="D3D12 (Microsoft Basic Render Driver)")
        self.assertTrue(sw.device_ok("warp", warp)[0])

    def test_selftest_args(self):
        self.assertEqual(["assets/act1_town.ini", "--selftest=30", "--selftest-shot=s.png",
                          "--selftest-indices=f.idx", "--deterministic", "--uncapped",
                          "--zoom=1:16"],
                         sw.selftest_args("act1_town", 30, "s.png", indices="f.idx",
                                          deterministic=True, uncapped=True, zoom="1:16"))

    def test_find_archives_is_case_insensitive(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "D2DATA.MPQ").write_bytes(b"x")
            (Path(d) / "d2exp.mpq").write_bytes(b"x")
            found = sw.find_archives(Path(d))
            self.assertEqual({"d2data", "d2exp"}, set(found))
        self.assertEqual({}, sw.find_archives(Path(d) / "missing"))


if __name__ == "__main__":
    unittest.main()
```

Run: `python -m unittest -v test.python.test_sw_runtime`
Expected: `ModuleNotFoundError: No module named 'scripts.equivalence.sw_runtime'`.

- [ ] **Step 2: Write `scripts/equivalence/sw_runtime.py`**

```python
"""Private, windowed-only runtimes for the software-renderer lanes.

Each lane runs the editor from a directory it owns: the candidate executable
and the shared libraries beside it, the repository's assets/ (without cached
palettes and colour maps, and without the extracted assets/tiles -- the editor
reads DS1s from the archives first), and an owned Ds1edit.ini. That INI always
says full_screen = NO: these lanes never open a fullscreen window, and
assert_windowed() refuses any INI that could.

Game data comes from the vanilla 1.13c install (DEFAULT_GAME_DIR) or the one
the caller names; without it only the INI and copy logic run, which is what
test/python/test_sw_runtime.py covers. Local lane only.

    python -m scripts.equivalence.sw_runtime smoke --exe bin/ds1edit.exe \\
        --renderer software --map act1_town --frames 120 --out-dir tmp/sw-smoke
    python -m scripts.equivalence.sw_runtime headless --exe bin/ds1edit.exe \\
        --renderer software --map act1_town --zoom 1:1 --out-dir tmp/sw-headless
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_GAME_DIR = Path(r"F:\D2VersionChanger\VersionChanger\LoD\1.13c")
INI_NAME = "Ds1edit.ini"          # src/config.h DS1EDIT_INI_NAME; case matters on Linux
ARCHIVES = (("patch_d2", "patch_d2.mpq"), ("d2exp", "d2exp.mpq"),
            ("d2data", "d2data.mpq"), ("d2char", "d2char.mpq"))
RENDERERS = ("gpu", "software", "auto")
DEVICES = ("native", "llvmpipe", "warp")
DEVICE_PATTERNS = {"native": None,
                   "llvmpipe": re.compile(r"llvmpipe"),
                   "warp": re.compile(r"Basic Render|WARP")}
ZOOMS = ("2:1", "1:1", "1:2", "1:4", "1:8", "1:16")
RENDERER_LINE = re.compile(r'^renderer: (\w+) \(renderer = (\w+), device "([^"]*)"\)', re.M)
ASSET_IGNORE = shutil.ignore_patterns("pal*.bin", "cmap*.bin", "version", "tiles")
LIB_SUFFIXES = (".dll", ".so", ".dylib")


class RuntimeSetupError(Exception):
    """The runtime could not be built as asked (never a product verdict)."""


def suppress_error_dialogs() -> None:
    """SetErrorMode(0x8003) so a missing DLL or a fault is an exit code (Windows)."""
    if os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)


def find_archives(game_dir) -> dict:
    """{slot: path} for the vanilla archives present in game_dir, any case."""
    if game_dir is None or not Path(game_dir).is_dir():
        return {}
    names = {p.name.lower(): p for p in Path(game_dir).iterdir() if p.is_file()}
    return {slot: str(names[fname]) for slot, fname in ARCHIVES if fname in names}


def ini_text(*, archives: dict, renderer: str, zoom: str = "1:1", width: int = 1280,
             height: int = 720, gamma: str = "1.30") -> str:
    if renderer not in RENDERERS:
        raise RuntimeSetupError(f"renderer must be one of {RENDERERS}, not {renderer!r}")
    if zoom not in ZOOMS:
        raise RuntimeSetupError(f"zoom must be one of {ZOOMS}, not {zoom!r}")
    lines = [f"{slot} = {archives.get(slot, '')}" for slot, _ in ARCHIVES]
    lines += ["mod_dir =",
              "upscale_enabled = NO",
              "full_screen = NO",
              f"screen_width = {int(width)}",
              f"screen_height = {int(height)}",
              f"renderer = {renderer}",
              f"gamma_correction = {gamma}",
              f"default_zoom = {zoom}",
              "mouse_edge_scroll = NO",
              "only_normal_type2 = YES",
              "always_max_layers = YES",
              "workspace_enable = NO"]
    return "\n".join(lines) + "\n"


def assert_windowed(text: str) -> None:
    values = re.findall(r"^\s*full_screen\s*=[ \t]*(\S*)", text, re.M | re.I)
    if not values or any(v.upper() != "NO" for v in values):
        raise RuntimeSetupError("refusing to write an INI without full_screen = NO")


def build(runtime, exe, ini: str, *, device: str = "native", soft_gl_dir=None,
          assets_dir=REPO_ROOT / "assets") -> tuple:
    """Create `runtime` from scratch. Returns (exe in the runtime, environment)."""
    assert_windowed(ini)
    if device not in DEVICES:
        raise RuntimeSetupError(f"device must be one of {DEVICES}, not {device!r}")
    exe, runtime, assets_dir = Path(exe), Path(runtime), Path(assets_dir)
    if not exe.is_file():
        raise RuntimeSetupError(f"executable not found: {exe}")
    if not assets_dir.is_dir():
        raise RuntimeSetupError(f"assets directory not found: {assets_dir}")
    if device != "native" and os.name == "nt":
        if soft_gl_dir is None or not (Path(soft_gl_dir) / "opengl32.dll").is_file():
            raise RuntimeSetupError(f"device {device} needs --soft-gl-dir holding Mesa's "
                                    "opengl32.dll (mesa-dist-win, x86 build)")

    if runtime.exists():
        shutil.rmtree(runtime)
    runtime.mkdir(parents=True)
    shutil.copy2(exe, runtime / exe.name)
    for f in exe.parent.iterdir():
        if f.is_file() and f.suffix.lower() in LIB_SUFFIXES:
            shutil.copy2(f, runtime / f.name)
    shutil.copytree(assets_dir, runtime / "assets", ignore=ASSET_IGNORE)
    (runtime / "appdata").mkdir()

    env = dict(os.environ)
    for k in ("APPDATA", "XDG_CONFIG_HOME", "HOME"):
        env[k] = str(runtime / "appdata")
    if device == "native":
        cfg = exe.parent / "allegro5.cfg"
        if cfg.is_file():
            shutil.copy2(cfg, runtime / "allegro5.cfg")
    else:
        (runtime / "allegro5.cfg").write_text("[graphics]\ndriver=opengl\n", encoding="ascii")
        if os.name == "nt":
            for f in Path(soft_gl_dir).iterdir():
                if f.is_file() and f.suffix.lower() == ".dll":
                    shutil.copy2(f, runtime / f.name)
        else:
            env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        if device == "llvmpipe":
            env["GALLIUM_DRIVER"] = "llvmpipe"
        else:
            env["GALLIUM_DRIVER"] = "d3d12"
            env["MESA_D3D12_DEFAULT_ADAPTER_NAME"] = "Microsoft Basic Render Driver"
    (runtime / INI_NAME).write_text(ini, encoding="ascii")
    return runtime / exe.name, env


def first_map_entry(map_name: str, assets_dir=REPO_ROOT / "assets") -> tuple:
    ini = Path(assets_dir) / f"{map_name}.ini"
    for line in ini.read_text(errors="replace").splitlines():
        line = line.strip()
        if line and not line.startswith(("#", ";")):
            a, b, path = line.split(None, 2)
            return a, b, path.strip()
    raise RuntimeSetupError(f"{ini} has no entries")


def selftest_args(map_name: str, frames: int, shot, *, indices=None, deterministic=False,
                  uncapped=False, zoom=None) -> list:
    args = [f"assets/{map_name}.ini", f"--selftest={int(frames)}", f"--selftest-shot={shot}"]
    if indices:
        args.append(f"--selftest-indices={indices}")
    if deterministic:
        args.append("--deterministic")
    if uncapped:
        args.append("--uncapped")
    if zoom:
        args.append(f"--zoom={zoom}")
    return args


def headless_args(map_name: str, zoom: str, out_image, *, indices=None,
                  assets_dir=REPO_ROOT / "assets") -> list:
    a, b, ds1 = first_map_entry(map_name, assets_dir)
    args = [ds1, a, b, f"--zoom={zoom}", "--headless", str(out_image)]
    if indices:
        args.append(f"--headless-indices={indices}")
    return args


def run(exe, runtime, args: list, env: dict, timeout_s: float = 600.0):
    suppress_error_dialogs()
    return subprocess.run([str(exe), *[str(a) for a in args]], cwd=str(runtime), env=env,
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=timeout_s)


def parse_renderer(text: str) -> Optional[dict]:
    m = RENDERER_LINE.search(text or "")
    if not m:
        return None
    return {"active": m.group(1), "setting": m.group(2), "device": m.group(3)}


def device_ok(device: str, info: Optional[dict]) -> tuple:
    pattern = DEVICE_PATTERNS.get(device)
    if pattern is None:
        return True, "native device"
    if info is None:
        return False, "no renderer line, so the bound device could not be confirmed"
    if not pattern.search(info["device"]):
        return False, f"asked for {device}, the display bound {info['device']!r}"
    return True, info["device"]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("smoke", "headless"):
        p = sub.add_parser(name)
        p.add_argument("--exe", type=Path, required=True)
        p.add_argument("--game-dir", type=Path, default=DEFAULT_GAME_DIR)
        p.add_argument("--renderer", choices=RENDERERS, default="software")
        p.add_argument("--map", default="act1_town")
        p.add_argument("--zoom", choices=ZOOMS, default="1:1")
        p.add_argument("--device", choices=DEVICES, default="native")
        p.add_argument("--soft-gl-dir", type=Path)
        p.add_argument("--out-dir", type=Path, required=True)
        if name == "smoke":
            p.add_argument("--frames", type=int, default=120)
    args = ap.parse_args(argv)

    archives = find_archives(args.game_dir)
    if len(archives) != len(ARCHIVES):
        print(f"sw_runtime: game data unavailable in {args.game_dir} "
              f"(found {sorted(archives)})", file=sys.stderr)
        return 2
    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    try:
        exe, env = build(out / "runtime", args.exe,
                         ini_text(archives=archives, renderer=args.renderer, zoom=args.zoom),
                         device=args.device, soft_gl_dir=args.soft_gl_dir)
    except RuntimeSetupError as exc:
        print(f"sw_runtime: {exc}", file=sys.stderr)
        return 2

    sw_mode = args.renderer == "software"
    if args.cmd == "smoke":
        cmd = selftest_args(args.map, args.frames, out / "shot.png",
                            indices=(out / "frame.idx") if sw_mode else None)
    else:
        cmd = headless_args(args.map, args.zoom, out / "map.bmp",
                            indices=(out / "map.idx") if sw_mode else None)
    proc = run(exe, out / "runtime", cmd, env)
    sys.stdout.write(proc.stdout)
    info = parse_renderer(proc.stdout)
    dev_ok, why = device_ok(args.device, info)
    print(f"sw_runtime: exit {proc.returncode}; renderer {info}; device check: {why}")
    if proc.returncode != 0 or not dev_ok:
        return 1
    if args.renderer != "auto" and (info is None or info["active"] != args.renderer):
        print("sw_runtime: the editor did not run the requested renderer")
        return 1
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
```

Run: `python -m unittest -v test.python.test_sw_runtime`
Expected: `Ran 11 tests ... OK`. (CTest picks the file up automatically as `py_sw_runtime` at the next configure.)

- [ ] **Step 3: Command-line flags**

In `src/structs.h`, in `CMD_LINE_S`, after the `char        * selftest_shot;` member, add:

```c
   /* --selftest-indices=<path>: renderer = software only -- also dump the
      final frame's palette indices and tag plane (canvas_write_dump), for
      scripts/equivalence/renderer_parity.py. NULL means do not. */
   char        * selftest_indices;
   /* --headless-indices=<path>: the same for a --headless software render. */
   char        * headless_indices;
   /* --uncapped: vsync off and no 60 Hz present pacing. For benchmarks: a
      capped frame rate measures the cap, not the renderer. */
   int           uncapped;
```

In `src/main.c`, after `glb_ds1edit.cmd_line.selftest_shot = NULL;` add:

```c
   glb_ds1edit.cmd_line.selftest_indices = NULL;
   glb_ds1edit.cmd_line.headless_indices = NULL;
   glb_ds1edit.cmd_line.uncapped = FALSE;
```

In `src/misc.c`, `misc_cmd_line_parse()`, insert before the branch `else if (strnicmp(argv[i], "--selftest-shot=", 16) == 0` (it must come before both `--selftest` branches, which match on a prefix):

```c
      else if (strnicmp(argv[i], "--selftest-indices=", 19) == 0)
      {
         const char *eq = strchr(argv[i], '=');
         glb_ds1edit.cmd_line.selftest_indices = (eq[1] != 0) ? (char *) (eq + 1) : NULL;
         if (glb_ds1edit.cmd_line.selftest_indices == NULL)
         {
            printf("misc_cmd_line_parse(), error : --selftest-indices needs a filename\n");
            return -1;
         }
      }
      else if (strnicmp(argv[i], "--headless-indices=", 19) == 0)
      {
         const char *eq = strchr(argv[i], '=');
         glb_ds1edit.cmd_line.headless_indices = (eq[1] != 0) ? (char *) (eq + 1) : NULL;
         if (glb_ds1edit.cmd_line.headless_indices == NULL)
         {
            printf("misc_cmd_line_parse(), error : --headless-indices needs a filename\n");
            return -1;
         }
      }
      else if (stricmp(argv[i], "--uncapped") == 0)
      {
         glb_ds1edit.cmd_line.uncapped = TRUE;
      }
```

In `src/main.c`, `ds1edit_try_create_display()`, replace

```c
   al_set_new_display_option(ALLEGRO_VSYNC, 1, ALLEGRO_SUGGEST);
```

with

```c
   /* ALLEGRO_VSYNC: 1 = on, 2 = off. --uncapped turns it off so a benchmark
      measures the renderer rather than the refresh rate. */
   al_set_new_display_option(ALLEGRO_VSYNC, glb_ds1edit.cmd_line.uncapped ? 2 : 1,
                             ALLEGRO_SUGGEST);
```

- [ ] **Step 4: 60 Hz present pacing in `misc_draw_screen()`**

In `src/misc.c`, add after the existing includes:

```c
#include "render/present.h"
#include "render/render_select.h"
```

and in `misc_draw_screen()` replace

```c
   al_flip_display();

   glb_present_ms = al_get_time() * 1000.0 - start_ms;
```

with

```c
   /* Software path: presents are capped at 60 Hz without relying on vsync,
      so a device without it does not spin (spec, Frame data flow). With
      vsync on, the flip already paces; --uncapped turns both off. */
   if ((render_select_active() == DS1EDIT_RENDERER_SOFTWARE) &&
       !glb_ds1edit.cmd_line.uncapped &&
       (al_get_display_option(a5_display, ALLEGRO_VSYNC) != 1))
   {
      static double next_present_s = 0.0;
      double wait_s = present_pace_delay(al_get_time(), &next_present_s, 1.0 / 60.0);
      if (wait_s > 0.0)
         al_rest(wait_s);
   }
   al_flip_display();

   glb_present_ms = al_get_time() * 1000.0 - start_ms;
```

- [ ] **Step 5: The software branch in `wpreview_draw_tiles()`**

In `src/render/preview.c`:

1. After `#include "core/project.h"` add:

```c
#include "render/canvas.h"
#include "render/map_sw.h"
#include "render/present.h"
#include "render/render_select.h"
```

2. After `static int wpreview_old_pal = -1;` add:

```c
/* Software path timing of the last frame, for the --selftest report. */
double glb_sw_compose_ms = 0.0;
double glb_sw_upload_ms  = 0.0;
```

3. Immediately before `void wpreview_draw_tiles(int ds1_idx)` (keep its `// draw all tiles of this ds1` banner above the new function's own banner), add:

```c
// ==========================================================================
// renderer = software: passes 1-6 composed into the 8-bit canvas, uploaded
// into screen_buff with one write-only lock, then the labels the passes
// queued. Everything after pass 6 is still drawn by Allegro on top (phase 1
// of the software renderer; the UI port is phase 2).
static void wpreview_compose_software(int ds1_idx, int cx, int cy)
{
   static int warned = 0;
   MAP_SW_CONTEXT ctx;
   DS1_CANVAS *cv;
   double t0, t1;
   int i, w, h;

   glb_sw_compose_ms = 0.0;
   glb_sw_upload_ms = 0.0;
   w = al_get_bitmap_width(glb_ds1edit.screen_buff);
   h = al_get_bitmap_height(glb_ds1edit.screen_buff);
   cv = map_sw_frame_canvas(w, h, glb_ds1edit.cmd_line.selftest_indices != NULL);
   if ((cv == NULL) ||
       !map_sw_prepare_context(&ctx, wpreview_old_pal,
                               glb_ds1edit.win_preview.x0 + a5_mouse_x,
                               glb_ds1edit.win_preview.y0 + a5_mouse_y))
   {
      if (!warned)
      {
         warned = 1;
         fprintf(stderr, "render: software compositor unavailable (canvas %dx%d, "
                         "palette %d); the map is not drawn\n", w, h, wpreview_old_pal);
      }
      return;
   }

   t0 = al_get_time();
   map_sw_draw_view(cv, ds1_idx, &ctx, cx, cy);
   t1 = al_get_time();
   present_canvas_to_bitmap(glb_ds1edit.screen_buff, cv,
                            &glb_ds1edit.vga_pal[wpreview_old_pal]);
   glb_sw_compose_ms = (t1 - t0) * 1000.0;
   glb_sw_upload_ms = (al_get_time() - t1) * 1000.0;

   al_set_target_bitmap(glb_ds1edit.screen_buff);
   for (i = 0; i < map_sw_text_count(); i++)
   {
      const MAP_SW_TEXT *t = map_sw_text_at(i);
      wpreview_textprintf(t->x, t->y, t->color, "%s", t->text);
   }
}

```

4. In `wpreview_draw_tiles()`, in both palette branches, change the line `dt1_rebuild_bitmaps_from_cache(a5_current_palette);` to

```c
         if (render_select_active() != DS1EDIT_RENDERER_SOFTWARE)
            dt1_rebuild_bitmaps_from_cache(a5_current_palette);
```

(The software path reads palette-independent indices; its present table follows the palette by itself. Rebuilding every tile bitmap on an act change is pure cost there.)

5. Immediately before the line `   // Pass 1: Base terrain (lower walls, floors, tile shadows)` insert:

```c
   if (render_select_active() == DS1EDIT_RENDERER_SOFTWARE)
   {
      wpreview_compose_software(ds1_idx, cx, cy);
      goto overlays;
   }

```

and immediately before the line `   // Pass 7: Walkable info overlay (optional debug view)` insert the label:

```c
overlays:
```

(The skipped region holds GPU passes 1-6 and the over-floor grid, which `map_sw_draw_view` draws itself; it declares no variables, so the jump is well-defined C99.)

6. After the two lines that print the editing mode,

```c
   wpreview_textprintf(
       glb_config.screen.width - 56, glb_config.screen.height - 8, 108, "%s", mode);
```

add the status-bar half of the fallback warning (stderr got it once already from `present.c`):

```c
   if ((render_select_active() == DS1EDIT_RENDERER_SOFTWARE) && (present_warning() != NULL))
      wpreview_textprintf(
          glb_config.screen.width - 400, glb_config.screen.height - 8, 98,
          "slow present (see stderr.txt)");
```

7. At the end of the file, before the closing `#ifdef WIN32` / `#pragma warning(pop)` / `#endif`, add the headless software render:

```c
// ==========================================================================
// renderer = software, --headless: the whole map at the current zoom into an
// index canvas with a tag plane, expanded through the act palette into a
// memory bitmap and saved. out_indices (may be NULL) also gets the indices and
// tags (canvas_write_dump) for scripts/equivalence/renderer_parity.py. Like
// the GPU big screenshot it draws the map and its labels, not the editor
// overlays that are off by default (walkable info, grid, paths, object info).
// Returns 0 on success, -1 on failure.
int wpreview_draw_tiles_big_screenshot_sw(int ds1_idx, const char *out_image,
                                          const char *out_indices)
{
   int32_t minx, miny, maxx, maxy, w, h;
   DS1_CANVAS cv;
   MAP_SW_CONTEXT ctx;
   ALLEGRO_BITMAP *bmp = NULL;
   WIN_PREVIEW_S saved_view = glb_ds1edit.win_preview;
   int saved_w = glb_config.screen.width, saved_h = glb_config.screen.height;
   int prev_flags = al_get_new_bitmap_flags(), prev_format = al_get_new_bitmap_format();
   int pal, i, rc = -1;

   if (!map_sw_full_bounds(ds1_idx, &minx, &miny, &maxx, &maxy))
      return -1;
   w = maxx - minx;
   h = maxy - miny;
   if ((w <= 0) || (h <= 0) || !canvas_create(&cv, w, h, 1))
      return -1;

   pal = (glb_ds1edit.cmd_line.force_pal_num == -1)
            ? palette_resolve_index(glb_ds1[ds1_idx].txt_act, glb_ds1[ds1_idx].act)
            : glb_ds1edit.cmd_line.force_pal_num - 1;
   a5_current_palette = &glb_ds1edit.vga_pal[pal];

   glb_ds1edit.win_preview.x0 = minx;
   glb_ds1edit.win_preview.y0 = miny;
   glb_ds1edit.win_preview.w = w;
   glb_ds1edit.win_preview.h = h;
   glb_config.screen.width = w;
   glb_config.screen.height = h;

   if (map_sw_prepare_context(&ctx, pal, 0, 0))
   {
      map_sw_draw_view(&cv, ds1_idx, &ctx, -1, -1);

      al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
      al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE);
      bmp = al_create_bitmap(w, h);
      al_set_new_bitmap_flags(prev_flags);
      al_set_new_bitmap_format(prev_format);

      if ((bmp != NULL) && (present_canvas_to_bitmap(bmp, &cv, a5_current_palette) > 0))
      {
         al_set_target_bitmap(bmp);
         for (i = 0; i < map_sw_text_count(); i++)
         {
            const MAP_SW_TEXT *t = map_sw_text_at(i);
            wpreview_textprintf(t->x, t->y, t->color, "%s", t->text);
         }
         rc = al_save_bitmap(out_image, bmp) ? 0 : -1;
         if ((rc == 0) && (out_indices != NULL) && !canvas_write_dump(&cv, out_indices))
            rc = -1;
         al_set_target_bitmap(NULL);
      }
   }

   if (bmp != NULL)
      al_destroy_bitmap(bmp);
   canvas_destroy(&cv);
   glb_ds1edit.win_preview = saved_view;
   glb_config.screen.width = saved_w;
   glb_config.screen.height = saved_h;
   return rc;
}
```

In `src/render/preview.h`, add before `#endif`:

```c
extern double glb_sw_compose_ms;   /* last software frame: compose time, ms */
extern double glb_sw_upload_ms;    /* last software frame: present upload, ms */
int  wpreview_draw_tiles_big_screenshot_sw(int ds1_idx, const char *out_image,
                                           const char *out_indices);
```

- [ ] **Step 6: The headless software branch in `main.c`**

In the headless block of `src/main.c`, replace

```c
      // render complete map
      old_screen_buff = glb_ds1edit.screen_buff;
      if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
      {
         al_save_bitmap(glb_ds1edit.cmd_line.headless_output,
                        glb_ds1edit.screen_buff);
         printf("headless mode : saved %i x %i screenshot\n",
                al_get_bitmap_width(glb_ds1edit.screen_buff),
                al_get_bitmap_height(glb_ds1edit.screen_buff));
         al_destroy_bitmap(glb_ds1edit.screen_buff);
      }
      else
      {
         printf("headless mode : render failed (empty map?)\n");
      }
      glb_ds1edit.screen_buff = old_screen_buff;
```

with

```c
      printf("renderer: %s (renderer = %s, device \"none\")\n",
             render_select_name(render_select_active()),
             render_select_name(glb_config.renderer));

      if (render_select_active() == DS1EDIT_RENDERER_SOFTWARE)
      {
         if (wpreview_draw_tiles_big_screenshot_sw(
                ds1_idx, glb_ds1edit.cmd_line.headless_output,
                glb_ds1edit.cmd_line.headless_indices) == 0)
            printf("headless mode : saved software render\n");
         else
            printf("headless mode : render failed (empty map?)\n");
      }
      else
      {
         if (glb_ds1edit.cmd_line.headless_indices != NULL)
            printf("headless mode : --headless-indices is only written by renderer = software\n");

         // render complete map
         old_screen_buff = glb_ds1edit.screen_buff;
         if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
         {
            al_save_bitmap(glb_ds1edit.cmd_line.headless_output,
                           glb_ds1edit.screen_buff);
            printf("headless mode : saved %i x %i screenshot\n",
                   al_get_bitmap_width(glb_ds1edit.screen_buff),
                   al_get_bitmap_height(glb_ds1edit.screen_buff));
            al_destroy_bitmap(glb_ds1edit.screen_buff);
         }
         else
         {
            printf("headless mode : render failed (empty map?)\n");
         }
         glb_ds1edit.screen_buff = old_screen_buff;
      }
```

- [ ] **Step 7: Build, run the portable suite**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release
"$CTEST" --test-dir tmp/build-sw -C Release -L portable --output-on-failure
```
Expected: the whole tree builds (`ds1edit` writes `bin/ds1edit.exe`); every `portable` test passes, including `py_sw_runtime`.

- [ ] **Step 8: Game-data smoke (local only, vanilla 1.13c, windowed)**

```bash
python -m scripts.equivalence.sw_runtime smoke --exe bin/ds1edit.exe --renderer software --map act1_town --frames 120 --out-dir tmp/sw-smoke
python -m scripts.equivalence.sw_runtime smoke --exe bin/ds1edit.exe --renderer gpu --map act1_town --frames 120 --out-dir tmp/sw-smoke-gpu
python -m scripts.equivalence.sw_runtime headless --exe bin/ds1edit.exe --renderer software --map act1_town --zoom 1:4 --out-dir tmp/sw-headless
```
Expected, software smoke: a `renderer: software (renderer = software, device "...")` line, `selftest: rendered 120 frame(s), exiting`, `selftest: wrote ...shot.png`, `selftest: render avg ... ms/frame`, final line `sw_runtime: exit 0; ...`, exit status 0. GPU smoke: the same with `renderer: gpu`. Headless: `headless mode : saved software render`, `tmp/sw-headless/map.bmp` and `map.idx` exist.

Open `tmp/sw-smoke/shot.png` and `tmp/sw-smoke-gpu/shot.png` side by side (any image viewer, or the Read tool): the town is drawn in both, HUD bars on top. Any missing layer, offset sprite or wrong palette is a bug in Tasks 9-11; fix before continuing. `tmp/sw-smoke/runtime/stderr.txt` must not contain `present:` (a fallback) on a machine with a working driver.

- [ ] **Step 9: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/preview.c src/render/preview.h src/main.c src/misc.c src/structs.h scripts/equivalence/sw_runtime.py test/python/test_sw_runtime.py
git commit -m "Draw the map through the software compositor when renderer = software"
```

---

### Task 12: Frame-time evidence and a deterministic windowed session

The spec's performance record is "frame-time histogram, compose vs present time, peak working set", and its parity gate renders maps "windowed (never fullscreen)". A windowed run is only comparable between renderers if nothing in it depends on time or the pointer, so this task adds `--deterministic` alongside the frame-time report.

**Files:**
- Create: `src/render/frame_stats.h`, `src/render/frame_stats.c`, `test/test_render_frame_stats.c`
- Modify: `src/ui/interface.c`, `src/structs.h` (`CMD_LINE_S`), `src/main.c`, `src/misc.c`, `src/render/preview.c` (fps text)
- Modify: `test/cmake/software_renderer.cmake`, `CMakeLists.txt` (`SOURCES`)

**Interfaces:**
- Consumes: `glb_sw_compose_ms`, `glb_sw_upload_ms` (Task 11); `glb_present_ms` (`src/misc.h`); `render_select_active`, `render_select_name` (Task 7); `present_last_path` (Task 6); `map_sw_last_canvas` (Task 9); `canvas_write_dump` (Task 1).
- Produces (`src/render/frame_stats.h`):
  ```c
  #define FRAME_STATS_BUCKETS 9
  extern const double frame_stats_bucket_upper_ms[FRAME_STATS_BUCKETS - 1];   /* 4, 8, 12, 16.7, 25, 33.4, 50, 100 */
  typedef struct FRAME_STATS { double *ms; int count, cap; } FRAME_STATS;
  int    frame_stats_init(FRAME_STATS *s, int cap);           /* 1 ok */
  void   frame_stats_free(FRAME_STATS *s);
  void   frame_stats_add(FRAME_STATS *s, double ms);          /* ignored beyond cap */
  double frame_stats_percentile(const FRAME_STATS *s, double p);   /* nearest rank; 0 when empty */
  void   frame_stats_histogram(const FRAME_STATS *s, int counts[FRAME_STATS_BUCKETS]);
  int    frame_stats_format(const FRAME_STATS *frame, const FRAME_STATS *compose,
                            const FRAME_STATS *present, char *out, size_t cap);   /* 1 ok */
  ```
  The two report lines (parsed by `benchmarks.py`, Task 14), exactly:
  ```
  selftest: frametimes n=<int> frame_median_ms=<%.3f> frame_p95_ms=<%.3f> compose_median_ms=<%.3f> present_median_ms=<%.3f> fps_median=<%.1f>
  selftest: histogram_ms lt4=<n> lt8=<n> lt12=<n> lt16_7=<n> lt25=<n> lt33_4=<n> lt50=<n> lt100=<n> ge100=<n>
  ```
  followed by `selftest: renderer=<gpu|software> present=<locked|memory-target|fallback|none>`. Definitions: *frame* = wall time of `wpreview_draw_tiles()` including `misc_draw_screen()` and its flip; *compose* = `map_sw_draw_view` (software) or frame minus present (GPU); *present* = canvas upload plus `misc_draw_screen()` (software) or `misc_draw_screen()` (GPU).
  `CMD_LINE_S.deterministic` / `--deterministic`: the 25 Hz tick no longer advances animation (floors and objects stay on their seeded frames), the pointer is pinned to window (0,0), and the HUD prints `fps=0`.

- [ ] **Step 1: Write the failing test**

Create `test/test_render_frame_stats.c`:

```c
/* Frame-time statistics for the --selftest report (frame_stats.c). Unity is
 * built without double assertions here, so values are compared by hand. */

#include <string.h>

#include "unity/unity.h"
#include "render/frame_stats.h"

static FRAME_STATS s;

void setUp(void) { TEST_ASSERT_EQUAL_INT(1, frame_stats_init(&s, 16)); }
void tearDown(void) { frame_stats_free(&s); }

static int near(double a, double b) { return (a - b < 1e-9) && (b - a < 1e-9); }

void test_percentiles_use_nearest_rank(void)
{
   int i;
   for (i = 10; i >= 1; i--)
      frame_stats_add(&s, (double) i);
   TEST_ASSERT_TRUE(near(5.0, frame_stats_percentile(&s, 50.0)));
   TEST_ASSERT_TRUE(near(10.0, frame_stats_percentile(&s, 95.0)));
   TEST_ASSERT_TRUE(near(1.0, frame_stats_percentile(&s, 0.0)));
   TEST_ASSERT_TRUE(near(10.0, frame_stats_percentile(&s, 100.0)));
}

void test_empty_stats_report_zero(void)
{
   TEST_ASSERT_TRUE(near(0.0, frame_stats_percentile(&s, 50.0)));
}

void test_samples_beyond_capacity_are_ignored(void)
{
   int i;
   for (i = 0; i < 40; i++)
      frame_stats_add(&s, 1.0);
   TEST_ASSERT_EQUAL_INT(16, s.count);
}

void test_histogram_bucket_edges(void)
{
   int c[FRAME_STATS_BUCKETS];
   frame_stats_add(&s, 3.99);    /* lt4    */
   frame_stats_add(&s, 4.0);     /* lt8    */
   frame_stats_add(&s, 16.69);   /* lt16_7 */
   frame_stats_add(&s, 16.7);    /* lt25   */
   frame_stats_add(&s, 100.0);   /* ge100  */
   frame_stats_histogram(&s, c);
   TEST_ASSERT_EQUAL_INT(1, c[0]);
   TEST_ASSERT_EQUAL_INT(1, c[1]);
   TEST_ASSERT_EQUAL_INT(0, c[2]);
   TEST_ASSERT_EQUAL_INT(1, c[3]);
   TEST_ASSERT_EQUAL_INT(1, c[4]);
   TEST_ASSERT_EQUAL_INT(1, c[8]);
}

void test_report_lines_are_exact(void)
{
   FRAME_STATS compose, present;
   char out[512];

   TEST_ASSERT_EQUAL_INT(1, frame_stats_init(&compose, 4));
   TEST_ASSERT_EQUAL_INT(1, frame_stats_init(&present, 4));
   frame_stats_add(&s, 10.0);
   frame_stats_add(&s, 20.0);
   frame_stats_add(&compose, 5.0);
   frame_stats_add(&compose, 6.0);
   frame_stats_add(&present, 1.0);
   frame_stats_add(&present, 2.0);
   TEST_ASSERT_EQUAL_INT(1, frame_stats_format(&s, &compose, &present, out, sizeof(out)));
   TEST_ASSERT_EQUAL_STRING(
      "selftest: frametimes n=2 frame_median_ms=10.000 frame_p95_ms=20.000 "
      "compose_median_ms=5.000 present_median_ms=1.000 fps_median=100.0\n"
      "selftest: histogram_ms lt4=0 lt8=0 lt12=1 lt16_7=0 lt25=1 lt33_4=0 lt50=0 "
      "lt100=0 ge100=0\n", out);
   TEST_ASSERT_EQUAL_INT(0, frame_stats_format(&s, &compose, &present, out, 20));
   frame_stats_free(&compose);
   frame_stats_free(&present);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_percentiles_use_nearest_rank);
   RUN_TEST(test_empty_stats_report_zero);
   RUN_TEST(test_samples_beyond_capacity_are_ignored);
   RUN_TEST(test_histogram_bucket_edges);
   RUN_TEST(test_report_lines_are_exact);
   return UNITY_END();
}
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 12: frame-time statistics -------------------------------------------
ds1edit_sw_pure_test(test_render_frame_stats test_render_frame_stats.c ${_sw}/frame_stats.c)
ds1edit_sw_add_test(render_frame_stats test_render_frame_stats)
```

- [ ] **Step 2: Run it to make sure it fails**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
```
Expected: configure FAILS with `Cannot find source file: .../src/render/frame_stats.c`.

- [ ] **Step 3: Write `frame_stats`**

Create `src/render/frame_stats.h`:

```c
/* Frame-time samples and the --selftest report lines (parsed by
 * scripts/equivalence/benchmarks.py). No Allegro dependency. */

#ifndef DS1EDIT_RENDER_FRAME_STATS_H
#define DS1EDIT_RENDER_FRAME_STATS_H

#include <stddef.h>

#define FRAME_STATS_BUCKETS 9

extern const double frame_stats_bucket_upper_ms[FRAME_STATS_BUCKETS - 1];

typedef struct FRAME_STATS
{
   double *ms;
   int     count, cap;
} FRAME_STATS;

int    frame_stats_init(FRAME_STATS *s, int cap);
void   frame_stats_free(FRAME_STATS *s);
void   frame_stats_add(FRAME_STATS *s, double ms);
double frame_stats_percentile(const FRAME_STATS *s, double p);
void   frame_stats_histogram(const FRAME_STATS *s, int counts[FRAME_STATS_BUCKETS]);
int    frame_stats_format(const FRAME_STATS *frame, const FRAME_STATS *compose,
                          const FRAME_STATS *present, char *out, size_t cap);

#endif
```

Create `src/render/frame_stats.c`:

```c
/* See frame_stats.h. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/frame_stats.h"

const double frame_stats_bucket_upper_ms[FRAME_STATS_BUCKETS - 1] = {
   4.0, 8.0, 12.0, 16.7, 25.0, 33.4, 50.0, 100.0
};

int frame_stats_init(FRAME_STATS *s, int cap)
{
   if (s == NULL)
      return 0;
   memset(s, 0, sizeof(*s));
   if (cap <= 0)
      return 0;
   s->ms = (double *) malloc((size_t) cap * sizeof(double));
   if (s->ms == NULL)
      return 0;
   s->cap = cap;
   return 1;
}

void frame_stats_free(FRAME_STATS *s)
{
   if (s == NULL)
      return;
   free(s->ms);
   memset(s, 0, sizeof(*s));
}

void frame_stats_add(FRAME_STATS *s, double ms)
{
   if ((s == NULL) || (s->ms == NULL) || (s->count >= s->cap))
      return;
   s->ms[s->count++] = ms;
}

static int cmp_double(const void *a, const void *b)
{
   double x = *(const double *) a, y = *(const double *) b;
   return (x > y) - (x < y);
}

double frame_stats_percentile(const FRAME_STATS *s, double p)
{
   double *tmp, v;
   long    rank;

   if ((s == NULL) || (s->count <= 0))
      return 0.0;
   tmp = (double *) malloc((size_t) s->count * sizeof(double));
   if (tmp == NULL)
      return 0.0;
   memcpy(tmp, s->ms, (size_t) s->count * sizeof(double));
   qsort(tmp, (size_t) s->count, sizeof(double), cmp_double);
   rank = (long) ceil(p / 100.0 * (double) s->count);
   if (rank < 1)
      rank = 1;
   if (rank > s->count)
      rank = s->count;
   v = tmp[rank - 1];
   free(tmp);
   return v;
}

void frame_stats_histogram(const FRAME_STATS *s, int counts[FRAME_STATS_BUCKETS])
{
   int i, b;

   memset(counts, 0, sizeof(int) * FRAME_STATS_BUCKETS);
   if (s == NULL)
      return;
   for (i = 0; i < s->count; i++)
   {
      b = 0;
      while ((b < FRAME_STATS_BUCKETS - 1) && (s->ms[i] >= frame_stats_bucket_upper_ms[b]))
         b++;
      counts[b]++;
   }
}

int frame_stats_format(const FRAME_STATS *frame, const FRAME_STATS *compose,
                       const FRAME_STATS *present, char *out, size_t cap)
{
   int    c[FRAME_STATS_BUCKETS], n;
   double med = frame_stats_percentile(frame, 50.0);

   if ((frame == NULL) || (out == NULL) || (cap == 0))
      return 0;
   frame_stats_histogram(frame, c);
   n = snprintf(out, cap,
                "selftest: frametimes n=%d frame_median_ms=%.3f frame_p95_ms=%.3f "
                "compose_median_ms=%.3f present_median_ms=%.3f fps_median=%.1f\n"
                "selftest: histogram_ms lt4=%d lt8=%d lt12=%d lt16_7=%d lt25=%d "
                "lt33_4=%d lt50=%d lt100=%d ge100=%d\n",
                frame->count, med, frame_stats_percentile(frame, 95.0),
                frame_stats_percentile(compose, 50.0), frame_stats_percentile(present, 50.0),
                (med > 0.0) ? 1000.0 / med : 0.0,
                c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8]);
   return (n > 0) && ((size_t) n < cap);
}
```

Add `src/render/frame_stats.c` to `SOURCES` in `CMakeLists.txt` after `src/render/map_sw.c`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_frame_stats
"$CTEST" --test-dir tmp/build-sw -C Release -R "^render_frame_stats$" --output-on-failure
```
Expected: `5 Tests 0 Failures 0 Ignored`.

- [ ] **Step 5: `--deterministic`**

In `src/structs.h`, `CMD_LINE_S`, after the `int uncapped;` member from Task 11, add:

```c
   /* --deterministic: animation clock frozen, pointer pinned to (0,0), fps
      shown as 0 -- so two windowed runs (one per renderer) draw the same
      frame. For the parity harness. */
   int           deterministic;
```

In `src/main.c`, after `glb_ds1edit.cmd_line.uncapped = FALSE;` add `glb_ds1edit.cmd_line.deterministic = FALSE;`.

In `src/misc.c`, after the `--uncapped` branch added in Task 11, add:

```c
      else if (stricmp(argv[i], "--deterministic") == 0)
      {
         glb_ds1edit.cmd_line.deterministic = TRUE;
      }
```

In `src/ui/interface.c`, replace

```c
               if (event.timer.source == a5_tick_timer)
                  glb_ds1edit.ticks_elapsed++;
```

with

```c
               if (event.timer.source == a5_tick_timer)
               {
                  /* --deterministic: the input layer still counts the tick
                     (the selftest checks it), animation does not see it. */
                  if (!glb_ds1edit.cmd_line.deterministic)
                     glb_ds1edit.ticks_elapsed++;
               }
```

and replace

```c
      input_end_frame();
```

(the call inside the main loop, under `// poll input state`) with

```c
      input_end_frame();
      if (glb_ds1edit.cmd_line.deterministic)
      {
         a5_ms_state.x = 0;
         a5_ms_state.y = 0;
      }
```

In `src/render/preview.c`, in the HUD, replace

```c
       "  fps=%i",
       glb_ds1edit.old_fps);
```

with

```c
       "  fps=%i",
       glb_ds1edit.cmd_line.deterministic ? 0 : glb_ds1edit.old_fps);
```

- [ ] **Step 6: Collect and report frame times in `--selftest`**

In `src/ui/interface.c`:

1. Add after `#include "ui/input.h"`:

```c
#include "render/canvas.h"
#include "render/frame_stats.h"
#include "render/map_sw.h"
#include "render/present.h"
#include "render/render_select.h"
```

2. Add to the locals of `interfac_user_handler()` (after `int switchbench_exit_in = 0;`):

```c
   FRAME_STATS st_frame, st_compose, st_present;
   int st_ready = 0;
```

3. After the `// init` block's `done = FALSE;`, add:

```c
   if (glb_ds1edit.cmd_line.selftest_frames > 0)
      st_ready = frame_stats_init(&st_frame, glb_ds1edit.cmd_line.selftest_frames) &&
                 frame_stats_init(&st_compose, glb_ds1edit.cmd_line.selftest_frames) &&
                 frame_stats_init(&st_present, glb_ds1edit.cmd_line.selftest_frames);
```

4. Replace

```c
      glb_ds1edit.fps++;
      selftest_render_ms += perf_now_ms() - section_start_ms - glb_present_ms;
```

with

```c
      glb_ds1edit.fps++;
      selftest_render_ms += perf_now_ms() - section_start_ms - glb_present_ms;
      if (st_ready)
      {
         double frame_ms = perf_now_ms() - section_start_ms;
         frame_stats_add(&st_frame, frame_ms);
         if (render_select_active() == DS1EDIT_RENDERER_SOFTWARE)
         {
            frame_stats_add(&st_compose, glb_sw_compose_ms);
            frame_stats_add(&st_present, glb_sw_upload_ms + glb_present_ms);
         }
         else
         {
            frame_stats_add(&st_compose, frame_ms - glb_present_ms);
            frame_stats_add(&st_present, glb_present_ms);
         }
      }
```

5. After

```c
            fprintf(stdout, "selftest: render avg %.2f ms/frame\n",
                    selftest_render_ms / selftest_rendered);
```

insert

```c
            if (st_ready)
            {
               char report[512];
               if (frame_stats_format(&st_frame, &st_compose, &st_present,
                                      report, sizeof(report)))
                  fputs(report, stdout);
               fprintf(stdout, "selftest: renderer=%s present=%s\n",
                       render_select_name(render_select_active()), present_last_path());
               if (glb_ds1edit.cmd_line.selftest_indices != NULL)
               {
                  const DS1_CANVAS *cv = map_sw_last_canvas();
                  if ((render_select_active() == DS1EDIT_RENDERER_SOFTWARE) && (cv != NULL) &&
                      canvas_write_dump(cv, glb_ds1edit.cmd_line.selftest_indices))
                     fprintf(stdout, "selftest: wrote %s\n",
                             glb_ds1edit.cmd_line.selftest_indices);
                  else
                     fprintf(stdout, "selftest: could not write %s\n",
                             glb_ds1edit.cmd_line.selftest_indices);
               }
            }
```

(It sits before the 250 ms check, so a run that fails that check still prints its evidence. If the concurrent GPU work has changed the `render avg` lines, insert at the equivalent point, after the shot is written and before any `exit(1)`.)

6. Before the final `perf_print_summary();` of `interfac_user_handler()`, add:

```c
   if (st_ready)
   {
      frame_stats_free(&st_frame);
      frame_stats_free(&st_compose);
      frame_stats_free(&st_present);
   }
```

- [ ] **Step 7: Build and check the report on a real map (local game-data lane)**

```bash
"$CMAKE" --build tmp/build-sw --config Release
"$CTEST" --test-dir tmp/build-sw -C Release -L portable --output-on-failure
python -m scripts.equivalence.sw_runtime smoke --exe bin/ds1edit.exe --renderer software --map act1_town --frames 120 --out-dir tmp/sw-smoke
```
Expected: all portable tests pass; the smoke output contains one `selftest: frametimes n=120 ...` line, one `selftest: histogram_ms ...` line whose nine counts sum to 120, `selftest: renderer=software present=locked`, and `selftest: wrote ...frame.idx`.

- [ ] **Step 8: Commit (only when the maintainer has asked for commits)**

```bash
git add src/render/frame_stats.h src/render/frame_stats.c test/test_render_frame_stats.c test/cmake/software_renderer.cmake src/ui/interface.c src/structs.h src/main.c src/misc.c src/render/preview.c CMakeLists.txt
git commit -m "Report frame times from --selftest, and add --deterministic for comparable windows"
```

---

### Task 13: Six-map parity against the GPU path (phase 1 gate, part 1)

**Files:**
- Create: `scripts/equivalence/renderer_parity.py`, `test/python/test_renderer_parity.py`

**Interfaces:**
- Consumes: `sw_runtime` (Task 11): `find_archives`, `ini_text`, `build`, `selftest_args`, `headless_args`, `run`, `parse_renderer`, `DEFAULT_GAME_DIR`; `contracts.STATUSES`, `contracts.seal` (`scripts/equivalence/contracts.py`); the `DS1IDX1` dump (Task 1); `--headless-indices`, `--selftest-indices`, `--deterministic` (Tasks 11-12).
- Produces (`scripts/equivalence/renderer_parity.py`):
  ```python
  CORE_MAPS = ("act1_town", "act1_cave", "act2_desert", "act3_jungle", "act4_fortress", "act5_town")
  CONTRACT_RULES: dict[str, str]          # named rules; TAG_RULES maps tag 1/2 -> rule name
  def read_dump(path) -> tuple             # (w, h, indices: bytes, tags: bytes | None)
  def compare(gpu_png, sw_png, dump) -> dict      # {"status", "stage", "compared", "differing", "excluded": {rule: n}, "first_diff"}
  def parity_case(map_name, mode, gpu_png, sw_png, dump, extra=None) -> dict   # sealed ds1edit-style case record
  def exit_code(cases) -> int              # 0 all pass, 1 any mismatch, 2 anything unavailable
  def main(argv=None) -> int               # `run` subcommand
  ```

Parity rule (spec, Testing): "RGB compared with the GPU path; known differences (GPU bilinear zoom-out) are named contract rules, not tolerances." So every pixel is either compared exactly or excluded by a named rule, with the count recorded:

| Rule | Why it exists | How the excluded pixels are verified instead |
| --- | --- | --- |
| `colour-table-blend` | The GPU path blends RGB continuously (alpha tint, additive, multiply, shadow darkening); the software path snaps to the palette through D2CMP tables, as the game does. | `test_render_blit`, `test_render_sw_tables`, `test_render_map_sw*` in the index domain; tag 1. |
| `editor-line-raster` | Bresenham versus the GPU's line rasterizer (markers, grid). | `test_render_canvas`; tag 2. |
| `zoom-decimation` | Zooms other than 1:1 are not RGB-compared: nearest-pixel copies versus draw-time filtering. Parity runs at 1:1. | `test_render_rle`, `test_render_spritecache`. |
| `light-mode-per-cell` | Night mode lights per cell (software) versus per corner (GPU). Parity runs in tile mode. | `test_render_map_sw` light test. |
| `palshift-zero` | A COF pixel whose palette shift maps it to index 0 is transparent on the software path (index 0 is the seam's transparency key) but drawn on the GPU path. Shows up as differing pixels; any such pixel is reported, never silently passed. | Inspection of the reported `first_diff`. |

- [ ] **Step 1: Write the failing tests**

Create `test/python/test_renderer_parity.py`:

```python
"""Tests for scripts/equivalence/renderer_parity.py (synthetic images and dumps)."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PIL import Image  # noqa: E402

from scripts.equivalence import renderer_parity as rp  # noqa: E402


def write_dump(path, w, h, indices, tags=None):
    with open(path, "wb") as f:
        f.write(b"DS1IDX1\0" + struct.pack("<II", w, h) + bytes([1 if tags else 0]))
        f.write(bytes(indices))
        if tags:
            f.write(bytes(tags))


def write_png(path, w, h, rgb):
    img = Image.new("RGB", (w, h))
    img.putdata(rgb)
    img.save(path)


class CompareTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = Path(self.tmp.name)
        self.rgb = [(10, 20, 30), (40, 50, 60), (70, 80, 90), (1, 2, 3)]
        write_png(self.d / "gpu.png", 2, 2, self.rgb)

    def tearDown(self):
        self.tmp.cleanup()

    def test_read_dump(self):
        write_dump(self.d / "a.idx", 2, 1, [5, 6], [0, 1])
        self.assertEqual((2, 1, b"\x05\x06", b"\x00\x01"), rp.read_dump(self.d / "a.idx"))
        write_dump(self.d / "b.idx", 1, 1, [7])
        self.assertEqual((1, 1, b"\x07", None), rp.read_dump(self.d / "b.idx"))
        (self.d / "bad.idx").write_bytes(b"NOTADUMP")
        with self.assertRaises(ValueError):
            rp.read_dump(self.d / "bad.idx")

    def test_identical_images_pass(self):
        write_png(self.d / "sw.png", 2, 2, self.rgb)
        write_dump(self.d / "sw.idx", 2, 2, [1, 2, 3, 4], [0, 0, 0, 0])
        r = rp.compare(self.d / "gpu.png", self.d / "sw.png", self.d / "sw.idx")
        self.assertEqual("pass", r["status"])
        self.assertEqual(4, r["compared"])
        self.assertEqual(0, r["differing"])

    def test_a_differing_exact_pixel_is_a_mismatch_with_its_position(self):
        rgb = list(self.rgb)
        rgb[3] = (9, 9, 9)
        write_png(self.d / "sw.png", 2, 2, rgb)
        write_dump(self.d / "sw.idx", 2, 2, [1, 2, 3, 4], [0, 0, 0, 0])
        r = rp.compare(self.d / "gpu.png", self.d / "sw.png", self.d / "sw.idx")
        self.assertEqual("mismatch", r["status"])
        self.assertEqual("final_rgb", r["stage"])
        self.assertEqual(1, r["differing"])
        self.assertEqual([1, 1], r["first_diff"])

    def test_tagged_pixels_are_excluded_by_their_named_rule(self):
        rgb = list(self.rgb)
        rgb[1] = (0, 0, 0)
        rgb[2] = (0, 0, 0)
        write_png(self.d / "sw.png", 2, 2, rgb)
        write_dump(self.d / "sw.idx", 2, 2, [1, 2, 3, 4], [0, 1, 2, 0])
        r = rp.compare(self.d / "gpu.png", self.d / "sw.png", self.d / "sw.idx")
        self.assertEqual("pass", r["status"])
        self.assertEqual(2, r["compared"])
        self.assertEqual({"colour-table-blend": 1, "editor-line-raster": 1}, r["excluded"])

    def test_size_mismatch(self):
        write_png(self.d / "sw.png", 1, 2, self.rgb[:2])
        write_dump(self.d / "sw.idx", 1, 2, [1, 2], [0, 0])
        r = rp.compare(self.d / "gpu.png", self.d / "sw.png", self.d / "sw.idx")
        self.assertEqual("mismatch", r["status"])
        self.assertEqual("size", r["stage"])

    def test_missing_dump_is_unavailable_not_a_pass(self):
        write_png(self.d / "sw.png", 2, 2, self.rgb)
        r = rp.compare(self.d / "gpu.png", self.d / "sw.png", self.d / "missing.idx")
        self.assertEqual("unavailable", r["status"])


class RecordTests(unittest.TestCase):
    def test_every_tag_rule_is_a_named_contract_rule(self):
        for rule in rp.TAG_RULES.values():
            self.assertIn(rule, rp.CONTRACT_RULES)
        for rule in ("zoom-decimation", "light-mode-per-cell", "palshift-zero"):
            self.assertIn(rule, rp.CONTRACT_RULES)

    def test_exit_code(self):
        p = {"status": "pass"}
        m = {"status": "mismatch"}
        u = {"status": "unavailable"}
        self.assertEqual(0, rp.exit_code([p, p]))
        self.assertEqual(1, rp.exit_code([p, m, u]))
        self.assertEqual(2, rp.exit_code([p, u]))
        self.assertEqual(2, rp.exit_code([]))

    def test_case_records_are_sealed(self):
        c = rp.parity_case("act1_town", "headless", None, None, None,
                           extra={"status": "unavailable", "reason": "no game data"})
        self.assertEqual("act1_town", c["map"])
        self.assertEqual("unavailable", c["status"])
        self.assertIn("digest", c)

    def test_run_without_game_data_is_unavailable(self):
        with tempfile.TemporaryDirectory() as d:
            rc = rp.main(["run", "--exe", str(Path(d) / "ds1edit.exe"),
                          "--game-dir", str(Path(d) / "no-game"), "--out-dir", d,
                          "--maps", "act1_town"])
        self.assertEqual(2, rc)


if __name__ == "__main__":
    unittest.main()
```

Run: `python -m unittest -v test.python.test_renderer_parity`
Expected: `ModuleNotFoundError: No module named 'scripts.equivalence.renderer_parity'`.

- [ ] **Step 2: Write `scripts/equivalence/renderer_parity.py`**

```python
"""Software-renderer parity with the GPU path (spec phase 1 gate).

For each map, from two private windowed-only runtimes that differ only in
`renderer =` (sw_runtime.build), render:

* headless: ``--zoom=1:1 --headless`` -- the GPU path once, the software path
  twice (determinism) with ``--headless-indices`` dumping its palette indices
  and tag plane;
* windowed (never fullscreen): ``--selftest=60 --deterministic`` with a
  ``--selftest-shot`` from each path; the software run also dumps its canvas
  with ``--selftest-indices``.

RGB is compared exactly, pixel by pixel, except where a named contract rule
(CONTRACT_RULES) applies; excluded pixels are counted per rule, never passed
by a threshold. Local lane: needs the vanilla 1.13c install.

    python -m scripts.equivalence.renderer_parity run --exe bin/ds1edit.exe \\
        --out-dir tmp/sw-parity

Exit 0 when every case passed; 1 when any mismatched; 2 when anything was
unavailable (and nothing mismatched).
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import sw_runtime as SW  # noqa: E402

SCHEMA = "ds1edit.renderer-parity/1"
CORE_MAPS = ("act1_town", "act1_cave", "act2_desert", "act3_jungle", "act4_fortress",
             "act5_town")
SELFTEST_FRAMES = 60

CONTRACT_RULES = {
    "colour-table-blend":
        "Pixel last written through a D2CMP colour table (COF trans_b 0-6, the 50% "
        "trans colormap, darkness). The GPU path blends RGB continuously; the software "
        "path snaps to the palette as the game does. Verified in the index domain by "
        "test_render_blit, test_render_sw_tables and test_render_map_sw*; excluded here.",
    "editor-line-raster":
        "Pixel last written by canvas_line (unknown-tile markers, grid): Bresenham versus "
        "the GPU line rasterizer. Verified by test_render_canvas; excluded here.",
    "zoom-decimation":
        "Zoom levels other than 1:1 are not RGB-compared: nearest-pixel RLE copies versus "
        "draw-time filtering. Verified by test_render_rle and test_render_spritecache; "
        "parity runs at 1:1.",
    "light-mode-per-cell":
        "Night mode lights per cell (software) versus per corner (GPU). Parity runs in "
        "tile mode.",
    "palshift-zero":
        "A COF pixel palette-shifted onto index 0 is transparent on the software path "
        "(index 0 is the transparency key). Not excluded: it shows up as a differing "
        "pixel and is reported.",
}
TAG_RULES = {1: "colour-table-blend", 2: "editor-line-raster"}


def read_dump(path) -> tuple:
    data = Path(path).read_bytes()
    if len(data) < 17 or data[:8] != b"DS1IDX1\0":
        raise ValueError(f"{path} is not a DS1IDX1 dump")
    w, h = struct.unpack("<II", data[8:16])
    has_tags = data[16] == 1
    n = w * h
    need = 17 + n * (2 if has_tags else 1)
    if len(data) != need:
        raise ValueError(f"{path} is {len(data)} bytes, expected {need}")
    return w, h, data[17:17 + n], (data[17 + n:] if has_tags else None)


def _rgb_bytes(path) -> tuple:
    from PIL import Image
    with Image.open(path) as img:
        img = img.convert("RGB")
        return img.size, img.tobytes()


def compare(gpu_png, sw_png, dump) -> dict:
    out = {"status": "pass", "stage": None, "compared": 0, "differing": 0,
           "excluded": {}, "first_diff": None}
    try:
        (gw, gh), g = _rgb_bytes(gpu_png)
        (sw_w, sw_h), s = _rgb_bytes(sw_png)
        w, h, _idx, tags = read_dump(dump)
    except (OSError, ValueError) as exc:
        out.update(status="unavailable", stage="inputs", reason=str(exc))
        return out
    if (gw, gh) != (sw_w, sw_h) or (w, h) != (sw_w, sw_h):
        out.update(status="mismatch", stage="size",
                   reason=f"gpu {gw}x{gh}, software {sw_w}x{sw_h}, dump {w}x{h}")
        return out
    try:
        import numpy as np
        ga = np.frombuffer(g, dtype=np.uint8).reshape(-1, 3)
        sa = np.frombuffer(s, dtype=np.uint8).reshape(-1, 3)
        ta = np.frombuffer(tags, dtype=np.uint8) if tags else np.zeros(w * h, np.uint8)
        for t in sorted(set(int(v) for v in np.unique(ta)) - {0}):
            out["excluded"][TAG_RULES.get(t, f"tag-{t}")] = int((ta == t).sum())
        exact = ta == 0
        diff = np.any(ga != sa, axis=1) & exact
        out["compared"] = int(exact.sum())
        out["differing"] = int(diff.sum())
        if out["differing"]:
            i = int(np.argmax(diff))
            out["first_diff"] = [i % w, i // w]
    except ImportError:
        for i in range(w * h):
            t = tags[i] if tags else 0
            if t:
                rule = TAG_RULES.get(t, f"tag-{t}")
                out["excluded"][rule] = out["excluded"].get(rule, 0) + 1
                continue
            out["compared"] += 1
            if g[3 * i:3 * i + 3] != s[3 * i:3 * i + 3]:
                out["differing"] += 1
                if out["first_diff"] is None:
                    out["first_diff"] = [i % w, i // w]
    if out["differing"]:
        out.update(status="mismatch", stage="final_rgb")
    return out


def parity_case(map_name: str, mode: str, gpu_png, sw_png, dump,
                extra: Optional[dict] = None) -> dict:
    case = {"schema": SCHEMA, "map": map_name, "mode": mode,
            "rules": sorted(CONTRACT_RULES),
            "inputs": {"gpu": str(gpu_png) if gpu_png else None,
                       "software": str(sw_png) if sw_png else None,
                       "dump": str(dump) if dump else None}}
    if extra and "status" in extra:
        case.update(extra)
    else:
        case.update(compare(gpu_png, sw_png, dump))
        if extra:
            case.update(extra)
    if case["status"] not in C.STATUSES:
        raise ValueError(f"bad status {case['status']!r}")
    return C.seal(case)


def exit_code(cases: list) -> int:
    if not cases:
        return 2
    statuses = {c["status"] for c in cases}
    if "mismatch" in statuses:
        return 1
    return 0 if statuses == {"pass"} else 2


def _bmp_to_png(path: Path) -> Path:
    from PIL import Image
    png = path.with_suffix(".png")
    with Image.open(path) as img:
        img.convert("RGB").save(png)
    return png


def _run_map(map_name: str, runtimes: dict, out: Path) -> list:
    cases = []
    d = out / map_name
    d.mkdir(parents=True, exist_ok=True)

    # headless, 1:1
    gpu_bmp, sw_bmp, sw_bmp2 = d / "gpu.bmp", d / "sw.bmp", d / "sw2.bmp"
    sw_idx, sw_idx2 = d / "sw.idx", d / "sw2.idx"
    for f in (gpu_bmp, sw_bmp, sw_bmp2, sw_idx, sw_idx2):
        f.unlink(missing_ok=True)
    exe, rt, env = runtimes["gpu"]
    pg = SW.run(exe, rt, SW.headless_args(map_name, "1:1", gpu_bmp), env)
    exe, rt, env = runtimes["software"]
    ps = SW.run(exe, rt, SW.headless_args(map_name, "1:1", sw_bmp, indices=sw_idx), env)
    ps2 = SW.run(exe, rt, SW.headless_args(map_name, "1:1", sw_bmp2, indices=sw_idx2), env)
    if any(p.returncode != 0 for p in (pg, ps, ps2)) or not (gpu_bmp.is_file() and sw_bmp.is_file()):
        cases.append(parity_case(map_name, "headless", gpu_bmp, sw_bmp, sw_idx, extra={
            "status": "mismatch", "stage": "process",
            "reason": f"exit gpu={pg.returncode} sw={ps.returncode}/{ps2.returncode}",
            "stdout_tail": (pg.stdout + ps.stdout)[-2000:]}))
    elif sw_idx.read_bytes() != sw_idx2.read_bytes():
        cases.append(parity_case(map_name, "headless", gpu_bmp, sw_bmp, sw_idx, extra={
            "status": "mismatch", "stage": "determinism",
            "reason": "two software renders of the same map differ"}))
    else:
        cases.append(parity_case(map_name, "headless", _bmp_to_png(gpu_bmp),
                                 _bmp_to_png(sw_bmp), sw_idx))

    # windowed (never fullscreen), deterministic selftest
    gpu_shot, sw_shot, sw_frame = d / "gpu_shot.png", d / "sw_shot.png", d / "sw_frame.idx"
    for f in (gpu_shot, sw_shot, sw_frame):
        f.unlink(missing_ok=True)
    exe, rt, env = runtimes["gpu"]
    pg = SW.run(exe, rt, SW.selftest_args(map_name, SELFTEST_FRAMES, gpu_shot,
                                          deterministic=True), env)
    exe, rt, env = runtimes["software"]
    ps = SW.run(exe, rt, SW.selftest_args(map_name, SELFTEST_FRAMES, sw_shot,
                                          indices=sw_frame, deterministic=True), env)
    info_g, info_s = SW.parse_renderer(pg.stdout), SW.parse_renderer(ps.stdout)
    if pg.returncode != 0 or ps.returncode != 0:
        cases.append(parity_case(map_name, "windowed", gpu_shot, sw_shot, sw_frame, extra={
            "status": "mismatch", "stage": "process",
            "reason": f"exit gpu={pg.returncode} sw={ps.returncode}",
            "stdout_tail": (pg.stdout + ps.stdout)[-2000:]}))
    elif not info_g or info_g["active"] != "gpu" or not info_s or info_s["active"] != "software":
        cases.append(parity_case(map_name, "windowed", gpu_shot, sw_shot, sw_frame, extra={
            "status": "mismatch", "stage": "renderer",
            "reason": f"renderer lines gpu={info_g} software={info_s}"}))
    else:
        cases.append(parity_case(map_name, "windowed", gpu_shot, sw_shot, sw_frame,
                                 extra={"device": info_s["device"]}))
    return cases


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--exe", type=Path, required=True)
    r.add_argument("--game-dir", type=Path, default=SW.DEFAULT_GAME_DIR)
    r.add_argument("--out-dir", type=Path, required=True)
    r.add_argument("--maps", nargs="+", default=list(CORE_MAPS))
    args = ap.parse_args(argv)

    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    archives = SW.find_archives(args.game_dir)
    cases = []
    if len(archives) != len(SW.ARCHIVES) or not Path(args.exe).is_file():
        reason = (f"game data unavailable in {args.game_dir}"
                  if len(archives) != len(SW.ARCHIVES) else f"no executable at {args.exe}")
        cases = [parity_case(m, mode, None, None, None,
                             extra={"status": "unavailable", "reason": reason})
                 for m in args.maps for mode in ("headless", "windowed")]
    else:
        runtimes = {}
        for renderer in ("gpu", "software"):
            exe, env = SW.build(out / f"runtime-{renderer}", args.exe,
                                SW.ini_text(archives=archives, renderer=renderer))
            runtimes[renderer] = (exe, out / f"runtime-{renderer}", env)
        for m in args.maps:
            cases.extend(_run_map(m, runtimes, out))

    record = {"schema": SCHEMA, "rules": CONTRACT_RULES, "cases": cases}
    (out / "parity.json").write_text(json.dumps(record, indent=1, sort_keys=True) + "\n",
                                     encoding="utf-8")
    for c in cases:
        excl = ", ".join(f"{k}={v}" for k, v in sorted(c.get("excluded", {}).items()))
        print(f"{c['status']:12} {c['map']:14} {c['mode']:9} compared={c.get('compared')} "
              f"differing={c.get('differing')} first={c.get('first_diff')} excluded[{excl}]"
              f"{' ' + c['reason'] if c.get('reason') else ''}")
    return exit_code(cases)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
```

`contracts.seal()` adds the `digest` field (`canonical_digest` of the record), as every other equivalence record does.

Run: `python -m unittest -v test.python.test_renderer_parity`
Expected: `Ran 10 tests ... OK`.

- [ ] **Step 3: Run the gate on the six core maps (local game-data lane)**

```bash
"$CMAKE" --build tmp/build-sw --config Release --target ds1edit
python -m scripts.equivalence.renderer_parity run --exe bin/ds1edit.exe --out-dir tmp/sw-parity
```
Expected: twelve lines (six maps x headless/windowed), each `pass` with `differing=0`, exit status 0; `tmp/sw-parity/parity.json` holds the sealed records. Headless 1:1 renders of the big maps are large (act1_town is several thousand pixels on each side); allow several minutes.

For every `mismatch`: open `tmp/sw-parity/<map>/gpu.png` and `sw.png` (or the `_shot` pair) at `first_diff`, find which layer differs, and fix the software path (Tasks 9-11). Do not add a rule or a tolerance to make a mismatch pass; a genuinely new class of known difference is a spec change for the maintainer to approve. Record the per-rule excluded counts in the task report (they are the evidence that the excluded area is what the rules say: shadows, blended object layers, selection tint).

- [ ] **Step 4: Commit (only when the maintainer has asked for commits)**

```bash
git add scripts/equivalence/renderer_parity.py test/python/test_renderer_parity.py
git commit -m "Compare the software renderer with the GPU path on the six core maps"
```

---

### Task 14: Software-device lane and the performance gate (phase 1 gate, part 2)

Spec, Testing: "Software-device lane: selftest and the performance gate run under Windows WARP / Mesa llvmpipe `opengl32.dll`, and Mesa llvmpipe on Linux CI." and "Performance: `scripts/equivalence/benchmarks.py`; act1_town and act5_town at 1920x1080 windowed. Record frame-time histogram, compose vs present time, peak working set. CI thresholds only after a reviewed baseline." Linux CI has no game data, so CI exercises the present path on llvmpipe with a portable display test; the map selftests and the gate itself are the local game-data lane.

**Files:**
- Create: `test/test_render_present_display.c`, `test/equivalence/profiles/performance-software.json`
- Modify: `test/cmake/software_renderer.cmake`, `.github/workflows/build.yml`
- Modify: `scripts/equivalence/benchmarks.py`, `test/python/test_benchmarks.py`

**Interfaces:**
- Consumes: `present_canvas_to_bitmap`, `present_force_fallback`, `present_reset`, `present_warning`, `PRESENT_*` (Task 6); `display_device_name` (Task 7); `canvas_create` (Task 1); `sw_runtime` (Task 11); the `selftest:` report lines (Task 12).
- Produces (`scripts/equivalence/benchmarks.py`):
  ```python
  def parse_selftest_frametimes(text: str) -> Optional[dict]   # {"n", "frame_median_ms", "frame_p95_ms", "compose_median_ms", "present_median_ms", "fps_median", "histogram_ms", "renderer", "present"}
  def summarize_frametimes(runs: list, target_fps: float) -> dict   # status "measured"|"unavailable", fps_median, meets_target, comparison "unreviewed"
  class FrameTimeChecker:                                        # wraps OutputChecker; .runs holds measured stats
  # new workload kind "editor-selftest"; run_profile(..., game_dir=None, soft_gl_dir=None); CLI --game-dir, --soft-gl-dir; --env-dir optional
  ```
  CTest label `display` for the windowed present test; `DS1EDIT_REQUIRE_DISPLAY=1` turns "no display" from IGNORED into a failure.

- [ ] **Step 1: Write the display test**

Create `test/test_render_present_display.c`:

```c
/* Present into a real VIDEO bitmap on a real display: the software-device
 * lane (Mesa llvmpipe under Xvfb on Linux CI, the hosted Windows runner's
 * Microsoft Basic Render Driver, Mesa opengl32.dll locally).
 *
 * Creates a WINDOWED display -- never fullscreen -- presents a known canvas
 * through the locked path, reads it back, does the same through the forced
 * fallback, and records the time of a 1920x1080 upload (evidence, not a
 * gate). Without a display the tests are IGNORED, unless
 * DS1EDIT_REQUIRE_DISPLAY=1, where a missing display fails the run. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>

#include "unity/unity.h"
#include "core/palette.h"
#include "render/canvas.h"
#include "render/display_device.h"
#include "render/present.h"

static ALLEGRO_DISPLAY *s_disp;
static RGBA_PALETTE     s_pal;

void setUp(void)
{
   if (s_disp == NULL)
      TEST_IGNORE_MESSAGE("no display here: software-device lane not run");
   present_reset();
}

void tearDown(void) {}

static void make_canvas(DS1_CANVAS *cv, int32_t w, int32_t h)
{
   int32_t x, y;
   TEST_ASSERT_EQUAL_INT(1, canvas_create(cv, w, h, 0));
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         cv->pixels[y * w + x] = (uint8_t) ((x * 3 + y * 7) & 0xFF);
}

static void assert_reads_back(ALLEGRO_BITMAP *bmp, const DS1_CANVAS *cv)
{
   ALLEGRO_LOCKED_REGION *lr = al_lock_bitmap(bmp, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                                              ALLEGRO_LOCK_READONLY);
   int32_t x, y;

   TEST_ASSERT_NOT_NULL(lr);
   for (y = 0; y < cv->h; y++)
      for (x = 0; x < cv->w; x++)
      {
         const uint8_t *p = (const uint8_t *) lr->data + (ptrdiff_t) y * lr->pitch + x * 4;
         uint8_t i = cv->pixels[y * cv->stride + x];
         TEST_ASSERT_EQUAL_HEX8(i ? s_pal.colors[i].r : 0, p[0]);
         TEST_ASSERT_EQUAL_HEX8(i ? s_pal.colors[i].g : 0, p[1]);
         TEST_ASSERT_EQUAL_HEX8(i ? s_pal.colors[i].b : 0, p[2]);
      }
   al_unlock_bitmap(bmp);
}

static ALLEGRO_BITMAP *video_bitmap(int w, int h)
{
   ALLEGRO_BITMAP *bmp;
   al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);
   bmp = al_create_bitmap(w, h);
   TEST_ASSERT_NOT_NULL(bmp);
   TEST_ASSERT_FALSE(al_get_bitmap_flags(bmp) & ALLEGRO_MEMORY_BITMAP);
   return bmp;
}

void test_present_locks_a_video_bitmap_and_reads_back(void)
{
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = video_bitmap(64, 32);

   make_canvas(&cv, 64, 32);
   TEST_ASSERT_EQUAL_INT(PRESENT_LOCKED, present_canvas_to_bitmap(bmp, &cv, &s_pal));
   assert_reads_back(bmp, &cv);
   TEST_ASSERT_NULL(present_warning());
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

void test_fallback_draws_the_same_picture_and_warns(void)
{
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = video_bitmap(64, 32);

   make_canvas(&cv, 64, 32);
   present_force_fallback(1);
   TEST_ASSERT_EQUAL_INT(PRESENT_FALLBACK, present_canvas_to_bitmap(bmp, &cv, &s_pal));
   assert_reads_back(bmp, &cv);
   TEST_ASSERT_NOT_NULL(present_warning());
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

static int cmp_double(const void *a, const void *b)
{
   double x = *(const double *) a, y = *(const double *) b;
   return (x > y) - (x < y);
}

void test_full_hd_upload_time_is_recorded(void)
{
   enum { RUNS = 30 };
   DS1_CANVAS cv;
   ALLEGRO_BITMAP *bmp = video_bitmap(1920, 1080);
   double t[RUNS];
   int i, rc = 0;

   make_canvas(&cv, 1920, 1080);
   for (i = 0; i < RUNS; i++)
   {
      double t0 = al_get_time();
      rc = present_canvas_to_bitmap(bmp, &cv, &s_pal);
      t[i] = (al_get_time() - t0) * 1000.0;
   }
   qsort(t, RUNS, sizeof(t[0]), cmp_double);
   printf("present-display: 1920x1080 upload median_ms=%.3f max_ms=%.3f\n",
          t[RUNS / 2], t[RUNS - 1]);
   TEST_ASSERT_EQUAL_INT(PRESENT_LOCKED, rc);
   canvas_destroy(&cv);
   al_destroy_bitmap(bmp);
}

static int run_tests(void)
{
   const char *req = getenv("DS1EDIT_REQUIRE_DISPLAY");
   char device[256];
   int i, rc;

   if (!al_init())
   {
      printf("present-display: al_init failed\n");
      return 1;
   }
   al_set_new_display_flags(ALLEGRO_WINDOWED);
   s_disp = al_create_display(320, 240);
   if ((s_disp == NULL) && (req != NULL) && (strcmp(req, "1") == 0))
   {
      printf("present-display: FAIL -- DS1EDIT_REQUIRE_DISPLAY=1 but no display could be created\n");
      return 1;
   }
   if (s_disp != NULL)
   {
      if (!display_device_name(s_disp, device, sizeof(device)))
         strcpy(device, "unknown");
      printf("present-display: %s driver, device \"%s\"\n",
             (al_get_display_flags(s_disp) & ALLEGRO_OPENGL) ? "OpenGL" : "Direct3D",
             device);
   }
   for (i = 0; i < 256; i++)
   {
      s_pal.colors[i].r = (uint8_t) i;
      s_pal.colors[i].g = (uint8_t) (i * 5);
      s_pal.colors[i].b = (uint8_t) (255 - i);
      s_pal.colors[i].a = 255;
   }

   UNITY_BEGIN();
   RUN_TEST(test_present_locks_a_video_bitmap_and_reads_back);
   RUN_TEST(test_fallback_draws_the_same_picture_and_warns);
   RUN_TEST(test_full_hd_upload_time_is_recorded);
   rc = UNITY_END();
   if (s_disp != NULL)
      al_destroy_display(s_disp);
   return rc;
}

/* macOS builds define ALLEGRO_NO_MAGIC_MAIN: displays must be created under
   al_run_main, exactly as src/main.c does. */
#ifdef __APPLE__
static int apple_main(int argc, char **argv)
{
   (void) argc;
   (void) argv;
   return run_tests();
}

int main(int argc, char **argv)
{
   return al_run_main(argc, argv, apple_main);
}
#else
int main(void)
{
   return run_tests();
}
#endif
```

Append to `test/cmake/software_renderer.cmake`:

```cmake
# --- Task 14: software-device lane --------------------------------------------
# Opens a WINDOWED display. IGNORED where none can be created, unless
# DS1EDIT_REQUIRE_DISPLAY=1 (the CI lane steps in .github/workflows/build.yml).
ds1edit_visual_test(test_render_present_display ALLEGRO SOURCES
    test_render_present_display.c ${_sw}/present.c ${_sw}/canvas.c ${_sw}/display_device.c)
add_test(NAME render_present_display COMMAND test_render_present_display)
set_tests_properties(render_present_display PROPERTIES LABELS display TIMEOUT 120)
```

- [ ] **Step 2: Run it locally (windowed)**

```bash
"$CMAKE" -S . -B tmp/build-sw -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows -DVCPKG_TARGET_TRIPLET=x86-windows
"$CMAKE" --build tmp/build-sw --config Release --target test_render_present_display
DS1EDIT_REQUIRE_DISPLAY=1 "$CTEST" --test-dir tmp/build-sw -C Release -L display --output-on-failure -V
```
Expected: a small window appears briefly; output has `present-display: <driver> driver, device "<your GPU>"`, a `1920x1080 upload median_ms=...` line, `3 Tests 0 Failures 0 Ignored`.

Software-device variant on Windows (Mesa llvmpipe): download mesa-dist-win's x86 release (the maintainer fetches it; this plan does not) and copy its `opengl32.dll`, `libgallium_wgl.dll` and `libglapi.dll` (whichever that release ships) next to `tmp/build-sw/test/Release/test_render_present_display.exe`, add an `allegro5.cfg` there containing `[graphics]` / `driver=opengl`, then:

```bash
GALLIUM_DRIVER=llvmpipe DS1EDIT_REQUIRE_DISPLAY=1 "$CTEST" --test-dir tmp/build-sw -C Release -L display --output-on-failure -V
```
Expected: `device "llvmpipe (LLVM ..., 256 bits)"`, `3 Tests 0 Failures 0 Ignored`. Remove the three DLLs and the cfg afterwards so later local runs use the real driver.

- [ ] **Step 3: CI steps**

In `.github/workflows/build.yml`, append to the `windows` job's steps (after `Test`):

```yaml
      - name: Software-device lane (hosted runner, Microsoft Basic Render Driver)
        env:
          DS1EDIT_REQUIRE_DISPLAY: "1"
        run: ctest --preset ci -L display --output-on-failure -V
```

and to the `linux` job's steps (after `Test`):

```yaml
      - name: Install a software GL stack
        run: sudo apt-get install -y --no-install-recommends xvfb libgl1-mesa-dri libglx-mesa0
      - name: Software-device lane (Mesa llvmpipe under Xvfb)
        env:
          DS1EDIT_REQUIRE_DISPLAY: "1"
          LIBGL_ALWAYS_SOFTWARE: "1"
          GALLIUM_DRIVER: llvmpipe
        run: xvfb-run -a -s "-screen 0 1920x1080x24" ctest --preset linux -L display --output-on-failure -V
```

The macOS job runs the display test inside its normal `ctest` step; it reports IGNORED if the runner cannot open a window, and is not required (no macOS software rasterizer lane is in the spec).

- [ ] **Step 4: Write the failing benchmark tests**

Append to `test/python/test_benchmarks.py` (before the `if __name__` block, if any):

```python
class SelftestFrameTimeTests(unittest.TestCase):
    TEXT = ('renderer: software (renderer = software, device "llvmpipe (LLVM 15.0.7, 256 bits)")\n'
            "selftest: rendered 600 frame(s), exiting\n"
            "selftest: frametimes n=600 frame_median_ms=12.500 frame_p95_ms=15.000 "
            "compose_median_ms=8.000 present_median_ms=3.000 fps_median=80.0\n"
            "selftest: histogram_ms lt4=0 lt8=0 lt12=100 lt16_7=490 lt25=10 lt33_4=0 "
            "lt50=0 lt100=0 ge100=0\n"
            "selftest: renderer=software present=locked\n")

    def test_parse(self):
        s = bm.parse_selftest_frametimes(self.TEXT)
        self.assertEqual(600, s["n"])
        self.assertEqual(12.5, s["frame_median_ms"])
        self.assertEqual(8.0, s["compose_median_ms"])
        self.assertEqual(490, s["histogram_ms"]["lt16_7"])
        self.assertEqual("software", s["renderer"])
        self.assertEqual("locked", s["present"])
        self.assertIsNone(bm.parse_selftest_frametimes("selftest: rendered 3 frame(s)"))

    def test_summary_is_unreviewed_and_compares_with_the_target(self):
        runs = [bm.parse_selftest_frametimes(self.TEXT.replace("12.500", v))
                for v in ("12.500", "20.000", "14.000")]
        s = bm.summarize_frametimes(runs, 60.0)
        self.assertEqual("measured", s["status"])
        self.assertEqual(14.0, s["frame_median_ms"])
        self.assertAlmostEqual(1000.0 / 14.0, s["fps_median"])
        self.assertTrue(s["meets_target"])
        self.assertEqual("unreviewed", s["comparison"]["status"])
        self.assertFalse(bm.summarize_frametimes(runs, 90.0)["meets_target"])
        self.assertEqual("unavailable", bm.summarize_frametimes([None], 60.0)["status"])

    def test_expand_editor_selftest(self):
        profile = {"schema": bm.PROFILE_SCHEMA, "id": "p", "workloads": [{
            "id": "sw", "kind": "editor-selftest", "maps": ["act1_town"],
            "zooms": [{"zoom": "1:1", "target_fps": 60}, {"zoom": "1:16", "target_fps": 30}],
            "devices": ["native", "llvmpipe"], "renderer": "software", "frames": 600,
            "screen": [1920, 1080]}]}
        specs = bm.expand_workloads(profile, None, None)
        self.assertEqual(["sw:act1_town:1:1:native", "sw:act1_town:1:1:llvmpipe",
                          "sw:act1_town:1:16:native", "sw:act1_town:1:16:llvmpipe"],
                         [s["id"] for s in specs])
        self.assertIn("--uncapped", specs[0]["command"])
        self.assertIn("--deterministic", specs[0]["command"])
        self.assertIn("--zoom=1:16", specs[2]["command"])
        self.assertEqual(30.0, specs[2]["target_fps"])
        self.assertEqual([1920, 1080], specs[0]["screen"])

    def test_checker_requires_frametimes_renderer_and_device(self):
        class Inner:
            reference = None

            def __call__(self, oc, i, p):
                return True, "ok"

            def prepare(self, i, p):
                pass

        class Oc:
            def __init__(self, text):
                self.stdout = text.encode()

        c = bm.FrameTimeChecker(Inner(), "software", "llvmpipe")
        self.assertEqual((True, "ok"), c(Oc(self.TEXT), 0, "measured"))
        self.assertEqual(1, len(c.runs))
        self.assertFalse(c(Oc("nothing"), 1, "measured")[0])
        self.assertFalse(c(Oc(self.TEXT.replace("renderer=software", "renderer=gpu")), 2,
                           "measured")[0])
        self.assertFalse(bm.FrameTimeChecker(Inner(), "software", "warp")(
            Oc(self.TEXT), 3, "measured")[0])
        c(Oc(self.TEXT), 4, "warmup")
        self.assertEqual(1, len(c.runs))

    def test_software_profile_is_well_formed(self):
        profile = json.loads((ROOT / "test" / "equivalence" / "profiles" /
                              "performance-software.json").read_text())
        specs = bm.expand_workloads(profile, None, None)
        ids = [s["id"] for s in specs]
        self.assertIn("sw-selftest:act1_town:1:1:llvmpipe", ids)
        self.assertIn("sw-selftest:act5_town:1:16:warp", ids)
        self.assertIn("gpu-selftest:act5_town:1:1:native", ids)
        for s in specs:
            self.assertEqual([1920, 1080], s["screen"])
```

Run: `python -m unittest -v test.python.test_benchmarks`
Expected: the new tests fail with `AttributeError: module 'scripts.equivalence.benchmarks' has no attribute 'parse_selftest_frametimes'` (and the profile test with `FileNotFoundError`).

- [ ] **Step 5: Implement the `editor-selftest` workload**

In `scripts/equivalence/benchmarks.py`:

1. After the existing imports add:

```python
_REPO_ROOT = Path(__file__).resolve().parents[2]
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))
from scripts.equivalence import sw_runtime as SW  # noqa: E402
```

2. Before the `# Profile runner` section add:

```python
# ---------------------------------------------------------------------------
# Windowed --selftest workloads (software renderer performance gate)
# ---------------------------------------------------------------------------

SELFTEST_FRAMETIMES_RE = re.compile(
    r"selftest: frametimes n=(?P<n>\d+) frame_median_ms=(?P<frame_median_ms>[0-9.]+) "
    r"frame_p95_ms=(?P<frame_p95_ms>[0-9.]+) "
    r"compose_median_ms=(?P<compose_median_ms>[0-9.]+) "
    r"present_median_ms=(?P<present_median_ms>[0-9.]+) fps_median=(?P<fps_median>[0-9.]+)")
SELFTEST_HISTOGRAM_RE = re.compile(r"selftest: histogram_ms ((?:\w+=\d+ ?)+)")
SELFTEST_RENDERER_RE = re.compile(r"selftest: renderer=(\w+) present=([\w-]+)")


def parse_selftest_frametimes(text: str) -> Optional[dict]:
    m = SELFTEST_FRAMETIMES_RE.search(text or "")
    if not m:
        return None
    out = {k: (int(v) if k == "n" else float(v)) for k, v in m.groupdict().items()}
    h = SELFTEST_HISTOGRAM_RE.search(text)
    out["histogram_ms"] = ({k: int(v) for k, v in (p.split("=") for p in h.group(1).split())}
                           if h else None)
    r = SELFTEST_RENDERER_RE.search(text)
    out["renderer"], out["present"] = (r.group(1), r.group(2)) if r else (None, None)
    return out


def summarize_frametimes(runs: list, target_fps: float) -> dict:
    """Median of the per-run frame medians against the spec target. Never a
    pass/fail: the comparison stays 'unreviewed' until a maintainer reviews a
    baseline (spec: CI thresholds only after a reviewed baseline)."""
    runs = [r for r in runs if r]
    unreviewed = {"status": "unreviewed", "reason": UNREVIEWED}
    if not runs:
        return {"status": "unavailable", "reason": "no measured run printed a frametimes line",
                "runs": [], "target_fps": target_fps, "comparison": unreviewed}
    med = statistics.median(r["frame_median_ms"] for r in runs)
    fps = 1000.0 / med if med > 0 else None
    return {"status": "measured", "runs": runs, "target_fps": target_fps,
            "frame_median_ms": med, "fps_median": fps,
            "meets_target": fps is not None and fps >= target_fps,
            "compose_median_ms": statistics.median(r["compose_median_ms"] for r in runs),
            "present_median_ms": statistics.median(r["present_median_ms"] for r in runs),
            "comparison": unreviewed}


class FrameTimeChecker:
    """An OutputChecker that also requires the selftest's frame-time report,
    the requested renderer and the requested device. Keeps measured runs."""

    def __init__(self, inner, renderer: str, device: str):
        self.inner = inner
        self.renderer = renderer
        self.device = device
        self.runs = []

    @property
    def reference(self):
        return self.inner.reference

    def prepare(self, index, phase):
        self.inner.prepare(index, phase)

    def __call__(self, outcome, index, phase):
        ok, detail = self.inner(outcome, index, phase)
        if not ok:
            return ok, detail
        text = outcome.stdout.decode("utf-8", "replace")
        stats = parse_selftest_frametimes(text)
        if stats is None:
            return False, "selftest printed no frametimes line"
        if stats["renderer"] != self.renderer:
            return False, f"ran renderer {stats['renderer']!r}, asked for {self.renderer!r}"
        dev_ok, why = SW.device_ok(self.device, SW.parse_renderer(text))
        if not dev_ok:
            return False, why
        if phase == "measured":
            self.runs.append(stats)
        return True, "ok"
```

3. In `expand_workloads`, add before the final `else: raise BenchmarkError(...)`:

```python
        elif kind == "editor-selftest":
            frames = int(w.get("frames", 600))
            for map_name in w["maps"]:
                for z in w["zooms"]:
                    for device in w.get("devices", ["native"]):
                        specs.append({
                            "id": f"{w['id']}:{map_name}:{z['zoom']}:{device}",
                            "kind": kind, "map": map_name, "zoom": z["zoom"],
                            "target_fps": float(z["target_fps"]), "device": device,
                            "renderer": w.get("renderer", "software"), "frames": frames,
                            "screen": list(w.get("screen", [1920, 1080])),
                            "command": SW.selftest_args(map_name, frames, "{out}/shot.png",
                                                        deterministic=True, uncapped=True,
                                                        zoom=z["zoom"]),
                            "needs": ["game-data"], "cold_first": False,
                            "check": {"min_files": 1, "stable": False}})
```

and change the `map-open-headless` branch to refuse a missing environment explicitly, as its first line:

```python
            if env_dir is None:
                raise BenchmarkError("map-open-headless workloads need --env-dir")
```

4. Change `run_profile` to:

```python
def run_profile(profile: dict, env_dir, exe: Path, probe: Optional[Path],
                out_dir: Path, observer, *, build_dir=None, repo=None,
                only: Optional[list] = None, timeout_s: Optional[float] = None,
                game_dir=None, soft_gl_dir=None) -> dict:
    # Commands run with cwd=env_dir (or a private runtime), so every path handed
    # to them must be absolute.
    env_dir = Path(env_dir).resolve() if env_dir else None
    out_dir = Path(out_dir).resolve()
    exe = Path(exe).resolve()
    probe = Path(probe).resolve() if probe else None
    out_dir.mkdir(parents=True, exist_ok=True)
    reps = int(profile.get("repetitions", DEFAULT_REPETITIONS))
    warm = int(profile.get("warmups", DEFAULT_WARMUPS))
    host = host_identity()
    source_state = build_identity(exe, build_dir, repo)
    archives = SW.find_archives(game_dir if game_dir else SW.DEFAULT_GAME_DIR)
    results, skipped = [], []
    for spec in expand_workloads(profile, env_dir, probe):
        if only and not any(spec["id"].startswith(o) for o in only):
            continue
        if spec.get("status") == "not_run":
            skipped.append({"id": spec["id"], "status": "not_run", "reason": spec["reason"]})
            continue
        safe_id = re.sub(r"[^A-Za-z0-9_.-]", "_", spec["id"])
        work = out_dir / "work" / safe_id
        cwd, env = env_dir, None
        if spec["kind"] == "editor-selftest":
            if len(archives) != len(SW.ARCHIVES):
                skipped.append({"id": spec["id"], "status": "unavailable",
                                "reason": "vanilla 1.13c game data not found (--game-dir)"})
                continue
            runtime = out_dir / "runtime" / safe_id
            try:
                rt_exe, env = SW.build(
                    runtime, exe,
                    SW.ini_text(archives=archives, renderer=spec["renderer"], zoom=spec["zoom"],
                                width=spec["screen"][0], height=spec["screen"][1]),
                    device=spec["device"], soft_gl_dir=soft_gl_dir)
            except SW.RuntimeSetupError as exc:
                skipped.append({"id": spec["id"], "status": "unavailable", "reason": str(exc)})
                continue
            checker = FrameTimeChecker(OutputChecker(work, **spec["check"]),
                                       spec["renderer"], spec["device"])
            cmd = [str(rt_exe)] + [c.replace("{out}", str(work)) for c in spec["command"]]
            cwd = runtime
        else:
            if env_dir is None:
                skipped.append({"id": spec["id"], "status": "unavailable",
                                "reason": "--env-dir not given"})
                continue
            checker = OutputChecker(work, **spec["check"])
            cmd = [c.replace("{exe}", str(exe)).replace("{probe}", str(probe or ""))
                    .replace("{out}", str(work)) for c in spec["command"]]
            if "{probe}" in " ".join(spec["command"]) and not probe:
                skipped.append({"id": spec["id"], "status": "unavailable",
                                "reason": "probe executable not provided"})
                continue

        def probe_identity(exe=exe):
            return {"host_class": host["host_class"], "host": host,
                    "build": build_identity(exe, build_dir, None)}

        rec = measure_workload(cmd, cwd, reps, warm, observer,
                               timeout_s=timeout_s or float(profile.get("timeout_s", 120)),
                               check=checker, setup=checker.prepare,
                               identity_probe=probe_identity,
                               cold_first=spec.get("cold_first", False),
                               env=env,
                               workload_id=spec["id"],
                               inputs={"command_template": spec["command"],
                                       "output_reference_digest": None})
        rec["inputs"]["output_reference_digest"] = checker.reference
        if spec["kind"] == "editor-selftest":
            rec["inputs"]["game_data"] = {slot: {"path": p, "bytes": Path(p).stat().st_size,
                                                 "hashed": False}
                                          for slot, p in archives.items()}
            rec["inputs"]["device"] = spec["device"]
            rec["inputs"]["screen"] = spec["screen"]
            rec["frame_times"] = summarize_frametimes(checker.runs, spec["target_fps"])
        else:
            rec["inputs"]["game_data"] = _game_data_identity(env_dir) \
                if "game-data" in spec.get("needs", []) or spec["kind"] == "editor-cli" else None
        (out_dir / f"{safe_id}.json").write_text(
            json.dumps(rec, indent=1, sort_keys=True) + "\n", encoding="utf-8")
        results.append(rec)
        shutil.rmtree(work, ignore_errors=True)
    summary = {"schema": "ds1edit.benchmark-run/1", "profile": profile["id"],
               "host": host, "build": source_state,
               "source_state_note": "git_head/worktree_diff_sha256 describe the tree at capture "
                                    "time; the measured binary is pinned by exe_sha256",
               "results": [
                   {"id": r["workload_id"], "status": r["status"],
                    "median_s": (r.get("stats") or {}).get("median"),
                    "fps_median": (r.get("frame_times") or {}).get("fps_median"),
                    "meets_target": (r.get("frame_times") or {}).get("meets_target"),
                    "flags": r["flags"]} for r in results],
               "not_run": skipped}
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n",
                                          encoding="utf-8")
    return summary
```

5. In `main()`: make `--env-dir` optional (`r.add_argument("--env-dir", type=Path)`), add

```python
    r.add_argument("--game-dir", type=Path, default=SW.DEFAULT_GAME_DIR)
    r.add_argument("--soft-gl-dir", type=Path)
```

pass `game_dir=args.game_dir, soft_gl_dir=args.soft_gl_dir` to `run_profile`, and print the frame rate with each result:

```python
    for r_ in summary["results"]:
        fps = r_.get("fps_median")
        print(f"{r_['status']:16} {r_['id']}  median={r_['median_s']}  "
              f"fps={'%.1f' % fps if fps else '-'}  target_met={r_.get('meets_target')}  "
              f"{r_['flags']}")
```

Create `test/equivalence/profiles/performance-software.json`:

```json
{
 "schema": "ds1edit.performance-profile/1",
 "id": "performance-software",
 "description": "Software 8-bit renderer, phase 1 performance gate (docs/superpowers/specs/2026-10-22-software-renderer-design.md): 60 fps median at 1920x1080 windowed, 1:1 zoom, one core of a mid-range x86 CPU, no GPU acceleration; at least 30 fps at 1:16. Windowed --selftest=600 with --uncapped (vsync off, no 60 Hz cap) and --deterministic. Records the frame-time histogram, compose vs present medians and the per-process peak working set. Comparisons stay 'unreviewed': CI thresholds only after a reviewed baseline.",
 "repetitions": 5,
 "warmups": 1,
 "timeout_s": 300,
 "build_requirements": {"config": "Release", "sanitizers": false, "coverage": false},
 "comparison_policy": "like-for-like (workload, inputs, build configuration, compiler, host class, device) only; otherwise incomparable; never an automatic pass",
 "workloads": [
  {
   "id": "sw-selftest",
   "kind": "editor-selftest",
   "description": "renderer = software on the native device, Mesa llvmpipe and WARP (Mesa d3d12 on Microsoft Basic Render Driver).",
   "maps": ["act1_town", "act5_town"],
   "zooms": [{"zoom": "1:1", "target_fps": 60}, {"zoom": "1:16", "target_fps": 30}],
   "devices": ["native", "llvmpipe", "warp"],
   "renderer": "software",
   "frames": 600,
   "screen": [1920, 1080],
   "requires": ["d2-game-data"]
  },
  {
   "id": "gpu-selftest",
   "kind": "editor-selftest",
   "description": "The same workload through renderer = gpu on the native device, for comparison.",
   "maps": ["act1_town", "act5_town"],
   "zooms": [{"zoom": "1:1", "target_fps": 60}, {"zoom": "1:16", "target_fps": 30}],
   "devices": ["native"],
   "renderer": "gpu",
   "frames": 600,
   "screen": [1920, 1080],
   "requires": ["d2-game-data"]
  }
 ]
}
```

- [ ] **Step 6: Run the Python tests**

```bash
python -m unittest -v test.python.test_benchmarks test.python.test_sw_runtime
```
Expected: all tests pass, including the five new `SelftestFrameTimeTests`; the pre-existing benchmark tests still pass (the `map-open-headless` path is unchanged when `--env-dir` is given).

- [ ] **Step 7: Run the performance gate (local game-data lane)**

Release build, windowed, vanilla 1.13c. `C:/tools/mesa-dist-win/x86` stands for wherever the maintainer unpacked mesa-dist-win's x86 build:

```bash
"$CMAKE" --build tmp/build-sw --config Release --target ds1edit
python -m scripts.equivalence.benchmarks run --profile test/equivalence/profiles/performance-software.json --exe bin/ds1edit.exe --build-dir tmp/build-sw --repo . --out-dir tmp/perf-sw --soft-gl-dir "C:/tools/mesa-dist-win/x86"
```
Expected: one line per workload id, e.g. `pass  sw-selftest:act1_town:1:1:llvmpipe  median=...  fps=NN.N  target_met=True  [...]`; `tmp/perf-sw/<id>.json` carries `frame_times` (histograms, compose and present medians), `resource` samples with `peak_working_set_bytes`, and `comparison.status = "unreviewed"`. `warp` ids report `output_mismatch` with `asked for warp, the display bound '...'` if that Mesa build cannot reach WARP; record it as unavailable evidence rather than a pass.

Phase 1 performance gate (spec, Phasing: "fps target on WARP"): `fps_median >= 60` for `sw-selftest:*:1:1:warp` and `>= 30` for `sw-selftest:*:1:16:warp` on the reference machine. When WARP is unreachable, report the llvmpipe numbers and say so. Whatever the outcome, hand `tmp/perf-sw/summary.json` and the per-workload records to the maintainer for review; do not write thresholds into CI. If the target is missed, attach the compose/present split: compose-bound points at the blit loops (`blit_rle`'s per-pixel mode switch is the first candidate), present-bound at the upload.

- [ ] **Step 8: Commit (only when the maintainer has asked for commits)**

```bash
git add test/test_render_present_display.c test/cmake/software_renderer.cmake .github/workflows/build.yml scripts/equivalence/benchmarks.py test/python/test_benchmarks.py test/equivalence/profiles/performance-software.json
git commit -m "Measure the software renderer on software rasterizers, and present on llvmpipe in CI"
```

---

## Phase 1 gate

Phase 1 is done when, on the reference Windows machine with the vanilla 1.13c install:

1. `"$CTEST" --test-dir tmp/build-sw -C Release -L portable` passes, and CI is green on Windows x86, Linux x86_64 and macOS arm64 (including both software-device lane steps).
2. `python -m scripts.equivalence.renderer_parity run --exe bin/ds1edit.exe --out-dir tmp/sw-parity` exits 0: all six core maps, headless and windowed, `pass`, excluded pixels only under the named contract rules.
3. `tmp/perf-sw/summary.json` from Task 14 shows the WARP (or, where WARP is unreachable, llvmpipe) frame rates against "60 fps median at 1920x1080 windowed, 1:1 zoom" and "at least 30 fps at 1:16", handed to the maintainer for review. No CI threshold is set.

## Follow-up plans (out of scope here)

- **Phase 2 — UI port** (spec "UI port"): `src/render/ui_sw.c/.h` with `ui_text`, `ui_rect`, `ui_fill`, `ui_line`, `ui_blit` in palette indices; 1-bit glyph masks rasterized once from the current font; ~16 named UI colours resolved per act palette with `palette_find_closest` and recomputed on palette/gamma change; the five-palette WCAG 4.5:1 contrast test; window-by-window port of the ~91 text calls in 15 files. This also moves the overlays phase 1 leaves on Allegro (walkable info, grid over walls, cursor, paths, object labels, help window, HUD, sidebar, properties panel) into the canvas and makes `present` the single full-window upload the spec describes.
- **Phase 3 — `auto` becomes the default**: default `renderer = auto` (software on software rasterizers), then software for everyone once parity and performance hold on Windows, Linux and macOS; update `ini_create`, `Ds1edit.ini.sample`, README troubleshooting.
- **Phase 4 — delete the GPU per-tile path**: `wpreview_draw_*` GPU helpers, `dt1_zoom` bitmaps, `LAY_INF_S.bmp`, the bitmap promotion in `main.c`, `dt1_rebuild_bitmaps_from_cache`, the `a5_draw_*` blend helpers in `ui/compat.h`; `map_sw` becomes the only map path. Until then the software path still pays for building the GPU bitmaps at load.
- **Live-capture check of the colour tables** (spec, Testing): compare `sw_tables` against the tables read from the original game on an idle fleet member through the package B live-capture harness. Phase 1 covers the D2CMP formulas and the vanilla `Pal.PL2` bytes only.
- **Golden regeneration** (spec, Testing): "Goldens regenerated once, in a reviewed commit, after the tile rarity selection change is settled."
- **Performance work if the gate misses**: specialize `blit_rle` per mode; only then consider the spec's deferred options (dirty rectangles, pre-composited chunks, SIMD/threads), each of which the spec says to revisit "only if measurements miss the target".

## Self-review against the spec

**Spec coverage.**

| Spec requirement | Task |
| --- | --- |
| `canvas.c/.h`: fill, hline/vline/line, rect, rectfill, RLE blit (opaque, h-flip), RLE through 256x256 colormap, RLE through darkness; all clip; offsets never trusted; no Allegro | 1, 3 |
| `spritecache.c/.h`: index-frame seam -> game RLE (`skip n` / `copy n`), one copy per asset per zoom, built on first use, freed when a map closes, replaces per-tile bitmaps on the software path | 2, 4, 9, 10 |
| `map_sw.c/.h`: reuses `wpreview_get_visible_tile_bounds` and `wpreview_sort_order_data` | 8, 9 |
| `present.c/.h`: palette-to-RGB table, one write-only lock, expand, unlock; flip (still in `misc_draw_screen`) | 6, 11 |
| `renderer = software | gpu | auto`, `auto` = software on WARP / llvmpipe / GDI Generic | 7 |
| Animation on the 25 Hz tick; presents capped at 60 Hz without relying on vsync | 11 (pacing), unchanged tick |
| Layer passes: floor -> shadows -> walls/objects by sort order -> roofs -> overlays | 9, 10 (passes 1-6); overlays after pass 6 stay on Allegro in phase 1 (see open question 5) |
| Draw-operation mapping table (bitmap, trans/faded, blended `trans_b`, gouraud, line/rect/rectfill) | 9, 10 (rect/rectfill exist in the canvas, used by the phase 2 UI) |
| Zoom 2:1 .. 1:16 by nearest-pixel decimation, 2:1 by doubling, no per-frame scaling | 2, 4, 9, 10 |
| Canvas indices only; RGB table rebuilt only on palette or gamma change | 6 |
| Present fallback: one memory-bitmap draw, one-time warning (stderr and status bar) | 6, 11 |
| Editing state read from the same cell data | 9 |
| COF blend tables modes 0-6 (D2CMP formulas) and darkness colormaps | 5, 10 |
| Asset decode failure: magenta placeholder, logged once per asset | 4, 9, 10 |
| Canvas unit tests with poisoned borders | 1, 3 |
| Sprite cache: index frame -> RLE -> blit reproduces DC6/DCC/DT1 fixtures; each zoom matches a reference decimation | 2, 4 |
| Blend and darkness tables vs D2CMP formulas | 5 (formulas + vanilla `Pal.PL2`); live capture is a follow-up |
| Map parity: six core maps, headless and windowed (never fullscreen), named contract rules, not tolerances | 12, 13 |
| Software-device lane: WARP / llvmpipe `opengl32.dll` locally, llvmpipe on Linux CI | 14 |
| Performance: `benchmarks.py`, act1_town and act5_town at 1920x1080 windowed, histogram, compose vs present, peak working set; CI thresholds only after review | 12, 14 |
| UI colours, fonts, `ui_sw` | Phase 2 (follow-up) |

**Placeholder scan.** Every code step carries the code; the only steps without code are the verbatim move in Task 8 (whose full text is included) and the manual inspection in Tasks 11 and 13 (with exact files and criteria). The mesa-dist-win path in Task 14 is a stand-in for wherever the maintainer unpacks a download this plan does not perform.

**Type consistency.** Checked across tasks: `canvas_blit_rle_cmap/lit/shade` take flat `const uint8_t *` tables everywhere (`ctx->trans`, `sw_tables_blend()`, `tables->dark[L]`); `spritecache_get(const void *, const DS1EDIT_INDEX_FRAME *, int)`; `map_sw_frame_canvas(w, h, with_tags)`; `map_sw_prepare_context(ctx, pal, light_x, light_y)`; `present_canvas_to_bitmap(ALLEGRO_BITMAP *, const DS1_CANVAS *, const RGBA_PALETTE *)` returning `PRESENT_*`; `SC_ZOOM_*` asserted equal to `ZOOM_E` and `SW_PL2_OFF_*` to `COF_* * 256` at compile time in `map_sw.c`; the `renderer:` line format is produced in `main.c` (Tasks 7 and 11) and parsed by `sw_runtime.RENDERER_LINE`; the `selftest:` report lines are produced by `frame_stats_format` (Task 12) and parsed by `benchmarks.SELFTEST_*_RE` (Task 14).

**Open questions for the maintainer** (decisions this plan made provisionally; each is cheap to change before execution):

1. Phase 1 keeps `renderer = gpu` as the default and implements `auto` with the transition rule. Is that the intended phase 1 default?
2. Selection tint: the GPU path draws a selected/hovered tile as a 50 % alpha of the sprite (`wpreview_shape` ignores its `COL_*` argument), so the software path uses the 50 % `CM_TRANS` colormap to match. The original editor tinted through `CM_SELECT` with the colour index. Keep the GPU behaviour, or restore the coloured tint (which then becomes a named parity rule)?
3. Placeholder scope: drawn only where the GPU path would have drawn something (block header promises pixels but no index data; COF frame with a bitmap but no indices). A layer whose whole load failed stays invisible on both paths, which keeps parity on maps with missing DCCs. Is the narrower reading acceptable, or should every failed layer show a box?
4. Night mode: one light level per cell, computed at the cell centre with the GPU path's distance ramp; floors at level 0 are skipped, walls are not. Acceptable as the meaning of "darkness colormap at the cell light level"?
5. Overlays: the spec's layer list ends with "editor overlays (selection, paths, grid, labels)". Phase 1 draws selection, F9 markers and the grid-over-floor in the canvas, and leaves paths, object labels, grid-over-walls, cursor, walkable info and the HUD on Allegro on top, reading "Allegro UI drawn on top temporarily" as covering them. Confirm.
6. WARP: Direct3D 9 cannot be pointed at WARP on a machine that has a GPU. The plan's WARP lane is Mesa's d3d12 driver on the Microsoft Basic Render Driver (`GALLIUM_DRIVER=d3d12`, `MESA_D3D12_DEFAULT_ADAPTER_NAME`) plus the hosted GitHub Windows runner, which only has the Basic Render Driver. The adapter-selection variable needs confirming against the Mesa build used.
7. "Six core maps plus one per act": the six core maps already cover acts 1-5, so the plan uses that one set. Add more maps?

**Risks.**

- Concurrent GPU-path work edits the same functions (listed under Global Constraints). Anchors quoted here may drift.
- Headless 1:1 renders of the largest maps need a full-map 32-bit bitmap plus an index canvas and tag plane in a Win32 process (roughly 40M pixels, ~250 MB together for act1_town's first DS1); allocation can fail in a fragmented 2 GB address space. If it does, render the parity case in horizontal bands (map_sw already clips to any view) rather than lowering the zoom, which would fall under the zoom rule.
- Present pacing uses `al_rest`, which on Windows sleeps in ~15.6 ms steps by default; without vsync and without `--uncapped`, interactive frame rates can quantize below 60. The gate runs `--uncapped`, so it measures the renderer, not the sleep.
- `display_device.c` calls the D3D9 device from the main thread while Allegro's Direct3D driver owns a display thread; it only reads adapter information, but watch for a hang on first run.
- The WARP lane depends on a Mesa build with the d3d12 driver; if it binds anything else the lane reports a device mismatch instead of a number.
- `spritecache_clear()` on every `dt1_free`/`anim_destroy_cof` empties the cache on map switches; the first frames after a switch rebuild their RLE copies (bounded by what is visible).
- The software path still builds every GPU tile and sprite bitmap at load until phase 4, so its memory use and load time are not yet representative.

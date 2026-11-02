# Changelog

Notable changes to DS1Edit. Releases before 2.1.1 are described on the
[GitHub releases page](https://github.com/bethington/d2-ds1-edit/releases).

## 2.2.1 (unreleased)

### Fixed

- **`--selftest` no longer fails healthy runs on a busy machine.** The frame-time rule that backs up the MEMORY-bitmap draw guard compared the mean *wall* time of a frame with 250 ms. With parallel test lanes, frames waited seconds for a core (act1_town at 1:8: about 50 ms of CPU per frame, 200 ms to several seconds of wall time), and roughly one windowed GPU run in 150 exited 1 with nothing wrong. The rule now uses the main thread's CPU time per frame, which a host under load does not inflate; the direct MEMORY-bitmap guard is unchanged. The selftest summary and `--selftest-report` JSON now include it (`frame cpu ms avg`, `frame_cpu_ms_avg`).

## 2.2.0

### Added

- **Install auto-detection, including Project Diablo 2.** With `d2_install`
  blank the editor now finds Diablo II from, in order, `HKCU\Software\Blizzard
  Entertainment\Diablo II`, the HKLM key (64-bit and WOW6432Node views), the
  `Diablo II` Uninstall entry (HKCU, HKLM, WOW6432Node), then common folders.
  Trailing slashes are normalised and an invalid registry path falls through to
  the next step. Project Diablo 2 is found from `pd2_dir`, its Uninstall entry
  or the `ProjectD2` folder next to Diablo II; an automatically found PD2 is
  used only when it lives in the selected Diablo II install (otherwise startup
  logs that it was not used), while an explicit `pd2_dir` always applies. New `use_pd2 = auto | yes | no`
  (default `auto`): when PD2 is in use its archives (`pd2data`, `pd2maps`,
  `pd2assets`, `pd2monchars`, `ProjectD2\patch_d2`) are searched after `mod_dir`
  and before `patch_d2`, `d2exp`, `d2data`, `d2char`. Startup logs which data
  set is active. Windows reads the registry; Linux and macOS probe common
  folders. The CLI gains `--pd2-dir=` and `--use-pd2=`. The scripted vanilla
  lanes (smoke, render cases, frame-rate test) pin `use_pd2 = no`.

- **Project Diablo 2 compatibility lane (local only).** `scripts/equivalence/pd2_lane.py`
  (CTest label `pd2`, registered only with `-DDS1EDIT_PD2_DIR=...`, never in CI)
  binds results to the SHA-256 of the PD2 archives and reads every PD2 map, table,
  decoder input and a fixed headless render set through the editor; it never
  commits PD2 art. See BUILDING.md.

- **Frame-rate test.** `scripts/perf/fps_test.py` measures render time per map,
  driver and window size, fails on memory-bitmap draws or >16.7 ms at 1080p,
  and can profile failing cases with WPR or perf. Local CTest `perf_fps`.

- **Arrow keys and WASD scroll the map.** **Ctrl+arrows** browse maps (plain
  arrows used to).

- **Ctrl+Z undoes map edits** (while the properties panel is editing, it owns
  Ctrl+Z; **Ctrl+U** still undoes tiles). Ctrl+U now acts once per key press
  instead of repeating.

- **Alt+I** selects strictly identical tiles; **Alt-drag** box-selects objects.

- **Palette per map comes from the game tables first**, and maps no table lists
  are resolved by their exact `LvlPrest.txt` path, so a same-named map in
  another folder is no longer confused with them.

- **Old DS1 files load their tiles** through `.tg1` fallback tile loading.

- Build options `DS1EDIT_ENABLE_SANITIZERS` (AddressSanitizer and
  UndefinedBehaviorSanitizer; AddressSanitizer only on MSVC) and
  `DS1EDIT_ENABLE_COVERAGE` (GCC gcov, Clang source-based coverage).

- A much larger automated test inventory: about 70 CTest suites (was 29),
  including Python suites (`py_*`), an adversarial malformed-input campaign
  (6,717 cases), a DS1 sweep over the vanilla corpus, and a replay of recorded
  original-game answers.
- `scripts/run_golden_tests.py` is strict: exit 0 only if every selected map
  passes, 1 for a failure or a skip, 2 for a bad configuration. Each run keeps
  `results.json` under `tmp/golden-runs/`, and `--update` replaces references
  atomically.

- **Fast, check and full test tiers.** `ctest --preset fast` (about 20 s, every
  portable unit suite), `check` (about 35 s, adds the core golden images, a
  seeded 500-case slice of the adversarial campaign and the display self-test)
  and `full` (everything, including the full campaign and the local lanes), with
  matching `ci-*`, `linux-*` and `macos-*` names. See BUILDING.md.
- **Tests no longer take over your desktop.** Test launchers start child
  processes without console windows, and tests that open the editor run with
  `DS1EDIT_TEST_OFFSCREEN=1`, which keeps its window off-screen, non-activating
  and out of the taskbar (never in fullscreen). `scripts/perf/focus_watch.py`
  measures it.

### Changed

- **Edge-of-window mouse scrolling is off by default.** Set
  `mouse_edge_scroll = YES` in `Ds1edit.ini` to restore it.
- Building and testing now need CMake 3.21 and Python 3 with Pillow
  (`pip install -r test/python/requirements.txt`).
- Release packages are built and inspected by CI, and a release publishes only
  those exact packages after a reviewed qualification record is found. See
  `docs/release-readiness.md`.
- The `test_placeholder` suite was removed.

### Fixed

- **Translucent tints no longer draw too dark.** Fades (50 % selection tint, transparent layers, COF 25/50/75 % modes) were tinted with a premultiplied colour under a non-premultiplied blender, so the source was weighted by alpha twice (`0.25*src + 0.5*dst` for a 50 % tint). They now blend as `src*a + dst*(1-a)`. Additive, multiply, screen and shadow draws are unchanged. Four core golden images were regenerated; only translucent-tint pixels changed.
- **Zero-layer COFs compose to nothing instead of failing.** Project Diablo 2 ships
  three COFs with a layer count of 0 (`zznuhth`, `zzonhth`, `zzophth`); the map
  renderer accepted them but the export composer rejected them. They now parse as
  valid-but-empty, and exporting one writes no file and logs
  `compose: <cof> has no layers; nothing to export`. Every other COF check is
  unchanged (a header claiming more than 16 layers, such as `amblxbow.cof`, and
  truncated headers are still rejected).
- **The GPU path is guarded against silent slowdowns.** Every bitmap draw now
  checks for a memory bitmap on a video target and warns once; `--selftest`
  fails on any, and reports frame-time percentiles (`--selftest-report=`). The
  display-lost/found events restore bitmaps to the GPU, and exit frees them
  first (OpenGL exit 2.6 s to 0.14 s on a large map). This should also remove
  the Direct3D fullscreen exit hang, which is not yet re-verified.
- **Saving a map is safer.** A save now goes through a staging file, keeps a
  backup, and rolls back if anything fails. A failed save shows an error and
  leaves the editor open with your edits intact; it no longer ends the program.
- **Corrupt or truncated maps no longer crash the editor.** The DS1 reader
  checks every length and offset before using it. Opening a damaged map from
  the area browser now shows an error instead of exiting. All 2,276 DS1 files in
  vanilla 1.13c read and resave cleanly, and 2,238 of them are identical to what
  the original game's loader produces; the other 38 are the next item.
- **Objects outside the map grid are kept.** Objects beyond the map's width x 5
  by height x 5 grid (80 objects in 38 vanilla maps) used to be dropped on
  load. They are now kept, drawn with a warning tint, selectable and saved, and
  all 2,276 vanilla maps match the original game's loader.
- **Undo handles large edits and large maps.** Undo records used 8-bit counts
  and coordinates, which broke runs of more than 255 tiles and maps wider than
  256. They are now 32-bit.
- **Delete with nothing selected no longer creates a ghost object.**
- **Saving a project copy** handles sibling folders that share a name prefix,
  over-long paths and trailing separators, and a project copy now finds its
  preset.
- **Maps outside the area lists keep their folder's Act palette.** A DS1 with no
  preset or LvlTypes row and no act field of its own had been drawn with the Act 1
  palette; the browser group's Act is now used as a last resort.
- **The core golden screenshots are rebuilt from vanilla 1.13c inputs.** Five of
  the six references were regenerated (the old ones encoded the previous tile
  variant choice, and `act5_town` was a PD2 mod render); all six pass at
  tolerance 4.
- **Tile variants are picked by rarity as the game does**
  (`misc_check_tiles_conflicts`). Some maps drew a different variant of a tile
  than the original game.
- **Sprites and tiles decode correctly in edge cases.** One raw index-frame
  decode path now serves DCC, DC6 and DT1 (no more RGBA-to-palette recovery).
  Fixed: DCC 32-bit signed fields; a crash on a malformed DCC header; a DCC
  sub-stream overrun; AnimData lookup (file extension, bucket position, overrun;
  names are compared exactly after uppercasing, as the game does); DT1 RLE
  overrun and an offset crash; stale transparent pixels in DT1 tiles; two DT1
  32-bit wraparounds; DT1 sub-tile length now read as 16 bits; DT1 flags bit 0
  accepted; DT1 `roof_y` zero-extended; a block-table scan that read past the
  end.
- **Exports:** APNG frame delays no longer overflow, a failed APNG export
  removes its partial file, and transparent PNG pixels are written
  deterministically.
- **Palette:** the PL2 channel order is red, green, blue, confirmed against the
  running game. Nearest-colour matching agrees with the original D2CMP 1.13c on
  all 336 test vectors. The cached palette never overrides the archive palette,
  and the colour-map cache is keyed by palette and gamma.
- **Map memory leaks** in `ds1_free` fixed, along with a `dcc_destroy` heap
  overflow found by AddressSanitizer and an undefined-behaviour call to
  `mod_load_in_mem` (mismatched prototype).
- **The editor was unusably slow on Direct3D (under 1 frame per second)** on
  some machines: the built-in font glyphs created before the window stayed in
  system memory. They are now converted to video bitmaps once the display
  exists. `--selftest` now prints the average render time and fails above 250 ms
  per frame.
- **Windows package starts on a clean Windows 10 or later** without a separate
  Visual C++ Redistributable: the x86 `vcruntime140.dll` now ships in the ZIP
  (it needs the Universal C Runtime that Windows 10 already includes). The
  generated `assets/editor/version` stamp is no longer packaged.
- **macOS package** now contains `ds1edit.app` beside `assets/`, and its
  `Info.plist` carries the real version number instead of an empty one. Not yet
  verified on a Mac.
- `I` key repeat condition fixed.

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

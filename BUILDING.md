# Building d2-ds1-edit

## Prerequisites

- **CMake** 3.21 or later (needed for `RUNTIME_DEPENDENCY_SET`)
- **Visual Studio 2019** (or later) with C compiler
- **vcpkg** for Allegro 5 dependency management
- **Python 3** with the test dependencies (`pip install -r test/python/requirements.txt`,
  which is Pillow). Python runs the `py_*` CTest suites and the golden screenshot
  runner. CMake registers a failing `py_unavailable` test if it cannot find Python,
  so a missing interpreter shows up as a red run rather than a quiet gap.
- Linux and macOS: Allegro 5 from the system package manager (see the README).
  The Clang/GCC instrumented lanes below also need `clang` or `gcc`, and `xvfb`
  on a headless Linux machine.

## Install vcpkg and Allegro 5

```bash
# Clone and bootstrap vcpkg (if not already installed)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# Install Allegro 5 (32-bit)
C:\vcpkg\vcpkg.exe install allegro5:x86-windows
```

## Build

```bash
# Configure (from project root)
cmake --preset default

# Build (dev = optimized with debug symbols)
cmake --build --preset dev

# Run tests: the test preset uses the Release configuration, so build it first
cmake --build --preset release
ctest --preset default
```

On Linux and macOS use `cmake --preset linux` / `macos`, `cmake --build --preset
linux` / `macos`, and `ctest --preset linux` / `macos` (Release, Ninja). CI uses
the `ci` preset on Windows (Ninja plus MSVC) and then packages with
`cpack --config build/CPackConfig.cmake -B dist`.

The executable is output to `bin/ds1edit.exe`. Allegro 5 DLLs are automatically copied to `bin/` (and beside the test executables under `test/<config>/`, so they run from a build tree). Runtime data files (`data/` and `assets/`, which includes the editor's UI images under `assets/ui/`) are also copied to `bin/` by a post-build step.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_SOFTWARE_RENDERER` | OFF | Force software rendering (no GPU) |
| `DS1EDIT_PERF_LOG` | OFF | Enable per-frame perf logging to stderr and perf_log.csv |
| `DS1EDIT_ENABLE_SANITIZERS` | OFF | AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang; AddressSanitizer only on MSVC 19.28+). Instrumented lane only, never a Release/perf build |
| `DS1EDIT_ENABLE_COVERAGE` | OFF | GCC `--coverage` (read with `gcov --json-format`) or Clang source-based coverage (`llvm-cov export`). Needs `-DCMAKE_BUILD_TYPE=Debug`; mutually exclusive with sanitizers |

Example: `cmake --preset default -DUSE_SOFTWARE_RENDERER=ON`

Instrumented builds use their own build tree, for example:

```bash
# Linux: Clang ASan + UBSan, run the portable suites under a virtual display
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug       -DCMAKE_C_COMPILER=clang -DDS1EDIT_ENABLE_SANITIZERS=ON
cmake --build build-asan
xvfb-run -a ctest --test-dir build-asan -L portable --output-on-failure

# Linux: GCC coverage; summarise with the repository's own tool
cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug       -DCMAKE_C_COMPILER=gcc -DDS1EDIT_ENABLE_COVERAGE=ON
```

`python -m scripts.equivalence.coverage_report` reads the `gcov --json-format` or
`llvm-cov export` output and prints an honest line/branch summary: uninstrumented
production files are listed and make the result `partial`, never counted as
covered. Unsupported compiler combinations stop CMake at configure time. No local
coverage numbers exist yet; `.github/workflows/build.yml` runs both lanes.

## Running

```bash
cd bin

# Single DS1 file
ds1edit.exe <file.ds1> <LvlTypes.txt ID> <LvlPrest.txt DEF>

# Multiple DS1 files via INI
ds1edit.exe <file.ini>

# Headless screenshot (no display window)
ds1edit.exe <file.ds1> <ID> <DEF> --headless output.png
```

## Game Data

The editor needs Diablo II tile data to function. Place your DS1/DT1 files under `assets/tiles/` organized by Act (this directory is gitignored). The INI files in `assets/` define which tile files to load for each area.

## Testing

### GUI smoke test

```bash
cd bin
ds1edit assets/act5_town.ini --selftest        # 30 frames, then exit 0
ds1edit --area "Act 1 - Town" --selftest=120   # or pick the frame count
```

`--selftest` takes the ordinary GUI path -- create the display, load assets,
load the map, run the event loop -- renders N frames and exits 0. It cannot
tell you the picture is *correct*, but it catches a window that opens and
never paints. It also prints a frame-time summary and fails above 250 ms
per frame, or if any memory bitmap was drawn onto a video target (the editor
falling off the GPU, or on Direct3D glyph bitmaps left in system memory).

Worth running before any release: nothing else in the project exercises the
display. The unit tests are headless, `--headless` never creates a display,
and the golden screenshots do not run in CI. A startup change that hung the
editor once shipped precisely because it was a no-op in every test that
existed.



### Frame-rate test

`scripts/perf/fps_test.py` runs `--selftest` over a matrix of maps x drivers x
window sizes, each run in its own owned runtime (exe, DLLs, assets, a generated
`Ds1edit.ini` with `full_screen = NO`, and an `allegro5.cfg` naming the driver).
Windowed only; it needs a display, a GPU and a vanilla 1.13c install. Every owned
`Ds1edit.ini` (this test, native smoke, render cases) sets `use_pd2 = no` so
auto-detected Project Diablo 2 data can never reach vanilla evidence.

```bash
python scripts/perf/fps_test.py --exe bin/ds1edit.exe
python scripts/perf/fps_test.py --exe bin/ds1edit.exe --drivers opengl     --maps act1_town --size 1920x1080 --runs 5 --frames 240
cmake -B build -DDS1EDIT_GAME_DIR=F:/.../LoD/1.13c && ctest --test-dir build -L perf
```

Defaults: the six core maps, `direct3d,opengl` on Windows (`opengl` elsewhere),
1280x720 and 1920x1080, 120 frames, 3 runs. Output goes to
`tmp/perf/fps-<timestamp>/` (`summary.json`, `summary.txt`, per-run folders with
the editor's stdout/stderr and `report.json`, schema `ds1edit.fps/1`).
`--selftest-report=PATH` on the editor writes the same JSON for one run.

Reading the table (times in ms, per frame, median over the runs):

| column | meaning |
| --- | --- |
| render avg / p95 / p99 | composing the frame: tiles, sprites, HUD, not the swap |
| present | `al_flip_display`, which includes the vsync wait |
| wall fps | frames over wall-clock time, vsync-limited |
| exit ms | shutdown time (bitmaps freed, display destroyed) |
| slowest | the largest per-frame section of the editor's perf timers |
| baseline | `none`, `ok`, `regressed`, `unreviewed`, `incomparable`, `no-entry` |

A case FAILS on a nonzero exit or timeout, a missing report, any
memory-bitmap draw (a memory bitmap onto a video target), median render avg
above 16.7 ms at 1080p, or a more than 25 % regression against
`test/equivalence/profiles/fps-baseline.json`. The baseline only judges when it
is `"reviewed": true` and its host (CPU/OS/GPU) and build identity match;
otherwise the column says `unreviewed` or `incomparable` and never passes or
fails the case. Record a new baseline with `--write-baseline PATH` (always
written `"reviewed": false`; a maintainer flips it after checking the numbers).

`--profile-on-fail` (or `--profile` for every case) reruns the case under
Windows Performance Recorder (`wpr -start CPU -filemode`, `wpr -stop
<run>/trace.etl`, then `xperf -i trace.etl -symbols -a profile -detail`) or
`perf record -g` on Linux, and writes the top 20 functions to
`<run>/hotspots.txt`; both are referenced from `summary.json`. WPR usually
needs an elevated shell: if it refuses, the case records status `unavailable`
with WPR's message instead of prompting. Point `--symbols-dir` at a
RelWithDebInfo build's PDBs for symbol names.

Manual profiling recipe when the table shows a slowdown:

1. Build with `DS1EDIT_PERF_LOG` and read the per-section timers
   (events / input / render / present / ...) to see which section moved.
2. Run the fps_test matrix on the suspect and the last good commit
   (`git archive <rev> | tar -x -C tmp/bisect-<rev>`, build each) and bisect.
3. Profile the bad build: WPR + WPA on Windows, `perf` on Linux, Instruments
   (Time Profiler) on macOS.
4. If the CPU is idle but frames are slow, capture a frame in RenderDoc (OpenGL)
   or PIX (Direct3D) and look for per-draw locks and texture uploads.

### Test tiers: which preset when

Three test presets per build family, so a typical change does not pay for the
whole suite. Measured on an 8-thread Windows machine (`-j8`):

| Preset | Runs | Wall time |
|--------|------|-----------|
| `fast` | every portable unit suite (74): no display, no game data, no long campaign | about 20 s |
| `check` | `fast` + `golden_core`, `display_selftest`, `adversarial_slice` (and `parity_quick` when present) | about 35 s |
| `full` | everything, including the full `adversarial_core` campaign (about 3 min alone), `perf_fps` (when `DS1EDIT_GAME_DIR` is set) and the pd2 lane (`DS1EDIT_PD2_DIR`) | about 3.5 min (was 186 s before the tiers existed; the extra tests add the rest) |

Use `fast` while you work (before every commit), `check` before you push or
open a PR, `full` before a release or when you touched the parsers/writers the
campaign covers. The same names exist per family: `fast`, `check`, `full` for
the default (Visual Studio) family and `ci-fast`, `linux-fast`, `macos-fast`
(and `-check`, `-full`) for the others. The existing `default`, `ci`, `linux`
and `macos` test presets are unchanged and still run everything; CI keeps using
them (the workflows were not changed).

```bash
cmake --build --preset release && ctest --preset fast
ctest --preset check          # needs -DDS1EDIT_GOLDEN_RUNTIME, see below
ctest --preset full
```

**Label scheme** (CTest `LABELS`, set in `test/cmake/*.cmake`, finished by
`test/cmake/tiers.cmake`):

| Label | Meaning |
|-------|---------|
| `fast` | added automatically to every test that carries none of the labels below, so a new unit test is fast by default |
| `check` | the beyond-fast tier: `golden_core`, `display_selftest`, `adversarial_slice` |
| `slow` | the full adversarial campaign (`adversarial_core`); `full` only |
| `display` | creates a window; CTest sets `DS1EDIT_TEST_OFFSCREEN=1` for it |
| `local` | needs a local runtime (`DS1EDIT_GOLDEN_RUNTIME`) or game data |
| `perf`, `pd2` | the existing local-only lanes |
| `portable` | unchanged; CI selects it (`-L portable`) |
| `parity_quick` | hook for the headless software-renderer parity test (it lives on another branch): a test that carries this label is promoted to `check` automatically and kept out of `fast` |

**`adversarial_slice`** runs the same campaign as `adversarial_core` through
`python -m scripts.equivalence.adversarial run --max-cases 500 --seed 20261023`:
every control and valid-sample case plus a reproducible, seeded sample of the
mutated cases, so a failure reproduces from its case id. Change the size or
seed with `-DDS1EDIT_ADVERSARIAL_SLICE_CASES=` / `-DDS1EDIT_ADVERSARIAL_SLICE_SEED=`.

**`golden_core` and `display_selftest`** need a vanilla 1.13c runtime you own:
a directory with `ds1edit.exe`, its DLLs, `assets/` and a `ds1edit.ini` that
points at the game archives (never the repository's own `bin/ds1edit.ini`).

```bash
cmake -S . -B tmp/build -DDS1EDIT_GOLDEN_RUNTIME=C:/path/to/runtime ...
```

They run the executable *in that directory*, which this build does not
rebuild: copy a fresh `bin/ds1edit.exe` over it when the code under test
changes (a stale executable fails the exact-match golden comparison). Without
`DS1EDIT_GOLDEN_RUNTIME` both are still registered, as skip stubs: CTest
reports `Not Run (Skipped)` with "not configured (pass -DDS1EDIT_GOLDEN_RUNTIME=...)",
never a silent pass and never a failure.

**Tests do not take your desktop.** Every Python launcher starts its child
processes with `CREATE_NO_WINDOW` (`scripts/winproc.py`), so no console flashes.
Tests that create the editor window run with `DS1EDIT_TEST_OFFSCREEN=1`
(set by CTest for `display`-labelled tests and `golden_core`, and by default in
`scripts/equivalence/native_smoke.py`; pass `--visible` to opt out). With the
switch the editor creates a frameless window beyond the edge of the virtual
desktop, makes it non-activating and a tool window (no taskbar or Alt-Tab
entry), refuses activation, and hands focus back if anything still took it.
Rendering, presents and `--selftest` are unaffected (verified on Direct3D and
OpenGL). Never honoured in fullscreen. `scripts/perf/fps_test.py` stays
**visible by default** because Windows may throttle an occluded window;
`--offscreen` opts in and `summary.json` records `window_mode`.
`scripts/perf/focus_watch.py` checks the claim:

```bash
python scripts/perf/focus_watch.py --strict -- ctest --test-dir tmp/build -C Release -L "fast|check"
```

It samples `GetForegroundWindow()` and the top-level windows of the test
processes, and reports how many samples had a test process in the foreground
and how many test windows would appear on the taskbar (target: 0 and 0).

### CTest and the Python suites

```bash
ctest --preset default                       # every suite (about 70)
ctest --preset default -L portable           # only suites that need no game files
ctest --preset default -N                    # list them; py_golden_runner must be present,
                                             # py_unavailable / py_missing must not be
python -m unittest discover -s test/python -t . -p "test_*.py"   # Python suites alone
```

`py_*` suites are registered automatically from `test/python/test_*.py` by
`test/cmake/python.cmake`, run from the repository root, and carry the
`portable` label. They use authored fixtures and mocked subprocesses only.

### Golden screenshot comparison

```bash
python scripts/run_golden_tests.py --core --tolerance 4
python scripts/run_golden_tests.py --full --tolerance 4
```

The runner is a strict gate: exit **0** only if every selected map passes,
**1** if any map fails or is skipped (missing reference or INI included), **2**
for a bad command line or configuration. Every run writes a fresh
`tmp/golden-runs/run-*/` with `results.json` and the rendered images;
`--update` rewrites references atomically and should only be used after the
difference is explained. Pin the inputs (profile `visual-core.json` through
`scripts/equivalence/render_cases.py`) before trusting a comparison.

### Local-only equivalence lanes

Lanes that need a licensed Diablo II 1.13c installation (the vanilla corpus
sweep, `baseline.py`) or a live original-game reference (`capture_palette.py`)
are never run in public CI. They need `pip install -r
test/equivalence/requirements-local.txt` (adds `mpyq`); see
[test/equivalence/README.md](test/equivalence/README.md). The recorded replay
`equivalence_palette_replay` is portable and runs in CTest.

### Project Diablo 2 lane (local only)

`scripts/equivalence/pd2_lane.py` checks the editor against the Project Diablo 2
installation on your machine: archive identity and version, an inventory of
every PD2 DS1/DT1/TXT/DCC/DC6/COF (unique, overriding vanilla, or identical),
every PD2 map through the reader and checked writer, the Levels/LvlPrest/
LvlTypes/Objects/MonStats tables and the area browser, every PD2 DCC/DC6/DT1/COF
through the decoders, and a fixed headless render set compared with a local
baseline. PD2's art is not redistributable, so the lane is never run in CI, is
registered only when you set `DS1EDIT_PD2_DIR`, and writes only under `DS1EDIT_PD2_OUT`
(default `<build>/pd2`; set it to `tmp/pd2` to keep one baseline across build directories).

```bash
cmake -S . -B tmp/build-pd2 ... -DDS1EDIT_PD2_DIR=C:/Diablo2/ProjectD2 -DDS1EDIT_PD2_OUT=tmp/pd2   # optional -DDS1EDIT_PD2_BASE_DIR, -DDS1EDIT_VANILLA_MANIFEST
cmake --build tmp/build-pd2 --config Debug
ctest --test-dir tmp/build-pd2 -C Debug -L pd2 --output-on-failure           # pd2_inventory, pd2_sweep, pd2_render
```

or directly (`inventory | sweep | render | all`, all paths explicit):

```bash
python -m scripts.equivalence.pd2_lane all --pd2-dir C:/Diablo2/ProjectD2 --d2-dir C:/Diablo2     --exe bin/ds1edit.exe --probe-ds1 <build>/test/Debug/test_ds1_reference_probe.exe     --probe-visual <build>/test/Debug/test_visual_reference_probe.exe     --probe-robust <build>/test/Debug/test_adversarial_probe.exe
```

Needs `mpyq` (`test/equivalence/requirements-local.txt`). Results are
`ds1edit.case/1` files in `tmp/pd2/run-<timestamp>/cases/` with a summary table;
the first render run creates `tmp/pd2/baseline/` marked `reviewed: false`. A
baseline for another PD2 version or archive set is reported as `incomparable`,
never as a pass or a failure. The unit tests (`py_pd2_lane`) use fakes and run
everywhere.

### Packages

```bash
cpack --config build/CPackConfig.cmake -B dist        # ZIP on Windows, TGZ elsewhere
python -m scripts.equivalence.package_check --archive dist/DS1Edit-<ver>-<system>.<ext>     --platform windows-x86|linux-x86_64|macos-arm64 --version <ver> --out-dir tmp/package-check
```

`package_check` inspects the archive (layout, no game data, architecture and
embedded version, runtime closure) before extracting it. The Windows ZIP carries
the x86 `vcruntime140.dll`, so it needs Windows 10 or later (Universal C Runtime)
but not a separate Visual C++ Redistributable. The macOS package holds
`ds1edit.app` beside `assets/` with the project version in its `Info.plist`; its
`codesign --verify` and version checks run on the macOS CI job, and the package
has not been verified on a Mac outside CI. Releases publish only the packages CI
built and qualified; see [docs/release-readiness.md](docs/release-readiness.md)
and [release/qualification/README.md](release/qualification/README.md).

## Project Structure

```
src/                C source files
  main.c            Entry point
  config.c/h        INI creation and reading
  core/             File format parsers (DS1, DT1, COF, palette, etc.)
  render/           Tile rendering pipeline
  editor/           Editing operations (tiles, objects, paths, undo)
  ui/               User interface (event loop, dialogs, windows)
  mpq/              MPQ archive reader
data/               Palettes, gamma tables, editor tile data
assets/
  ui/               Editor interface images (buttons, tabs, cursors,
                    walkable-info bits, window frames)
assets/             Area INI configs, excel tables, palette data
  tiles/            Game tile data (gitignored, user-supplied)
test/
  unity/            Unity test framework (ThrowTheSwitch)
  golden/           Golden reference screenshots (PNG)
  test_*.c          Unit test files
  python/           Python unittest suites (registered as py_* CTest tests)
  cmake/            CTest registration helpers (python, equivalence, ...)
  equivalence/      Contracts, profiles and recorded reference results
scripts/
  equivalence/      Equivalence, qualification, package and coverage tools
  run_golden_tests.py     Automated render+compare workflow (strict exit codes)
  capture_golden.py       Capture reference screenshots
  compare_golden.py       Pixel-level image comparison
bin/                Build output (gitignored)
```

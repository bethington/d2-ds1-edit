# Building d2-ds1-edit

## Prerequisites

- **CMake** 3.20 or later
- **Visual Studio 2019** (or later) with C compiler
- **vcpkg** for Allegro 5 dependency management
- **Python 3** with Pillow (`pip install Pillow`) for golden screenshot tests

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

# Run tests
ctest --preset default
```

The executable is output to `bin/ds1edit.exe`. Allegro 5 DLLs are automatically copied to `bin/`. Runtime data files (`data/` and `assets/`, which includes the editor's UI images under `assets/ui/`) are also copied to `bin/` by a post-build step.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_SOFTWARE_RENDERER` | OFF | Force software rendering (no GPU) |
| `DS1EDIT_PERF_LOG` | OFF | Enable per-frame perf logging to stderr and perf_log.csv |

Example: `cmake --preset default -DUSE_SOFTWARE_RENDERER=ON`

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
never paints.

Worth running before any release: nothing else in the project exercises the
display. The unit tests are headless, `--headless` never creates a display,
and the golden screenshots do not run in CI. A startup change that hung the
editor once shipped precisely because it was a no-op in every test that
existed.



```bash
# Unit tests
ctest --preset default

# Golden screenshot comparison (from project root)
python scripts/run_golden_tests.py --core --tolerance 4
python scripts/run_golden_tests.py --full --tolerance 4
```

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
scripts/
  run_golden_tests.py     Automated render+compare workflow
  capture_golden.py       Capture reference screenshots
  compare_golden.py       Pixel-level image comparison
bin/                Build output (gitignored)
```

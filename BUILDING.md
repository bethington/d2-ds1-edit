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
mkdir cmake-build && cd cmake-build
cmake .. -G "Visual Studio 16 2019" -A Win32

# Build
cmake --build . --config Debug

# Run tests
ctest -C Debug --output-on-failure
```

The executable is output to `bin/ds1edit.exe`. Allegro 5 DLLs are automatically copied to `bin/`.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_SOFTWARE_RENDERER` | OFF | Force software rendering (no GPU) |
| `VCPKG_ROOT` | `C:/vcpkg` | Path to vcpkg installation |

Example: `cmake .. -DUSE_SOFTWARE_RENDERER=ON`

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

## Testing

```bash
# Unit tests (from cmake-build/)
ctest -C Debug

# Golden screenshot comparison (from project root)
python scripts/run_golden_tests.py --core --tolerance 4
python scripts/run_golden_tests.py --full --tolerance 4
```

## Project Structure

```
Sources/          C source files
  a5_compat.h     Allegro 4->5 compatibility layer
  palette.c/h     RGBA palette system
  rgba_cache.c/h  Hybrid index+RGBA tile cache
  dt1_decode.c    Allegro-independent tile decoding
test/
  unity/          Unity test framework (ThrowTheSwitch)
  golden/         Golden reference screenshots (PNG)
  test_*.c        Unit test files
scripts/
  capture_golden.py       Capture reference screenshots
  compare_golden.py       Pixel-level image comparison
  run_golden_tests.py     Automated render+compare workflow
  convert_golden_to_png.py  BMP->PNG conversion utility
bin/
  assets/         Game tile data and INI configurations
  pcx/            UI element images (PNG format despite directory name)
  data/           Runtime data files (palettes, gamma)
```

# DS1Edit - Diablo II Level Editor

A level editor for Diablo II that allows creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering at 60+ FPS. Runs on Windows, Linux, and macOS.

## Download

**[Download the latest release](https://github.com/bethington/d2-ds1-edit/releases/latest)** — Windows, Linux, and macOS builds are published for every tag.

| Platform | Package | Runtime dependency |
|----------|---------|--------------------|
| Windows (x86) | `ds1edit-<ver>-windows-x86.zip` | none — Allegro 5 DLLs are bundled |
| Linux (x86_64) | `ds1edit-<ver>-linux-x86_64.tar.gz` | system Allegro 5 (see below) |
| macOS (arm64) | `ds1edit-<ver>-macos-arm64.tar.gz` | `brew install allegro` |

Unpack anywhere and run `ds1edit` (`ds1edit.exe` on Windows). There is no installer.

**Linux** needs Allegro 5 from your package manager:

```bash
# Debian / Ubuntu
sudo apt install liballegro5.2 liballegro-image5.2 liballegro-ttf5.2 liballegro-dialog5.2

# Fedora
sudo dnf install allegro5 allegro5-addon-image allegro5-addon-ttf
```

**macOS** builds are not code-signed, so Gatekeeper will refuse the first launch.
Either right-click the binary and choose Open, or clear the quarantine flag:

```bash
xattr -dr com.apple.quarantine ds1edit
```

You need your own copy of Diablo II (Classic or LoD) — the editor reads tiles and sprites straight out of your MPQs and ships no game assets.

## Quick Start

```bash
# Prerequisites: Visual Studio 2019+, vcpkg with Allegro 5
vcpkg install allegro5:x86-windows

# Build
cmake --preset default
cmake --build --preset release

# Configure (first run)
cd bin
copy Ds1edit.ini.sample Ds1edit.ini
# Edit Ds1edit.ini to set your Diablo II MPQ paths

# Run
bin\ds1edit.exe assets\act5_town.ini
```

## Features

- **DS1 Map Editing** - Create and modify Diablo II level files for Acts 1-5
- **GPU-Accelerated Rendering** - VIDEO bitmap pipeline with ~1ms render time per frame
- **D2 Blend Modes** - Correct additive, multiply, and screen blending (reverse-engineered from D2CMP.dll)
- **Object Animation** - DCC/DC6 sprite rendering with shadow projection
- **Multi-Zoom** - 7 zoom levels from 1:1 to 1:16
- **Headless Mode** - Command-line screenshot rendering for CI/testing
- **Performance Profiling** - Optional per-frame and per-section timing (`-DDS1EDIT_PERF_LOG=ON`)

## Building

```bash
# Windows -- vcpkg + Visual Studio
cmake --preset default           # Configure (uses vcpkg toolchain)
cmake --build --preset dev       # Dev build (optimized + debug symbols)
cmake --build --preset release   # Release build (full optimization + LTCG)

# Linux -- Ninja + system Allegro 5
sudo apt install build-essential ninja-build cmake pkg-config zlib1g-dev      liballegro5-dev liballegro-image5-dev liballegro-ttf5-dev liballegro-dialog5-dev
cmake --preset linux && cmake --build --preset linux

# macOS -- Ninja + Homebrew Allegro 5
brew install allegro ninja pkg-config
cmake --preset macos && cmake --build --preset macos
```

### Build Presets

| Preset | Config | Use |
|--------|--------|-----|
| `dev` | RelWithDebInfo | Day-to-day development, F5 debugging |
| `release` | Release | Distribution, maximum performance |
| `debug` | Debug | Deep debugging (slower rendering) |

### VSCode Integration

- **F5** builds and launches with the `dev` preset
- Launch dropdown has configs for every Act/area
- "Run DS1Edit" configs launch without debugger for full speed

## Testing

```bash
ctest --preset default                          # Unit tests (4 tests, <0.2s)
python scripts/run_golden_tests.py --core       # Golden screenshot tests
```

### Unit Tests
- `test_palette` - Palette color conversion and matching
- `test_rgba_cache` - Tile cache create/rebuild/convert
- `test_dt1_decode` - DT1 sub-tile pixel decoder
- `test_placeholder` - Test infrastructure validation

### Golden Screenshot Tests
Renders maps in headless mode and compares against reference PNGs in `test/golden/`.

## Command Line

```bash
ds1edit.exe <file.ds1> <ID> <DEF> --headless output.png
ds1edit.exe --list-areas
ds1edit.exe --list-files [filter]
ds1edit.exe --audit-lvltypes
```

`--audit-lvltypes` prints every mismatch between a `LvlTypes.txt` row's `Act X - ...` name prefix and its authoritative `Act` column. Palette selection uses the `Act` column as the source of truth.

## Configuration

After building, copy `Ds1edit.ini.sample` from the project root (or `bin/`) to `bin/Ds1edit.ini` and set your Diablo II MPQ paths:

```ini
d2char   = C:\Diablo2\d2char.mpq
d2data   = C:\Diablo2\d2data.mpq
d2exp    = C:\Diablo2\d2exp.mpq
patch_d2 = C:\Diablo2\patch_d2.mpq
```

See the sample file for all available settings (resolution, scroll speed, gamma, etc.).

## Project Structure

See [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) for detailed layout.

Key directories:
- `src/` - C source code organized into `core/`, `render/`, `editor/`, `ui/`, `mpq/`
- `data/`, `pcx/`, `assets/` - Runtime data (copied to `bin/` at build time)
- `test/` - Unit tests (Unity framework)
- `scripts/` - Golden test scripts (Python)
- `bin/` - Build output directory (gitignored)

## Architecture

### Rendering Pipeline

7-pass compositing pipeline in `src/render/preview.c`, all GPU-accelerated:

1. Base terrain (floors, lower walls, tile shadows)
2. Object shadows (tinted black silhouettes)
3. Objects behind walls
4. Upper walls + objects in front
5. Roofs
6. Special tiles
7. Walkable info overlay

### Timing

- **Game tick**: 25 Hz (animation frame advancement)
- **Display**: Vsync rate (60-165 Hz depending on monitor)
- **Render work**: ~1ms per frame (GPU-bound, not CPU-bound)

### D2 Blend Modes

COF layer transparency (`trans_b` field) mapped to Allegro 5 blender states:

| trans_b | Mode | Formula |
|---------|------|---------|
| 0 | 75% transparent | Alpha 0.25 |
| 1 | 50% transparent | Alpha 0.50 |
| 2 | 25% transparent | Alpha 0.75 |
| 3 | Additive | min(src + dst, 255) |
| 4 | Multiply | (src * dst) / 255 |
| 6 | Screen | src + dst - src*dst/255 |

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes and add tests
4. Run `ctest --preset default` to verify
5. Submit a pull request

## License

[MIT License](LICENSE) - Copyright (c) 2025-2026 Ben Ethington

## Acknowledgments

This project stands entirely on **Paul Siramy's** original `win_ds1edit` — the DS1
map editor that the Diablo II modding community has relied on since 2003. Paul
wrote the DS1/DT1/DCC/DC6 format handling, the WYSIWYG editing model, and the
25,000+ lines this codebase grew out of, and released the source publicly.
Everything here is a continuation of that work, not a replacement for it.

- **Original DS1Edit / win_ds1edit by Paul Siramy** —
  [download page](http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html) ·
  [documentation](http://paul.siramy.free.fr/_divers/ds1/doc/index.html)
- GUI Loader by Mark Nevill ('DarthDevilous')
- The [Phrozen Keep](http://d2mods.com/forum/) community, for two decades of
  file-format documentation
- Built with the [Allegro 5](https://liballeg.org/) game programming library
- D2 blend mode formulas reverse-engineered from D2CMP.dll via Ghidra

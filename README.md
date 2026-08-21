# DS1Edit - Diablo II Level Editor

A level editor for Diablo II that allows creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering at 60+ FPS. Runs on Windows, Linux, and macOS.

DS1Edit is a continuation of [Paul Siramy's `win_ds1edit`](http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html), the Diablo II community's DS1 editor since 2002, ported from Allegro 4 to Allegro 5 and extended. See [Acknowledgments](#acknowledgments) and [NOTICE](NOTICE).

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
- **Reads your MPQs directly** - maps, tiles, sprites, palettes and data tables, with a mod-directory overlay; nothing has to be extracted first
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

Run `ds1edit help` for the full verb list. The area-browser verbs:

```bash
ds1edit list-areas              # every area, grouped by Act
ds1edit list-files [filter]     # DS1 files with their LvlType / Def
ds1edit audit-lvltypes          # LvlTypes rows whose name disagrees with its Act
ds1edit <file.ds1> <ID> <DEF> --headless out.png
ds1edit --area "Act 1 - Town" --headless out.png
```

The legacy `--list-areas` / `--list-files` / `--audit-lvltypes` spellings still work.

`audit-lvltypes` prints every mismatch between a `LvlTypes.txt` row's `Act X - ...` name prefix and its authoritative `Act` column.

### How a map picks its palette

`LvlTypes.txt`'s **`Act` column** is the source of truth, not the row name and not the
DS1. The chain is:

```
LvlPrest.txt (Def)  ->  Levels.txt (LevelType)  ->  LvlTypes.txt (Act + File 1..32)
                                                          |            |
                                                     palette      the DT1 set
```

`palette_resolve_act()` takes that `Act` when it is 1-5, falls back to the DS1's own
act, and defaults to 1. The palette index is `act - 1`, selecting `Act1..Act5/Pal.pl2`
straight from your MPQs.

Maps that no table names -- the maze and room fragments the game assembles
procedurally -- are grouped per Act as **Unlisted**, with their tileset inferred from
the folder (its listed neighbours first, then a group whose name matches the folder).

## Configuration

Copy `Ds1edit.ini.sample` to `Ds1edit.ini` and point it at your Diablo II install:

```ini
d2_install = C:\Diablo2
```

That one line fills every MPQ slot. **Nothing needs extracting** -- maps, tiles,
sprites, palettes and the data tables are all read straight out of the archives, and
the whole map list appears in the browser. Set `mod_dir` and your mod's loose files
take priority over the base game without repacking anything.

Individual archives can still be named explicitly if you need to override the
auto-detected set:

```ini
d2char   = C:\Diablo2\d2char.mpq
d2data   = C:\Diablo2\d2data.mpq
d2exp    = C:\Diablo2\d2exp.mpq
patch_d2 = C:\Diablo2\patch_d2.mpq
```

See the sample file for all available settings (resolution, scroll speed, gamma, etc.).

## Troubleshooting

**The window opens but the map area is black.** The status bar and the mouse
cursor draw, the frame rate looks normal, and nothing appears in the logs.

This is the graphics driver, and switching it fixes it. Which way you switch
depends on how you got the editor.

Allegro picks a backend per platform, and either choice can misbehave on a
given machine: Direct3D has been seen dropping to 0 fps, and OpenGL has been
seen rendering an all-black map exactly as described. An `allegro5.cfg` next
to the executable overrides the choice:

```ini
[graphics]
driver=opengl
```

- **From a release package**, there is no `allegro5.cfg` and you are on the
  platform default. Create the file next to `ds1edit` with `driver=opengl`, or
  `driver=direct3d` on Windows, and try the other one.
- **From a source build**, the repository ships an `allegro5.cfg` that pins
  OpenGL, and CMake copies it next to the executable. Rename or delete it to
  fall back to the platform default.

The editor prints the driver it ended up on at startup, so check `stderr.txt`
(or the console) for:

```text
display: 2560x1440, OpenGL driver
```

## Project Structure

See [docs/structure.md](docs/structure.md) for detailed layout.

Key directories:
- `src/` - C source code organized into `core/`, `render/`, `editor/`, `ui/`, `mpq/`
- `data/`, `assets/` - Runtime data (copied to `bin/` at build time); `assets/ui/` holds the editor's interface images
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

[MIT License](LICENSE) - Copyright (c) 2025-2026 Ben Ethington, covering the
modifications and additions made in this project.

The original `win_ds1edit` code this project grew out of was written by Paul
Siramy and released publicly with every build from 2002 to 2011, without a
formal license text; it is used here with attribution, not relicensed. The MPQ
reader in `src/mpq/` is Tom Amigo's Stormless MPQ sample code. [NOTICE](NOTICE)
has the full attribution and file-by-file provenance, and ships in every release
package next to LICENSE.

## Acknowledgments

This project stands entirely on **Paul Siramy's** original `win_ds1edit` — the DS1
map editor that the Diablo II modding community has relied on since 2002. Paul
wrote the DS1/DT1/DCC/DC6 format handling, the WYSIWYG editing model, and the
25,000+ lines this codebase grew out of, and released the source publicly
alongside every build. Everything here is a continuation of that work, not a
replacement for it. Every source file that descends from `win_ds1edit` carries
a header saying so; [NOTICE](NOTICE) lists them all.

- **Original DS1Edit / win_ds1edit by Paul Siramy** —
  [download page](http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html) ·
  [documentation](http://paul.siramy.free.fr/_divers/ds1/doc/index.html)
- **MPQ reader** (`src/mpq/`) — Tom Amigo's Stormless MPQ sample code (2000),
  as adapted by Paul for `win_ds1edit`
- GUI Loader by Mark Nevill ('DarthDevilous')
- The [Phrozen Keep](http://d2mods.com/forum/) community, for two decades of
  file-format documentation
- Built with the [Allegro 5](https://liballeg.org/) game programming library
- D2 blend mode formulas reverse-engineered from D2CMP.dll via Ghidra

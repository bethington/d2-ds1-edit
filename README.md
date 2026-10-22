# DS1Edit - Diablo II Level Editor

A level editor for Diablo II that allows creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering at 60+ FPS. Runs on Windows, Linux, and macOS.

DS1Edit is a continuation of [Paul Siramy's `win_ds1edit`](http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html), the Diablo II community's DS1 editor since 2002, ported from Allegro 4 to Allegro 5 and extended. See [Acknowledgments](#acknowledgments) and [NOTICE](NOTICE).

## Configure once. Launch. Choose your map.

Point DS1Edit at your Diablo II installation in `Ds1edit.ini`, then launch the
editor and choose maps from its built-in browser. Everyday use needs no
command-line arguments, map IDs, or separate launcher.

![DS1Edit displaying Harrogath, with the Act and map browser on the left and the properties panel on the right](docs/assets/images/editor-harrogath.png)

*The editor in action: browse maps by Act, edit the map, and
inspect its properties in the same window.*

### Why try this continuation?

- **Easier to get started:** one install-folder setting resolves the standard
  game archives; maps and assets load directly from your MPQs.
- **Quicker iteration:** GPU-accelerated rendering and tilesets that stay loaded
  when switching maps. The [2.1.0 release measurements](https://github.com/bethington/d2-ds1-edit/releases/tag/v2.1.0)
  improved first-map loading from 23.3 seconds to 0.5 seconds, and another map
  sharing its tileset from 9.4 seconds to 2 milliseconds compared with earlier
  builds of this continuation.
- **Built-in map discovery:** browse areas grouped by Act, including an Unlisted
  group for map fragments the game tables do not name.
- **A workflow for your mod:** loose files in a configured mod folder take
  priority over base-game assets, without repacking archives.
- **Modern platform support:** Windows, Linux, and macOS builds, plus a map
  viewport that grows when you resize the window.
- **More export options:** full-map PNGs, tile and sprite frame exports, and
  layered animation composition to animated PNG.

<details>
<summary>See a full-map PNG export of Harrogath</summary>

![Full-map Harrogath render exported by DS1Edit](docs/assets/images/harrogath-map-export.png)

*An existing reference render showing full-map PNG output.*

</details>

## Download

**[Download the latest release](https://github.com/bethington/d2-ds1-edit/releases/latest)** — Windows, Linux, and macOS builds are published for every tag.

| Platform | Package | Runtime dependency |
|----------|---------|--------------------|
| Windows (x86) | `ds1edit-<ver>-windows-x86.zip` | none — Allegro 5 DLLs and `vcruntime140.dll` are bundled; needs Windows 10 or later (its Universal C Runtime) |
| Linux (x86_64) | `ds1edit-<ver>-linux-x86_64.tar.gz` | system Allegro 5 (see below) |
| macOS (arm64) | `ds1edit-<ver>-macos-arm64.tar.gz` | `brew install allegro` |

Unpack anywhere and run `ds1edit` (`ds1edit.exe` on Windows, `ds1edit.app` on macOS). There is no installer.

**Windows** needs no separate Visual C++ Redistributable: the package carries the
x86 `vcruntime140.dll`. It does rely on the Universal C Runtime, which is part of
Windows 10 and later.

**Linux** needs Allegro 5 from your package manager:

```bash
# Debian / Ubuntu
sudo apt install liballegro5.2 liballegro-image5.2 liballegro-ttf5.2 liballegro-dialog5.2

# Fedora
sudo dnf install allegro5 allegro5-addon-image allegro5-addon-ttf
```

**macOS** packages contain `ds1edit.app` (the entry point to launch; Input
Monitoring, which Allegro needs for the keyboard, is granted to it) beside the
plain `ds1edit` binary and `assets/`. The app is ad-hoc signed, not notarized,
so Gatekeeper will refuse the first launch. Either right-click the app and
choose Open, or clear the quarantine flag:

```bash
xattr -dr com.apple.quarantine ds1edit.app ds1edit
```

The macOS package has not yet been run on a Mac by the maintainers; see
[docs/release-readiness.md](docs/release-readiness.md).

You need your own copy of Diablo II (Classic or LoD) — the editor reads tiles and sprites straight out of your MPQs and ships no game assets.

## Quick Start

1. [Download the latest release](https://github.com/bethington/d2-ds1-edit/releases/latest)
   for your platform and unpack it. See the runtime dependencies above.
2. Copy `Ds1edit.ini.sample` to `Ds1edit.ini` beside the executable and set your
   Diablo II install folder:

   ```ini
   d2_install = C:\Diablo2
   ```

3. Launch `ds1edit.exe` on Windows, `ds1edit` on Linux, or `ds1edit.app` on macOS, with no arguments.
   Choose a map from the built-in browser.

You do not need to enter a DS1 path, LevelType ID, or LevelDef value to browse
and open maps. Command-line options remain available for automation and direct
file opening. To compile the editor yourself, see [Building](#building).

## Features

- **DS1 Map Editing** - Create and modify Diablo II level files for Acts 1-5
- **GPU-Accelerated Rendering** - VIDEO bitmap pipeline with ~1ms render time per frame
- **D2 Blend Modes** - Correct additive, multiply, and screen blending (reverse-engineered from D2CMP.dll)
- **Object Animation** - DCC/DC6 sprite rendering with shadow projection
- **Multi-Zoom** - 7 zoom levels from 1:1 to 1:16
- **Keyboard navigation** - arrow keys and **WASD** scroll the map; **Ctrl+arrows** browse maps; **Ctrl+Z** / **Ctrl+U** undo. Edge-of-window mouse scrolling is off unless you set `mouse_edge_scroll = YES` in `Ds1edit.ini`
- **Reads your MPQs directly** - maps, tiles, sprites, palettes and data tables, with a mod-directory overlay; nothing has to be extracted first
- **Headless Mode** - Command-line screenshot rendering for CI/testing
- **Performance Profiling** - Optional per-frame and per-section timing (`-DDS1EDIT_PERF_LOG=ON`)

## Building

Requirements: CMake 3.21 or later, a C compiler (Visual Studio 2019 or later on
Windows), Allegro 5, and Python 3 with Pillow for the test suite
(`pip install -r test/python/requirements.txt`). See [BUILDING.md](BUILDING.md).

```bash
# Windows -- Visual Studio 2022 (x86); uses build/vcpkg_installed/x86-windows
cmake --preset default           # Configure the VS2022 build in tmp/build-vs2022
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
pip install -r test/python/requirements.txt     # Python test dependencies (Pillow)
ctest --preset default                          # Windows (VS2022 build): every suite
ctest --preset linux --output-on-failure        # Linux  (or: macos, ci)
ctest --preset default -L portable              # only the portable suites
python -m unittest discover -s test/python -t . -p "test_*.py"  # the Python suites directly
python scripts/run_golden_tests.py --core --tolerance 4   # golden screenshots
```

About 70 CTest suites run: native Unity suites for the parsers, decoders, editor
logic and CLI, and `py_*` Python suites that CMake registers from `test/python/`
(label `portable`). If Python is not found at configure time, a failing
`py_unavailable` test is registered on purpose so a missing interpreter cannot
look like a green run.

Suites labelled `portable` use only authored fixtures and need no Diablo II
files. Suites that need a licensed installation or a live reference are local
lanes: see [test/equivalence/README.md](test/equivalence/README.md) for the
corpus and live-equivalence lanes, which are never run in public CI.

### Golden Screenshot Tests
`scripts/run_golden_tests.py` renders maps in headless mode and compares them
with reference PNGs in `test/golden/`. It is strict: exit **0** only when every
selected map passes, **1** when any map fails or is skipped (a missing
reference or INI is a skip), **2** for an invalid command line or
configuration. Each run writes a new `tmp/golden-runs/run-*/` directory with
`results.json` and the rendered images; `--update` replaces references
atomically. Inputs must be pinned (the `visual-core.json` profile used by
`scripts/equivalence/render_cases.py`) for a comparison to mean anything. At the
moment four of the six core references (`act1_town`, `act2_desert`,
`act3_jungle`, `act4_fortress`) differ from current output because they encode
the old tile-variant selection defect, and `act5_town` was made with a Project
Diablo 2 overlay; they have not been regenerated.

### Unit Tests
Examples: `test_palette` (colour matching), `test_rgba_cache` (tile cache),
`test_dt1_decode` (DT1 sub-tile decoder), `test_ds1_save` and `test_editor_undo`
(save and undo behaviour). Run `ctest --preset default -N` for the full list.

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

That one line fills every MPQ slot. You can even leave it blank: the editor finds
Diablo II from the registry (HKCU, HKLM, the Uninstall list) or a common folder, and
logs what it found at startup. **Nothing needs extracting** -- maps, tiles,
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
`mouse_edge_scroll` (default `NO`) turns on scrolling when the mouse touches the
window border; the arrow keys and **WASD** always scroll, and **Ctrl+arrows**
browse maps.

### Project Diablo 2

If Project Diablo 2 is installed (its `ProjectD2` folder holds `pd2data.mpq`), the editor
finds it on its own and reads PD2's data and maps instead of the vanilla ones:

```ini
; auto (default) | yes (require it) | no (vanilla only)
use_pd2 = auto
; blank = detect; or e.g. C:\Diablo2\ProjectD2
pd2_dir =
```

PD2's archives are searched after `mod_dir` and before the base game's, in this order:
`pd2data.mpq`, `pd2maps.mpq`, `pd2assets.mpq`, `pd2monchars.mpq`, `ProjectD2\patch_d2.mpq`,
then `patch_d2`, `d2exp`, `d2data`, `d2char`. PD2 ships no readable load list, so this order
is the editor's assumption: the first four are the order PD2's own DLL names them, with the
folder's patch last. `D2MultiRes`, `SGD2FreeRes`, `d2gl` and `ver-IX86-1` only hold display
helpers and are not opened. Startup prints a line such as
`d2install: Project Diablo 2 data from <C:\Diablo2\ProjectD2> (use_pd2=auto, ...)`; with
`use_pd2 = no` it prints `vanilla Diablo II data` instead. `use_pd2 = yes` logs an error and
falls back to vanilla data if PD2 cannot be found. Explicit `d2char`/`d2data`/`d2exp`/`patch_d2`
paths and `mod_dir` still win over detection. To pin vanilla data for reproducible
runs, set `use_pd2 = no`.

## Troubleshooting

**The editor is slow (a few frames per second, or exit takes seconds).** A
bitmap that fell back to system memory draws a correct picture at a fraction
of the speed. Look for a `render: WARNING -- ... MEMORY bitmap` line in
`stderr.txt`, and run `ds1edit assets/act1_town.ini --selftest=60`: it prints
the frame-time summary and fails if any memory bitmap was drawn. To measure
across maps, drivers and window sizes, use `python scripts/perf/fps_test.py
--exe <path to ds1edit>` and read its table as described under "Frame-rate
test" in [BUILDING.md](BUILDING.md); `render avg` above 16.7 ms at 1080p
cannot hold 60 fps.

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
display: renderer "NVIDIA GeForce RTX 5060 Ti/PCIe/SSE2", vendor "NVIDIA Corporation"
```

**The editor draws correctly but at well under one frame per second.** The map
is right, the mouse works, nothing is in the logs except a very small number in
the `[perf]` block.

That is almost always the GPU not being used at all, and the `renderer` line
above says so. Two cases:

- **`renderer "GDI Generic"`** (or `llvmpipe` / `swrast` on Linux) is a software
  rasterizer, not a GPU. The editor now warns about this by name. It means no
  vendor driver is installed, or the OpenGL context could not reach the card --
  a remote desktop session is the usual reason. Install the graphics vendor's
  driver and run on the machine's own console.
- **The renderer names an integrated chip** (`Intel(R) UHD Graphics`, say) on a
  laptop that also has a discrete card. The executable asks for the discrete GPU
  on both NVIDIA and AMD, but a per-application setting can override that: check
  NVIDIA Control Panel → Manage 3D Settings → Program Settings (or the Windows
  Graphics settings page) and set `ds1edit.exe` to the high-performance GPU.

The `[perf]` block splits `render` from `present`, which narrows it further: time
in `render` is compositing the map, time in `present` is `al_flip_display`.

**Direct3D at well under one frame per second on a machine with a working GPU**
was a bug in versions before this fix: the built-in font's glyph bitmaps were
created before the display and stayed in system memory, so every text draw went
through a slow path. They are now converted to video bitmaps after the display
is created, and `ds1edit --selftest` prints the average render time and fails
above 250 ms per frame. If you still see it, report the `display:` and
`renderer` lines.

**Known issue: exiting in fullscreen on Direct3D can hang** while the display is
torn down (reproduced on Windows). Use `full_screen = NO`, or end the process
from Task Manager. This is not fixed yet.

## Project Structure

See [docs/structure.md](docs/structure.md) for detailed layout.

Key directories:
- `src/` - C source code organized into `core/`, `render/`, `editor/`, `ui/`, `mpq/`
- `data/`, `assets/` - Runtime data (copied to `bin/` at build time); `assets/ui/` holds the editor's interface images
- `test/` - Unit tests (Unity framework), Python suites in `test/python/`
- `scripts/` - Golden test and equivalence/qualification scripts (Python)
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
4. Run `ctest --preset default` (or your platform's preset) to verify
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

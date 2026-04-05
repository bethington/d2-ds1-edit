# DS1Edit - Project Structure

## Directory Layout

```
d2-ds1-edit/
├── CMakeLists.txt              # Main build configuration
├── CMakePresets.json           # Build presets (default, ci, dev, release)
├── vcpkg.json                  # Dependency manifest (Allegro 5)
├── LICENSE                     # MIT License
├── README.md                   # Project overview
├── BUILDING.md                 # Build instructions
├── PROJECT_STRUCTURE.md        # This file
├── Ds1edit.ini.sample          # Sample configuration (copy to bin/)
│
├── src/                        # C source and header files
│   ├── main.c                 # Entry point, initialization, display setup
│   ├── globals.c              # Allegro 5 global state (display, timers, etc.)
│   ├── config.c/h             # INI creation and reading
│   ├── error.c/h              # Error handling
│   ├── misc.c/h               # Utility functions, screen presentation
│   ├── structs.h              # Core data structures (COF_S, LAY_INF_S, etc.)
│   ├── types.h                # Basic type definitions
│   │
│   ├── core/                  # File format parsers and data management
│   │   ├── ds1.c/h           # DS1 map loading and saving
│   │   ├── dt1.c/h           # DT1 tile loading, caching, palette rebuild
│   │   ├── dt1_draw.c/h      # DT1 sub-tile drawing routines
│   │   ├── dt1_decode.c       # DT1 sub-tile pixel decoder
│   │   ├── cof.c/h           # COF/DCC/DC6 animation loading
│   │   ├── dcc.c/h           # DCC sprite format
│   │   ├── dc6.c/h           # DC6 sprite format
│   │   ├── palette.c/h       # Palette management and color matching
│   │   ├── rgba_cache.c/h    # Palette-indexed tile cache (indices + RGBA)
│   │   ├── animdata.c/h      # Animation data tables
│   │   └── txtread.c/h       # Text/config file parsing
│   │
│   ├── render/                # Rendering pipeline
│   │   ├── preview.c/h       # Tile rendering pipeline (7 render passes)
│   │   └── gfx.c/h           # Custom graphics helpers
│   │
│   ├── editor/                # Editing operations
│   │   ├── tiles.c/h         # Tile editing
│   │   ├── objects.c/h       # Object editing
│   │   ├── paths.c/h         # NPC path editing
│   │   └── undo.c/h          # Undo system
│   │
│   ├── ui/                    # User interface
│   │   ├── interface.c/h     # Main event loop, input handling
│   │   ├── dialogs.c/h       # Message dialogs (quit, save)
│   │   ├── edit_window.c/h   # Edit window
│   │   ├── bits_window.c/h   # Bits/flags window
│   │   └── compat.h          # Allegro 5 compatibility layer and draw helpers
│   │
│   └── mpq/                   # MPQ archive reader
│       ├── MpqView.c/h
│       ├── Explode.c
│       ├── Dcl_tbl.c
│       └── Wav_unp.c/h
│
├── data/                       # Runtime data (copied to bin/ at build time)
│   ├── pal*.bin               # Palettes
│   ├── cmap*.bin              # Color maps
│   ├── gamma.dat              # Gamma correction tables
│   ├── obj.txt                # Object definitions
│   └── ds1edit.dt1            # Editor tile set
│
├── pcx/                        # UI graphics (copied to bin/ at build time)
│   ├── *.png                  # Cursors, buttons, tabs, icons
│   ├── preview/               # Preview direction arrows
│   └── tiles/                 # Tile selection arrows
│
├── assets/                     # Game configuration (copied to bin/ at build time)
│   ├── *.ini                  # Area configs (act1_town.ini, etc.)
│   ├── tiles/                 # DS1 map files organized by Act
│   ├── excel/                 # Game data tables
│   └── palette/               # Palette data
│
├── examples/                   # Example launch scripts
│
├── test/                       # Unit tests (Unity framework)
│   ├── CMakeLists.txt         # Test build configuration
│   ├── test_placeholder.c     # Infrastructure validation
│   ├── test_palette.c         # Palette color conversion tests
│   ├── test_rgba_cache.c      # RGBA cache create/rebuild/convert tests
│   ├── test_dt1_decode.c      # DT1 sub-tile decoder tests
│   └── unity/                 # Unity test framework
│
├── scripts/                    # Development and test scripts
│   ├── run_golden_tests.py    # Golden screenshot comparison tests
│   ├── capture_golden.py      # Generate golden reference images
│   ├── compare_golden.py      # Compare two screenshots
│   └── convert_golden_to_png.py
│
├── bin/                        # Build output (gitignored)
│   ├── ds1edit.exe            # Built executable
│   └── *.dll                  # Allegro 5 runtime DLLs
│
├── docs/                       # Documentation
│
├── .vscode/                    # VSCode configuration
│   ├── launch.json            # Debug + run launch configs
│   └── tasks.json             # Build tasks
│
└── .github/
    └── workflows/
        └── build.yml          # CI: build + test on push/PR
```

## Build System

CMake with vcpkg for dependency management. Build presets:

```bash
cmake --preset default          # Configure (first time)
cmake --build --preset dev      # Development build (optimized + debug symbols)
cmake --build --preset release  # Release build (full optimization + LTCG)
ctest --preset default          # Run unit tests
cmake --build --preset release --target package   # Create release zip
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_SOFTWARE_RENDERER` | OFF | Force software rendering (no GPU) |
| `DS1EDIT_PERF_LOG` | OFF | Enable per-frame perf logging to stderr and perf_log.csv |

## Testing

**Unit tests** (C, Unity framework):
```bash
ctest --preset default          # Runs 4 tests in ~0.1s
```

**Golden screenshot tests** (Python, requires Pillow):
```bash
python scripts/run_golden_tests.py --core    # Core maps only
python scripts/run_golden_tests.py --full    # All maps
```

## Rendering Architecture

The render pipeline in `src/render/preview.c` runs 7 passes per frame:

1. **Base terrain** -- Lower walls, floors, tile shadows (batched GPU draws)
2. **Object shadows** -- Tinted black silhouettes at D2 darkness levels
3. **Objects behind walls** -- Animated sprites (orderflag=1)
4. **Upper walls + objects** -- Wall tiles + sprites in front (orderflag 0/2)
5. **Roofs** -- Roof tiles
6. **Special tiles** -- Orientation 10/11 tiles (optional)
7. **Walkable info** -- Debug overlay (optional)

All rendering targets a pre-set VIDEO bitmap (`screen_buff`). Draw helpers in
`src/ui/compat.h` skip target switching when it's already correct. The 25 Hz tick
timer drives animation; the display refreshes at vsync rate (~60-165 Hz).

## Include Convention

All `#include` directives use project-root style relative to `src/`:

```c
#include "core/dt1.h"
#include "render/preview.h"
#include "editor/objects.h"
#include "ui/compat.h"
#include "structs.h"      // files at src/ root
```

## Key Files

| File | Purpose |
|------|---------|
| `src/render/preview.c` | Tile rendering pipeline, per-frame perf stats |
| `src/ui/compat.h` | Allegro 5 draw helpers, D2 blend modes |
| `src/main.c` | Init, display creation, bitmap promotion |
| `src/core/cof.c` | COF/DCC/DC6 animation loading |
| `src/core/dt1.c` | DT1 tile loading and palette rebuild |
| `src/core/rgba_cache.c` | Tile bitmap cache (palette -> RGBA) |
| `src/structs.h` | Core structs: COF_S, LAY_INF_S, DS1_S |
| `CMakePresets.json` | Build presets for dev/release/CI |
| `vcpkg.json` | Allegro 5 dependency declaration |

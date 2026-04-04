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
│
├── Sources/                    # C source and header files
│   ├── main.c                 # Entry point, initialization, display setup
│   ├── interfac.c             # Main event loop, input handling
│   ├── wPreview.c             # Tile rendering pipeline (7 render passes)
│   ├── a5_compat.h            # Allegro 5 compatibility layer and draw helpers
│   ├── a5_globals.c           # Allegro 5 global state (display, timers, etc.)
│   ├── anim.c                 # COF/DCC/DC6 animation loading
│   ├── dt1misc.c              # DT1 tile loading, caching, palette rebuild
│   ├── dt1_decode.c           # DT1 sub-tile pixel decoder
│   ├── ds1misc.c              # DS1 map file loading
│   ├── ds1save.c              # DS1 map file saving
│   ├── palette.c              # Palette management and color matching
│   ├── rgba_cache.c           # Palette-indexed tile cache (indices + RGBA)
│   ├── structs.h              # Core data structures (COF_S, LAY_INF_S, etc.)
│   ├── gfx_custom.c           # Legacy graphics stubs (mostly removed)
│   ├── editobj.c              # Object editing
│   ├── editpath.c             # NPC path editing
│   ├── edittile.c             # Tile editing
│   ├── misc.c                 # Utility functions, screen presentation
│   └── mpq/                   # MPQ archive reader
│       ├── MpqView.c
│       ├── Explode.c
│       ├── Dcl_tbl.c
│       └── Wav_unp.c
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
├── bin/                        # Runtime directory (exe, DLLs, assets)
│   ├── ds1edit.exe            # Built executable (all configurations)
│   ├── Ds1edit.ini.sample     # Sample configuration (copy to Ds1edit.ini)
│   ├── *.dll                  # Allegro 5 runtime DLLs (copied by CMake)
│   ├── data/                  # Palettes, gamma tables, version
│   └── assets/                # Map configurations and test fixtures
│       ├── *.ini              # Area configs (act1_town.ini, etc.)
│       ├── tiles/             # DS1 map files organized by Act
│       ├── excel/             # Game data tables
│       └── palette/           # Palette data
│
├── docs/                       # Documentation
│   ├── README.md              # Detailed documentation index
│   └── guides/                # Technical guides
│
├── .vscode/                    # VSCode configuration
│   ├── launch.json            # 31 debug + 1 run launch configs (all Acts)
│   └── tasks.json             # Build tasks (dev, release, test, package)
│
└── .github/
    └── workflows/
        └── build.yml          # CI: build + test on push/PR
```

## Build System

CMake with vcpkg for dependency management. Three build presets:

```bash
cmake --preset default          # Configure (first time)
cmake --build --preset dev      # Development build (optimized + debug symbols)
cmake --build --preset release  # Release build (full optimization + LTCG)
ctest --preset default          # Run unit tests
cmake --build --preset release --target package   # Create release zip
```

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

The render pipeline in `wPreview.c` runs 7 passes per frame:

1. **Base terrain** — Lower walls, floors, tile shadows (batched GPU draws)
2. **Object shadows** — Tinted black silhouettes at D2 darkness levels
3. **Objects behind walls** — Animated sprites (orderflag=1)
4. **Upper walls + objects** — Wall tiles + sprites in front (orderflag 0/2)
5. **Roofs** — Roof tiles
6. **Special tiles** — Orientation 10/11 tiles (optional)
7. **Walkable info** — Debug overlay (optional)

All rendering targets a pre-set VIDEO bitmap (`screen_buff`). Draw helpers in
`a5_compat.h` skip target switching when it's already correct. The 25 Hz tick
timer drives animation; the display refreshes at vsync rate (~60-165 Hz).

## Key Files

| File | Purpose |
|------|---------|
| `Sources/wPreview.c` | Tile rendering pipeline, per-frame perf stats |
| `Sources/a5_compat.h` | Allegro 5 draw helpers, D2 blend modes |
| `Sources/main.c` | Init, display creation, bitmap promotion |
| `Sources/anim.c` | COF/DCC/DC6 animation loading |
| `Sources/dt1misc.c` | DT1 tile loading and palette rebuild |
| `Sources/rgba_cache.c` | Tile bitmap cache (palette → RGBA) |
| `Sources/structs.h` | Core structs: COF_S, LAY_INF_S, DS1_S |
| `CMakePresets.json` | Build presets for dev/release/CI |
| `vcpkg.json` | Allegro 5 dependency declaration |

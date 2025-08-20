# DS1Edit - Project Organization

## Directory Structure

```
DS1Edit/
├── .gitignore              # Git ignore rules
├── .vscode/                # VS Code configuration
├── assets/                 # Game assets and DS1 configuration files
│   ├── excel/             # Excel data files (LvlPrest.txt, etc.)
│   ├── tiles/             # DS1 tile files organized by Act
│   └── *.ini              # DS1 configuration files by area
├── bin/                    # Compiled executables
│   ├── win_ds1edit.exe    # Release build
│   └── win_ds1edit_debug.exe  # Debug build  
├── build/                  # Build artifacts and intermediate files
├── config/                 # Configuration files
│   ├── Ds1edit.ini        # Main DS1Edit configuration
│   └── *.ini              # Other configuration files
├── Data/                   # Game data files (palettes, etc.)
├── docs/                   # Project documentation
│   ├── BUILD_*.md         # Build instructions
│   ├── PROJECT_*.md       # Project status docs
│   └── README.md          # This file
├── examples/               # Example DS1 files and configurations
├── logs/                   # Log files and debug output
│   ├── stderr.txt         # Error logs
│   └── debug_*.txt        # Debug output files
├── media/                  # Media files
│   ├── screenshots/       # Screenshot files (.bmp, .pcx)
│   └── pcx/              # PCX image resources
├── scripts/                # Development scripts
│   ├── batch/             # Batch scripts for building and testing
│   │   ├── build.bat      # Main build script
│   │   ├── clean.bat      # Cleanup script
│   │   └── test_*_lines.bat  # Testing scripts
│   └── python/            # Python utilities
│       ├── generate_level_inis.py  # INI file generation
│       ├── test_ini.py    # INI testing utilities
│       └── rename_tileset_files.py  # File management
├── Sources/                # C/C++ source code
│   ├── *.c                # Source files
│   ├── *.h                # Header files
│   └── Makefile           # Build configuration
├── temp/                   # Temporary files (auto-cleaned)
├── third_party/            # Third-party libraries
│   └── allegro/           # Allegro graphics library
└── tools/                  # Development tools and utilities
```

## Quick Start

### Building the Project
```bash
# Navigate to project root
cd DS1Edit

# Run build script
scripts\batch\build.bat debug
```

### Testing DS1 Files
```bash
# Test individual area configurations
scripts\batch\test_clearing_lines.bat
scripts\batch\test_mesa_lines.bat
# ... etc for other areas
```

### Generating INI Files
```bash
# Generate level configuration files
python scripts\python\generate_level_inis.py
```

## File Organization

### Assets Directory
- **`assets/tiles/`** - DS1 files organized by Act (ACT1/, ACT2/, ACT3/, ACT4/, Expansion/)
- **`assets/excel/`** - Game data files (LvlPrest.txt with level definitions)
- **`assets/*.ini`** - Area-specific DS1 configuration files (act1_*, act2_*, etc.)

### Configuration Files
- **Main Config**: `config/Ds1edit.ini` - Primary DS1Edit settings
- **Area Configs**: `assets/*.ini` - DS1 file lists for each game area

### Scripts Organization
- **Batch Scripts**: `scripts/batch/` - Windows batch files for building and testing
- **Python Scripts**: `scripts/python/` - Python utilities for file generation and management

### Build System
- **Source**: `Sources/` - All C/C++ source and header files
- **Build Output**: `bin/` - Final executables
- **Build Artifacts**: `build/` - Intermediate build files

## Development Workflow

1. **Edit Source Code**: Modify files in `Sources/`
2. **Build Project**: Run `scripts\batch\build.bat debug`
3. **Test Changes**: Use `scripts\batch\test_*_lines.bat` scripts
4. **Generate Configs**: Run `scripts\python\generate_level_inis.py` if needed
5. **Clean Build**: Use `scripts\batch\clean.bat` when needed

## Key Features

- **Systematic DS1 Testing**: Individual file validation with error reporting
- **Automated INI Generation**: Python scripts for creating area configuration files
- **Comprehensive Build System**: Debug and release build configurations
- **Asset Organization**: Logical grouping of game assets by Act and area type
- **Development Tools**: Scripts for testing, building, and file management

## Notes

- Debug builds include additional logging and error checking
- Test scripts generate `*_working.ini` and `*_errors.txt` files for validation
- Screenshots and temporary files are automatically organized
- Configuration files are centralized for easy management

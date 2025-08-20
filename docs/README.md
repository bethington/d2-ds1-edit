# DS1Edit Documentation

DS1Edit is a powerful level editor for Diablo II, allowing users to create and modify game maps (.ds1 files) with comprehensive tileset support and systematic validation tools.

## 📚 Documentation Overview

### 🚀 Getting Started
- **[Main README](../README.md)** - Project overview and quick start
- **[Installation Guide](BUILD_INSTRUCTIONS.md)** - Complete setup instructions
- **[Project Structure](../PROJECT_STRUCTURE.md)** - Directory organization

### 📖 User Guides
- **[Getting Started Tutorial](01-Getting-Started/)** - First steps with DS1Edit
- **[Basic Tutorials](02-Tutorials/)** - Step-by-step guides
- **[Advanced Guides](03-Advanced-Guides/)** - In-depth topics
- **[Examples](04-Examples/)** - Sample projects and maps

### 🔧 Technical Documentation
- **[API Reference](API_REFERENCE.md)** - Complete command-line and configuration reference
- **[Development Guide](DEVELOPMENT.md)** - Contributing and extending DS1Edit
- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions
- **[DS1 Format Docs](d2_maze_ds1_docs.md)** - File format specifications

### 🛠️ Build and Setup
- **[Build Instructions](BUILD_INSTRUCTIONS.md)** - How to compile DS1Edit
- **[Allegro Setup](ALLEGRO_SETUP.md)** - Graphics library configuration
- **[VS Code Setup](VS_CODE_SETUP_COMPLETE.md)** - IDE configuration

## 🎯 Key Features

### Core Functionality
- **DS1 File Editing**: Create and modify Diablo II level files
- **Multi-Act Support**: Full support for Acts 1-5 including Expansion content
- **Tileset Management**: Comprehensive tileset validation and organization
- **Visual Editor**: Graphical interface for map editing

### Advanced Tools
- **Systematic Testing**: Individual DS1 file validation with error reporting
- **Automated INI Generation**: Python scripts for configuration file creation
- **Batch Processing**: Scripts for bulk operations and testing
- **Debug Mode**: Enhanced logging and error checking capabilities

### Supported Content
- **Act 1**: Cave, Catacombs, Monastery areas
- **Act 2**: Desert, Tomb, Maggot Lair areas
- **Act 3**: Jungle, Kurast, Spider, Temple, Mephisto areas
- **Act 4**: Mesa, Lava, Fortress, Diablo areas
- **Act 5**: Siege, Barricade, Ice Cave areas

## 📊 Validation Results

| Area | Tileset | Files | Working | Success Rate |
|------|---------|-------|---------|--------------|
| Act 2 Maggot | 18 | 40 | 2 | 5.0% |
| Act 3 Clearing | 21 | 90 | 90 | 100.0% |
| Act 3 Kurast | 20 | 85 | 85 | 100.0% |
| Act 3 Mephisto | 22 | 55 | 55 | 100.0% |
| Act 4 Mesa | 27 | 145 | 42 | 29.0% |
| Act 4 Lava | 28 | 45 | 43 | 95.6% |
| Act 5 Siege | 30 | 55 | 15 | 27.3% |

## 🔍 Quick Reference

### Command Line Usage
```bash
# Load and edit a DS1 file
win_ds1edit_debug.exe mapfile.ds1 tileset_id def_value

# Example - Act 3 clearing
win_ds1edit_debug.exe assets/tiles/ACT3/Jungle/clearing1.ds1 21 575
```

### Tileset IDs
- **2** - Act 1 Cave
- **21** - Act 3 Jungle/Clearing  
- **27** - Act 4 Mesa
- **28** - Act 4 Lava
- **30** - Act 5 Siege

### Test Scripts
```bash
# Test specific areas
scripts\batch\test_clearing_lines.bat
scripts\batch\test_mesa_lines.bat
scripts\batch\test_lava_lines.bat
```

## 🏗️ Project Architecture

### Directory Structure
```
DS1Edit/
├── assets/              # Game assets and configuration files
├── bin/                 # Compiled executables
├── docs/                # This documentation
├── scripts/             # Development and utility scripts
├── Sources/             # C/C++ source code
└── third_party/         # External libraries (Allegro)
```

### Build System
- **Debug Build**: `build.bat debug` - Full debugging support
- **Release Build**: `build.bat release` - Optimized performance
- **Clean Build**: `scripts\batch\clean.bat` - Remove artifacts

### Testing Framework
- **Individual file validation** with exit code analysis
- **Batch processing** for systematic area testing
- **Automated reporting** of success/failure rates
- **Error categorization** by exit codes

## 🤝 Contributing

1. **Read the [Development Guide](DEVELOPMENT.md)**
2. **Check [Troubleshooting](TROUBLESHOOTING.md)** for common issues
3. **Review [API Reference](API_REFERENCE.md)** for technical details
4. **Follow project conventions** outlined in documentation

## 📄 License

This project is open source. Please refer to the LICENSE file for details.

## 🙏 Acknowledgments

- Built using **Allegro 4.4.3.1** game development library
- Based on **Diablo II file format specifications**
- Community contributions for **tileset validation and testing**

---

**Last Updated**: August 2025  
**Documentation Version**: 2.0  
**DS1Edit Version**: Latest Development Build
  - Built from source in `third_party/allegro-4.4.3.1/`
  - Runtime library: `bin/allegro-4.4.2-monolith-md.dll`
  - Link library: `bin/allegro-4.4.2-monolith-md.lib`

## Usage

Run the executable from the `bin/` directory:

```cmd
cd bin
win_ds1edit.exe              # Release version
win_ds1edit_debug.exe        # Debug version (with console output)
```

The editor will load with the default configuration. Use the various `.bat` files in the `examples/` directory to load different map examples.

## Development

### VS Code Integration

The project includes full VS Code integration with:
- IntelliSense for C code completion
- Build tasks for debug and release builds
- Debugging configuration
- Problem matcher for compiler errors

### Build Configuration

- **Debug**: Optimized for debugging with symbols (`/Zi /Od /MDd`)
- **Release**: Optimized for size and performance (`/O2 /MD`)
- **Console**: `USE_CONSOLE` define enables debug output in debug builds

## File Formats

- `.ds1`: Diablo II map files (main editing format)
- `.dt1`: Tile graphics data
- `.pcx`: Image files for UI and previews
- `.ini`: Configuration files for various settings

## License

This appears to be a community tool for Diablo II modding. Please respect Blizzard Entertainment's intellectual property rights when using this software.

## Troubleshooting

### Common Build Issues

1. **"Cannot find allegro.h"**: Ensure Allegro libraries are built in `third_party/`
2. **"dinput.lib not found"**: Update Windows SDK, uses dinput8.lib for modern systems
3. **"vcvars32.bat not found"**: Install Visual Studio 2019 Community or update compiler path

### Runtime Issues

1. **Missing DLL**: Ensure `allegro-4.4.2-monolith-md.dll` is in the same directory as the executable
2. **Access violations**: Run from the correct working directory with required data files

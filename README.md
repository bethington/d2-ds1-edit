# DS1Edit - Diablo II Level Editor

A powerful level editor for Diablo II that allows creation and modification of game maps (.ds1 files) with comprehensive tileset support and systematic validation tools.

## 🚀 Quick Start

```bash
# Clone or download the project
cd DS1Edit

# Build the project
build.bat debug

# Run DS1Edit
bin\win_ds1edit_debug.exe
```

## 📋 Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Development](#development)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## ✨ Features

### Core Functionality
- **DS1 File Editing**: Create and modify Diablo II level files
- **Multi-Act Support**: Full support for Acts 1-5 including Expansion content
- **Tileset Management**: Comprehensive tileset validation and organization
- **Visual Editor**: Graphical interface for map editing
- **Asset Management**: Organized handling of game assets and resources

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

## 🔧 Installation

### Prerequisites
- **Operating System**: Windows 10/11
- **Compiler**: Microsoft Visual Studio 2019+ or MSVC Build Tools
- **Dependencies**: Allegro 4.4.3.1 (included)
- **Optional**: Python 3.x for utility scripts

### Build Instructions

1. **Clone the Repository**
   ```bash
   git clone [repository-url]
   cd DS1Edit
   ```

2. **Build the Project**
   ```bash
   # Debug build (recommended for development)
   build.bat debug
   
   # Release build (optimized)
   build.bat release
   ```

3. **Verify Installation**
   ```bash
   # Check if build was successful
   bin\win_ds1edit_debug.exe --version
   ```

## 📖 Usage

### Basic Operations

1. **Launch DS1Edit**
   ```bash
   bin\win_ds1edit_debug.exe [mapfile.ds1] [tileset] [def_value]
   ```

2. **Load a Map**
   - Use File → Open to load existing .ds1 files
   - Navigate to `assets/tiles/` for example maps

3. **Edit Maps**
   - Use the tile palette to place tiles
   - Modify layers (floor, wall, shadow, etc.)
   - Save changes with File → Save

### Testing and Validation

1. **Test Individual Areas**
   ```bash
   # Test Act 3 clearing areas
   scripts\batch\test_clearing_lines.bat
   
   # Test Act 4 mesa areas
   scripts\batch\test_mesa_lines.bat
   ```

2. **Generate Configuration Files**
   ```bash
   python scripts\python\generate_level_inis.py
   ```

### Configuration Management

- **Main Config**: `config/Ds1edit.ini` - Primary DS1Edit settings
- **Area Configs**: `assets/*.ini` - DS1 file lists for each game area
- **Tilesets**: Organized by Act in `assets/tiles/`

## 📁 Project Structure

```
DS1Edit/
├── 📁 assets/              # Game assets and DS1 configuration files
│   ├── excel/             # Game data (LvlPrest.txt)
│   ├── tiles/             # DS1 files organized by Act
│   └── *.ini              # Area configuration files
├── 📁 bin/                 # Compiled executables
├── 📁 build/               # Build artifacts
├── 📁 config/              # Configuration files
├── 📁 docs/                # Comprehensive documentation
├── 📁 logs/                # Debug output and error logs
├── 📁 media/               # Screenshots and graphics
├── 📁 scripts/             # Development and utility scripts
├── 📁 Sources/             # C/C++ source code
└── 📁 third_party/         # External libraries (Allegro)
```

## 🛠️ Development

### Building from Source

1. **Setup Development Environment**
   - Install Visual Studio 2019+ with C++ support
   - Ensure Windows SDK is available
   - Python 3.x for utility scripts (optional)

2. **Build Configuration**
   ```bash
   # Debug build with full logging
   build.bat debug
   
   # Release build optimized
   build.bat release
   
   # Clean all build artifacts  
   scripts\batch\clean.bat
   ```

3. **Testing Framework**
   ```bash
   # Test all areas systematically
   scripts\batch\test_*_lines.bat
   
   # Generate test reports
   python scripts\python\test_ini.py
   ```

### Code Organization

- **Core Editor**: `Sources/` - Main DS1Edit application code
- **MPQ Support**: `Sources/mpq/` - Archive file handling
- **Build System**: `scripts/batch/` - Build and test scripts
- **Utilities**: `scripts/python/` - File generation and management

### Development Workflow

1. **Make Changes**: Edit source files in `Sources/`
2. **Build**: Run `build.bat debug` 
3. **Test**: Use test scripts to validate changes
4. **Debug**: Check `logs/` for error output
5. **Commit**: Use organized git structure

## 📚 Documentation

### Available Documentation
- **📖 [Getting Started](docs/01-Getting-Started/)** - Installation and first steps
- **🎓 [Tutorials](docs/02-Tutorials/)** - Step-by-step guides
- **🔧 [Advanced Guides](docs/03-Advanced-Guides/)** - In-depth topics
- **💡 [Examples](docs/04-Examples/)** - Sample projects and maps
- **📋 [API Reference](docs/05-Reference/)** - Technical documentation

### Quick Links
- [Build Instructions](docs/BUILD_INSTRUCTIONS.md)
- [Allegro Setup](docs/ALLEGRO_SETUP.md) 
- [Project Structure](PROJECT_STRUCTURE.md)
- [DS1 Format Documentation](docs/d2_maze_ds1_docs.md)

## 🎯 Key Features in Detail

### Systematic DS1 Validation
- Individual file testing with error classification
- Exit code analysis (0=success, 1/3=various errors)
- Automated generation of working file lists
- Comprehensive error reporting

### Multi-Act Tileset Support  
- **Act 1**: Tileset 2 (Cave), 4 (Catacombs), 5 (Monastery)
- **Act 2**: Tileset 18 (Maggot), 10 (Desert), 12 (Tomb)
- **Act 3**: Tileset 21 (Jungle), 20 (Town), 22 (Temple), 23 (Spider)
- **Act 4**: Tileset 27 (Mesa), 28 (Lava), 29 (Diablo)
- **Act 5**: Tileset 30 (Siege), 31 (Barricade), 35 (Ice)

### Area Coverage Statistics
- **Act 2 Maggot**: 2/40 files compatible (act number issues)
- **Act 3 Clearing**: 90/90 files working (100% success)
- **Act 3 Kurast**: 85 files covering urban areas
- **Act 3 Mephisto**: 55 files for boss encounters
- **Act 4 Mesa**: 42/145 files working (core areas functional)
- **Act 4 Lava**: 43/45 files working (95.6% success)
- **Act 5 Siege**: 15/55 files working (core strips functional)

## 🤝 Contributing

1. **Fork the Repository**
2. **Create Feature Branch**: `git checkout -b feature/new-feature`
3. **Make Changes**: Follow existing code style
4. **Test Changes**: Run test scripts to validate
5. **Update Documentation**: Add/update relevant docs
6. **Submit Pull Request**: Include description of changes

### Development Guidelines
- Use descriptive commit messages
- Test all changes with validation scripts
- Update documentation for new features
- Follow existing project organization

## 📄 License

This project is open source. Please refer to the LICENSE file for details.

## 🙏 Acknowledgments

- Built using Allegro 4.4.3.1 game development library
- Based on Diablo II file format specifications
- Community contributions for tileset validation and testing

---

**DS1Edit** - Professional Diablo II Level Editor with Comprehensive Validation and Multi-Act Support

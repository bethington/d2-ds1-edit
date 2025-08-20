# DS1Edit Development Guide

## Getting Started with Development

### Prerequisites
- **Visual Studio 2019+** with C++ Desktop Development workload
- **Windows SDK 10.0.19041.0** or later
- **Git** for version control
- **Python 3.6+** (optional, for utility scripts)

### Setting Up Development Environment

1. **Clone the Repository**
   ```bash
   git clone [repository-url]
   cd DS1Edit
   ```

2. **Initialize Submodules** (if any)
   ```bash
   git submodule update --init --recursive
   ```

3. **Build Dependencies**
   ```bash
   scripts\batch\build_allegro.bat
   ```

4. **Test Build**
   ```bash
   build.bat debug
   ```

## Code Architecture

### Project Structure
```
Sources/
├── main.c              # Application entry point
├── ds1edit.c           # Core DS1 editing functions
├── ds1file.c           # DS1 file format handling
├── graphics.c          # Allegro graphics interface
├── interface.c         # User interface logic
├── palette.c           # Color palette management
├── tiles.c             # Tile rendering and management
└── mpq/                # MPQ archive support
    ├── mpq.c          # MPQ file handling
    ├── compress.c     # Compression algorithms
    └── crypt.c        # Encryption/decryption
```

### Key Components

#### DS1 File Handler (ds1file.c)
- **Load DS1 files**: Parse binary DS1 format
- **Validate structure**: Check file integrity
- **Extract layers**: Floor, wall, shadow, special layers
- **Save modifications**: Write back to DS1 format

#### Graphics Engine (graphics.c) 
- **Allegro integration**: Initialize display, input, sound
- **Tile rendering**: Draw tiles from DT1 files
- **Layer composition**: Combine multiple tile layers
- **View management**: Scrolling, zooming, selection

#### User Interface (interface.c)
- **Menu system**: File operations, tools, options
- **Tool palette**: Tile selection, editing tools
- **Status display**: Current tile, coordinates, layer info
- **Dialog boxes**: File dialogs, configuration

#### Palette System (palette.c)
- **Color management**: Load game palettes
- **Palette switching**: Different acts use different palettes
- **Color conversion**: Convert between formats

## Building and Testing

### Debug Build
```bash
# Full debug build with symbols
build.bat debug

# Run with debugging
bin\win_ds1edit_debug.exe [args]
```

### Release Build  
```bash
# Optimized release build
build.bat release

# Test release version
bin\win_ds1edit.exe [args]
```

### Clean Build
```bash
# Remove all build artifacts
scripts\batch\clean.bat

# Rebuild from scratch
build.bat debug
```

### Automated Testing
```bash
# Test all areas systematically
for %f in (scripts\batch\test_*_lines.bat) do call "%f"

# Generate test reports
python scripts\python\test_ini.py --all --report
```

## Code Style Guidelines

### Naming Conventions
- **Functions**: `snake_case` (e.g., `load_ds1_file`)
- **Variables**: `snake_case` (e.g., `tile_count`)
- **Constants**: `UPPER_CASE` (e.g., `MAX_TILES`)
- **Types**: `PascalCase` (e.g., `TileLayer`)

### File Organization
```c
// Header comment with file purpose
/*
 * ds1file.c - DS1 file format handling
 * Handles loading, parsing, and saving DS1 map files
 */

#include <stdio.h>          // System includes
#include <stdlib.h>
#include "ds1edit.h"        // Local includes
#include "graphics.h"

// Constants and macros
#define MAX_LAYERS 4
#define DS1_SIGNATURE 0x01

// Type definitions
typedef struct {
    int width;
    int height;
    int act;
} DS1Header;

// Function implementations
int load_ds1_file(const char* filename) {
    // Implementation
}
```

### Error Handling
```c
// Use consistent error codes
#define ERROR_SUCCESS       0
#define ERROR_FILE_NOT_FOUND 1
#define ERROR_INVALID_FORMAT 2
#define ERROR_MEMORY_ALLOC   3

// Always check return values
FILE* file = fopen(filename, "rb");
if (!file) {
    fprintf(stderr, "Error: Could not open file %s\n", filename);
    return ERROR_FILE_NOT_FOUND;
}

// Clean up resources
if (buffer) {
    free(buffer);
    buffer = NULL;
}
fclose(file);
```

## Adding New Features

### Adding New Tileset Support

1. **Update tileset definitions**
   ```c
   // In tiles.c
   typedef enum {
       TILESET_ACT1_CAVE = 2,
       TILESET_ACT1_CATACOMBS = 4,
       TILESET_NEW_AREA = 32,  // Add new tileset ID
   } TilesetID;
   ```

2. **Add tileset loading logic**
   ```c
   int load_tileset(int tileset_id) {
       switch (tileset_id) {
           case TILESET_NEW_AREA:
               return load_new_area_tiles();
           // ... existing cases
       }
   }
   ```

3. **Update configuration files**
   ```ini
   # Create assets/new_area.ini
   32 1000 assets/tiles/NewAct/Area/file1.ds1
   32 1001 assets/tiles/NewAct/Area/file2.ds1
   ```

4. **Add test script**
   ```bash
   # Create scripts/batch/test_new_area_lines.bat
   @echo off
   for /f "tokens=1,2,3*" %%a in (assets\new_area.ini) do (
       bin\win_ds1edit_debug.exe "%%c" %%a %%b
   )
   ```

### Adding New File Format Support

1. **Create format handler**
   ```c
   // In new file: dt1file.c
   int load_dt1_file(const char* filename) {
       // Implement DT1 loading
   }
   
   int save_dt1_file(const char* filename) {
       // Implement DT1 saving
   }
   ```

2. **Update main application**
   ```c
   // In main.c
   if (strstr(filename, ".dt1")) {
       return load_dt1_file(filename);
   } else if (strstr(filename, ".ds1")) {
       return load_ds1_file(filename);
   }
   ```

3. **Add to build system**
   ```batch
   REM In scripts/batch/build.bat
   cl /c Sources\dt1file.c /Fo:build\dt1file.obj
   ```

## Debugging Techniques

### Using Debug Builds
```c
// Add debug output
#ifdef _DEBUG
    printf("Debug: Loading file %s\n", filename);
    printf("Debug: Tileset ID = %d\n", tileset_id);
#endif

// Validate assumptions
assert(tile_count > 0);
assert(buffer != NULL);
```

### Memory Debugging
```c
// Track allocations in debug builds
#ifdef _DEBUG
static int allocation_count = 0;

void* debug_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) allocation_count++;
    printf("Allocated %zu bytes at %p (count: %d)\n", 
           size, ptr, allocation_count);
    return ptr;
}

void debug_free(void* ptr) {
    if (ptr) allocation_count--;
    printf("Freed %p (count: %d)\n", ptr, allocation_count);
    free(ptr);
}

#define malloc debug_malloc
#define free debug_free
#endif
```

### File I/O Debugging
```c
// Trace file operations
void trace_file_operation(const char* operation, const char* filename) {
    FILE* trace = fopen("logs/file_trace.log", "a");
    if (trace) {
        fprintf(trace, "[%s] %s: %s\n", 
                get_timestamp(), operation, filename);
        fclose(trace);
    }
}

// Usage
trace_file_operation("OPEN", filename);
FILE* file = fopen(filename, "rb");
if (!file) {
    trace_file_operation("OPEN_FAILED", filename);
    return ERROR_FILE_NOT_FOUND;
}
```

## Performance Optimization

### Profiling
```c
// Simple timing for critical sections
#include <time.h>

clock_t start = clock();
// ... expensive operation ...
clock_t end = clock();

double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
printf("Operation took %f seconds\n", cpu_time);
```

### Memory Optimization
```c
// Pool allocation for frequent small allocations
typedef struct {
    char data[1024];
    int used;
} MemoryPool;

MemoryPool pool = {0};

void* pool_alloc(size_t size) {
    if (pool.used + size > sizeof(pool.data)) {
        return NULL; // Pool full
    }
    void* ptr = pool.data + pool.used;
    pool.used += size;
    return ptr;
}

void pool_reset() {
    pool.used = 0;
}
```

### File I/O Optimization
```c
// Buffer file reads for better performance
#define BUFFER_SIZE 8192

int read_buffered(FILE* file, void* data, size_t size) {
    static char buffer[BUFFER_SIZE];
    static size_t buffer_pos = 0;
    static size_t buffer_size = 0;
    
    // Implementation of buffered reading
}
```

## Contributing Guidelines

### Before Submitting Changes

1. **Test thoroughly**
   ```bash
   # Run all tests
   build.bat debug
   for %f in (scripts\batch\test_*_lines.bat) do call "%f"
   ```

2. **Check code style**
   - Follow naming conventions
   - Add appropriate comments
   - Handle errors consistently

3. **Update documentation**
   - Add/update function comments
   - Update relevant .md files
   - Include usage examples

### Commit Message Format
```
feat: add support for Act 6 tilesets

- Add tileset ID 33 for new expansion areas
- Update configuration loading for extended areas  
- Add test script for Act 6 validation
- Update documentation with new tileset info

Fixes #123
```

### Pull Request Process

1. **Create feature branch**
   ```bash
   git checkout -b feature/act6-support
   ```

2. **Make changes with tests**
3. **Update documentation**
4. **Submit PR with**:
   - Clear description of changes
   - Test results
   - Documentation updates
   - Any breaking changes noted

## Advanced Topics

### MPQ Archive Integration
```c
// Working with MPQ files
#include "mpq/mpq.h"

int load_from_mpq(const char* mpq_file, const char* internal_path) {
    MPQArchive archive;
    if (mpq_open(&archive, mpq_file) != 0) {
        return ERROR_MPQ_OPEN;
    }
    
    void* data = mpq_extract_file(&archive, internal_path);
    if (!data) {
        mpq_close(&archive);
        return ERROR_FILE_NOT_FOUND;
    }
    
    // Process data
    
    mpq_free_file(data);
    mpq_close(&archive);
    return ERROR_SUCCESS;
}
```

### Custom Build Configurations
```batch
REM In scripts/batch/build_custom.bat
@echo off

REM Custom defines
set CUSTOM_DEFINES=/D ENABLE_EXTENDED_LOGGING /D MAX_TILES=2048

REM Custom optimization
set CUSTOM_OPTS=/O2 /GL

REM Build with custom settings
cl %CUSTOM_DEFINES% %CUSTOM_OPTS% Sources\*.c /Fe:bin\ds1edit_custom.exe
```

### Plugin Architecture (Future)
```c
// Plugin interface design
typedef struct {
    const char* name;
    const char* version;
    int (*init)(void);
    int (*load_file)(const char* filename);
    void (*cleanup)(void);
} Plugin;

// Plugin registration
int register_plugin(Plugin* plugin) {
    if (plugin && plugin->init() == 0) {
        // Add to plugin list
        return 0;
    }
    return -1;
}
```

This development guide provides the foundation for contributing to DS1Edit and extending its capabilities while maintaining code quality and project standards.

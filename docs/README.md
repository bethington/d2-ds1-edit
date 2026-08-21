# DS1Edit Documentation

DS1Edit is a level editor for Diablo II, allowing creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering. It continues Paul Siramy's `win_ds1edit` (2002-2011): the user guides below are his original documentation, and [NOTICE](../NOTICE) records the provenance of the code.

## Documentation Index

### Build and Setup
- **[Main README](../README.md)** - Project overview and quick start
- **[Building](../BUILDING.md)** - Build prerequisites and instructions
- **[Project Structure](../PROJECT_STRUCTURE.md)** - Directory layout and architecture

### User Guides (by Paul Siramy)
- **[Getting Started](getting-started/)** - First steps with DS1Edit
- **[Basic Tutorials](tutorials/)** - Step-by-step map editing guide
- **[Advanced Guides](guides/)** - Adding monsters, objects, and more

### Technical Reference
- **[API Reference](reference/API_REFERENCE.md)** - Command-line and configuration reference
- **[DS1/Maze Format](reference/d2_maze_ds1_docs.md)** - File format specifications and maze generation
- **[Troubleshooting](reference/TROUBLESHOOTING.md)** - Common issues and solutions
- **[COF Pipeline](guides/COF-Pipeline-1.13c.md)** - COF animation pipeline, reverse-engineered from the 1.13c DLLs

### Project Reports
- Reports in [internal/](internal/) document historical analysis
  and migration work (Allegro integration, data analysis, etc.)

## Quick Reference

### Command Line
```bash
cd bin

# Single DS1 file
ds1edit.exe <file.ds1> <LvlTypes.txt ID> <LvlPrest.txt DEF>

# Multiple DS1 files via INI
ds1edit.exe <file.ini>

# Headless screenshot
ds1edit.exe <file.ds1> <ID> <DEF> --headless output.png
```

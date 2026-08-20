# DS1Edit Documentation

DS1Edit is a level editor for Diablo II, allowing creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering. It continues Paul Siramy's `win_ds1edit` (2002-2011): the user guides below are his original documentation, and [NOTICE](../NOTICE) records the provenance of the code.

## Documentation Index

### Build and Setup
- **[Main README](../README.md)** - Project overview and quick start
- **[Building](../BUILDING.md)** - Build prerequisites and instructions
- **[Project Structure](../PROJECT_STRUCTURE.md)** - Directory layout and architecture

### User Guides (by Paul Siramy)
- **[Getting Started](01-Getting-Started/)** - First steps with DS1Edit
- **[Basic Tutorials](02-Tutorials/)** - Step-by-step map editing guide
- **[Advanced Guides](03-Advanced-Guides/)** - Adding monsters, objects, and more
- **[Examples](04-Examples/)** - Sample projects and maps

### Technical Reference
- **[API Reference](API_REFERENCE.md)** - Command-line and configuration reference
- **[DS1/Maze Format](d2_maze_ds1_docs.md)** - File format specifications and maze generation
- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions
- **[COF Pipeline](guides/PD2_S12_COF_PIPELINE.md)** - COF animation pipeline details

### Project Reports
- Reports in [project_reports/](project_reports/) document historical analysis
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

# DS1Edit Documentation

DS1Edit 2.2.0 is a level editor for Diablo II, allowing creation and modification of game maps (.ds1 files). Built with Allegro 5 for GPU-accelerated rendering. See the [changelog](../CHANGELOG.md) for what changed in this release. It continues Paul Siramy's `win_ds1edit` (2002-2011): the user guides below are his original documentation, and [NOTICE](../NOTICE) records the provenance of the code.

## Documentation Index

### Build and Setup
- **[Main README](../README.md)** - Project overview and quick start
- **[Building](../BUILDING.md)** - Build prerequisites and instructions
- **[Project Structure](../docs/structure.md)** - Directory layout and architecture

### User Guides (by Paul Siramy)
- **[Getting Started](getting-started/)** - First steps with DS1Edit; the [manual](getting-started/manual.md) notes where the current editor differs from the 2006 original (controls, saving, configuration)
- **[Basic Tutorials](tutorials/)** - Step-by-step map editing guide
- **[Advanced Guides](guides/)** - Adding monsters, objects, and more

### Preserved Originals
- **[Paul Siramy's original documentation](preservation/)** - his ds1edit site,
  archived byte for byte. The Markdown guides above are converted from these;
  the originals are authoritative.

### Technical Reference
- **[API Reference](reference/api.md)** - Command-line and configuration reference
- **[DS1/Maze Format](reference/maze-and-ds1.md)** - File format specifications and maze generation
- **[Troubleshooting](reference/troubleshooting.md)** - Common issues and solutions
- **[COF Pipeline](guides/cof-pipeline-1.13c.md)** - COF animation pipeline, reverse-engineered from the 1.13c DLLs

### Testing and Release
- **[Release readiness](release-readiness.md)** - Current verdict, open items, evidence, defects found and fixed
- **[Changelog](../CHANGELOG.md)** - User-facing changes per release
- **[Behavior ledger](testing/behavior-ledger.md)** - Every required release case, its lane and platforms, and the evidence that exists
- **[Native release checklist](testing/native-release-checklist.md)** - Manual GUI and clean-environment checks per platform
- **[Equivalence evidence](../test/equivalence/README.md)** - Contracts, recorded original-game results, and the scripts behind them
- **[Qualification records](../release/qualification/README.md)** - What a reviewed record binds and what CI must upload
- **[Vanilla 1.13c testing design](superpowers/specs/2026-10-21-vanilla-113c-testing-design.md)** and its [roadmap](superpowers/plans/2026-10-21-vanilla-113c-testing.md) with package plans [A results](superpowers/plans/2026-10-21-testing-a-results.md), [B equivalence](superpowers/plans/2026-10-21-testing-b-equivalence.md), [C map integrity](superpowers/plans/2026-10-21-testing-c-map-integrity.md), [D visuals](superpowers/plans/2026-10-21-testing-d-visuals.md), [E robustness](superpowers/plans/2026-10-21-testing-e-robustness.md), [F release](superpowers/plans/2026-10-21-testing-f-release.md)

### Software renderer (in development, not in 2.2.0)
- **[Software renderer design](superpowers/specs/2026-10-22-software-renderer-design.md)** - Goal: run without 3D hardware (design only; not shipped yet)
- **[Phase 1 plan](superpowers/plans/2026-10-22-software-renderer-phase1.md)** - First implementation phase

### Design Plans
- Plans in [plans/](plans/) record the design reasoning behind shipped
  features -- MPQ integration, character composition, export presets and
  progress. Kept for intent, not maintained as current documentation.

## Quick Reference

### Command Line
```bash
cd bin

# GUI: area browser, no arguments (set d2_install in Ds1edit.ini)
ds1edit.exe

# Single DS1 file
ds1edit.exe <file.ds1> <LvlTypes.txt ID> <LvlPrest.txt DEF>

# Multiple DS1 files via INI
ds1edit.exe <file.ini>

# Headless screenshot
ds1edit.exe <file.ds1> <ID> <DEF> --headless output.png
```

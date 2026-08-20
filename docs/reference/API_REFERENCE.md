# DS1Edit API Reference

## Command Line Interface

### Basic Usage
```bash
win_ds1edit_debug.exe [mapfile.ds1] [tileset_id] [def_value]
```

### Parameters

#### mapfile.ds1
- **Type**: String (file path)
- **Description**: Path to the DS1 file to load/edit
- **Example**: `assets/tiles/ACT3/Jungle/clearing1.ds1`

#### tileset_id  
- **Type**: Integer
- **Description**: Tileset identifier for the area type
- **Valid Values**:
  - `2` - Act 1 Cave
  - `4` - Act 1 Catacombs  
  - `5` - Act 1 Monastery
  - `10` - Act 2 Desert
  - `12` - Act 2 Tomb
  - `18` - Act 2 Maggot
  - `20` - Act 3 Town/Kurast
  - `21` - Act 3 Jungle/Clearing
  - `22` - Act 3 Temple/Travincal/Mephisto
  - `23` - Act 3 Spider
  - `27` - Act 4 Mesa
  - `28` - Act 4 Lava
  - `29` - Act 4 Diablo
  - `30` - Act 5 Siege
  - `31` - Act 5 Barricade
  - `35` - Act 5 Ice/Lava

#### def_value
- **Type**: Integer
- **Description**: DEF value from LvlPrest.txt for level definition
- **Range**: Varies by area (see LvlPrest.txt reference)

### Exit Codes

| Code | Meaning | Description |
|------|---------|-------------|
| 0 | Success | File loaded and processed successfully |
| 1 | Error | General error (file not found, invalid format) |
| 3 | Format Error | DS1 file format issues or corruption |

## Configuration File Format

### INI File Structure
```ini
tileset_id def_value filepath
tileset_id def_value filepath
...
```

### Example Configuration
```ini
# Act 3 Clearing areas
21 575 assets/tiles/ACT3/Jungle/clearing1.ds1
21 576 assets/tiles/ACT3/Jungle/clearing2.ds1
21 577 assets/tiles/ACT3/Jungle/clearing3.ds1
```

## Tileset Reference

### Act 1 Tilesets
- **Cave (2)**: Underground cave areas
- **Catacombs (4)**: Monastery basement levels
- **Monastery (5)**: Surface monastery buildings

### Act 2 Tilesets  
- **Desert (10)**: Outdoor desert areas
- **Tomb (12)**: Underground tomb complexes
- **Maggot (18)**: Maggot lair tunnels

### Act 3 Tilesets
- **Town/Kurast (20)**: City and town areas
- **Jungle/Clearing (21)**: Outdoor jungle areas
- **Temple/Travincal/Mephisto (22)**: Temple complexes and boss areas
- **Spider (23)**: Spider cavern areas

### Act 4 Tilesets
- **Mesa (27)**: Outer fortress and mesa areas
- **Lava (28)**: Lava-filled areas and forge
- **Diablo (29)**: Final boss encounter areas

### Act 5 Tilesets
- **Siege (30)**: Siege warfare areas
- **Barricade (31)**: Barricade and fortification areas
- **Ice/Lava (35)**: Mixed ice and lava environments

## File Validation Results

### Compatibility Statistics

| Area | Tileset | Total Files | Working | Success Rate |
|------|---------|-------------|---------|--------------|
| Act 2 Maggot | 18 | 40 | 2 | 5.0% |
| Act 3 Clearing | 21 | 90 | 90 | 100.0% |
| Act 3 Kurast | 20 | 85 | 85 | 100.0% |
| Act 3 Mephisto | 22 | 55 | 55 | 100.0% |
| Act 4 Mesa | 27 | 145 | 42 | 29.0% |
| Act 4 Lava | 28 | 45 | 43 | 95.6% |
| Act 5 Siege | 30 | 55 | 15 | 27.3% |

### Common Error Patterns

#### Exit Code 1 - Act Number Mismatch
- **Cause**: DS1 file internal act number doesn't match tileset requirements
- **Example**: Act 2 files used with Act 4 tilesets
- **Solution**: Use files from correct act directory

#### Exit Code 3 - Format Issues  
- **Cause**: File corruption, invalid format, or missing dependencies
- **Common Files**: Border variants, extended tile sets, pit files
- **Solution**: Use core area files, avoid problematic variants

## DEF Value Ranges

### Act 3 Areas
- **Clearing**: 575-604 (30 values)
- **Kurast**: 605-652 (48 values)  
- **Travincal**: 705-710 (6 values)
- **Temple**: 711-723 (13 values)
- **Mephisto**: 754-796 (43 values)

### Act 4 Areas
- **Fortress**: 797-799 (3 values)
- **Mesa**: 811-939 (129 values)
- **Lava**: 836-880 (45 values)
- **Diablo**: 855-862 (8 values)

### Act 5 Areas
- **Siege**: 865-911 (47 values)

## Best Practices

### File Selection
1. **Use core files first**: Basic area files have highest compatibility
2. **Avoid extended variants**: _2, _3, _4 variants often have issues
3. **Test systematically**: Use provided test scripts
4. **Check act compatibility**: Ensure DS1 act matches tileset act

### Configuration Management
1. **Separate by area type**: Create focused INI files per area
2. **Use descriptive names**: `act3_clearing.ini`, `act4_lava.ini`
3. **Document DEF ranges**: Keep track of used DEF values
4. **Validate regularly**: Run test scripts after changes

### Development Workflow
1. **Start with working files**: Use `*_working.ini` files as base
2. **Test incrementally**: Add files one by one
3. **Monitor error logs**: Check `logs/` directory for issues
4. **Use debug build**: More detailed error reporting

## Error Handling

### File Not Found
```bash
# Check file path exists
if exist "assets/tiles/ACT3/Jungle/clearing1.ds1" (
    bin\win_ds1edit_debug.exe "assets/tiles/ACT3/Jungle/clearing1.ds1" 21 575
) else (
    echo File not found
)
```

### Validation Loop
```bash
# Test multiple files with error checking
for /f "tokens=1,2,3*" %%a in (assets\act3_clearing.ini) do (
    bin\win_ds1edit_debug.exe "%%c" %%a %%b
    if !errorlevel! neq 0 (
        echo ERROR: %%c failed with exit code !errorlevel!
    )
)
```

## Utility Scripts

### Test Scripts
- `scripts/batch/test_clearing_lines.bat` - Test Act 3 clearing areas
- `scripts/batch/test_mesa_lines.bat` - Test Act 4 mesa areas  
- `scripts/batch/test_lava_lines.bat` - Test Act 4 lava areas
- `scripts/batch/test_siege_lines.bat` - Test Act 5 siege areas

### Python Utilities
- `scripts/python/generate_level_inis.py` - Generate INI files from LvlPrest.txt
- `scripts/python/test_ini.py` - INI file validation utilities
- `scripts/python/rename_tileset_files.py` - File management tools

## Integration Examples

### Batch Processing
```bash
@echo off
setlocal enabledelayedexpansion

:: Process all files in an INI
for /f "tokens=1,2,3*" %%a in (assets\act3_clearing.ini) do (
    echo Processing %%c...
    bin\win_ds1edit_debug.exe "%%c" %%a %%b
    if !errorlevel! equ 0 (
        echo SUCCESS: %%c
    ) else (
        echo FAILED: %%c with code !errorlevel!
    )
)
```

### Python Integration
```python
import subprocess
import sys

def test_ds1_file(filepath, tileset, def_value):
    """Test a DS1 file with validation."""
    cmd = [
        'bin/win_ds1edit_debug.exe',
        filepath,
        str(tileset),
        str(def_value)
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0
    except Exception as e:
        print(f"Error testing {filepath}: {e}")
        return False

# Usage example
success = test_ds1_file(
    'assets/tiles/ACT3/Jungle/clearing1.ds1',
    21,
    575
)
print(f"Test result: {'PASS' if success else 'FAIL'}")
```

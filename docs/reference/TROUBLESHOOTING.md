# DS1Edit Troubleshooting Guide

## Common Issues and Solutions

### Build Issues

#### Error: "MSVC compiler not found"
**Problem**: Visual Studio or MSVC build tools not installed or not in PATH.

**Solution**:
1. Install Visual Studio 2019+ with C++ workload
2. Or install Build Tools for Visual Studio
3. Ensure vcvars32.bat is accessible:
   ```bash
   "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
   ```

#### Error: "Allegro library not found"
**Problem**: Allegro dependencies missing or incorrectly configured.

**Solution**:
1. Verify Allegro files in `third_party/allegro/`
2. Check library paths in build script
3. Re-run Allegro setup:
   ```bash
   scripts\batch\build_allegro.bat
   ```

### Runtime Issues

#### Exit Code 1: File Not Found
**Symptoms**: DS1Edit exits immediately with code 1

**Causes and Solutions**:
- **File path incorrect**: Verify file exists at specified path
- **Working directory wrong**: Run from project root
- **File permissions**: Ensure read access to DS1 files

**Example Fix**:
```bash
# Wrong - relative path from wrong directory
bin\win_ds1edit_debug.exe clearing1.ds1 21 575

# Correct - full path from project root  
bin\win_ds1edit_debug.exe assets\tiles\ACT3\Jungle\clearing1.ds1 21 575
```

#### Exit Code 3: Format/Compatibility Issues
**Symptoms**: File loads but DS1Edit crashes or returns error code 3

**Common Causes**:
1. **Act number mismatch**: DS1 internal act doesn't match tileset
2. **File corruption**: DS1 file is damaged or incomplete
3. **Unsupported variants**: Extended file variants (_2, _3, _4)
4. **Missing dependencies**: Required tile data not available

**Solutions**:
```bash
# Check act compatibility
# Act 2 files should use Act 2 tilesets (10, 12, 18)
# Act 3 files should use Act 3 tilesets (20, 21, 22, 23)

# Use core variants instead of extended
# Good: clearing1.ds1, clearing2.ds1  
# Avoid: clearing1_2.ds1, clearing1_3.ds1

# Test with working file first
bin\win_ds1edit_debug.exe assets\tiles\ACT3\Jungle\clearing1.ds1 21 575
```

### Configuration Issues

#### INI Files Not Loading Properly
**Problem**: Configuration files seem to be ignored

**Check**:
1. File format is correct (tileset def_value filepath)
2. File paths are valid
3. No extra spaces or tabs
4. File encoding is ASCII/UTF-8

**Example Valid INI**:
```ini
21 575 assets/tiles/ACT3/Jungle/clearing1.ds1
21 576 assets/tiles/ACT3/Jungle/clearing2.ds1
```

**Example Invalid INI**:
```ini
# Wrong format - missing spaces
21,575,assets/tiles/ACT3/Jungle/clearing1.ds1

# Wrong format - extra columns
21 575 assets/tiles/ACT3/Jungle/clearing1.ds1 extra_data

# Wrong paths - backslashes on some systems
21 575 assets\tiles\ACT3\Jungle\clearing1.ds1
```

### Testing Script Issues

#### Test Scripts Fail to Run
**Problem**: `scripts\batch\test_*_lines.bat` scripts don't execute

**Solutions**:
1. **Run from correct directory**:
   ```bash
   # From project root
   scripts\batch\test_clearing_lines.bat
   
   # Or change to scripts directory
   cd scripts\batch
   test_clearing_lines.bat
   ```

2. **Check script permissions**: Ensure batch files are executable
3. **Verify INI file exists**: Script target file must exist

#### All Tests Fail
**Problem**: Every line in test script returns errors

**Check**:
1. **DS1Edit executable exists**: `bin\win_ds1edit_debug.exe`
2. **Working directory correct**: Scripts assume project root
3. **Asset files present**: Check `assets\tiles\` structure
4. **Build successful**: Rebuild if executable is missing

**Debug Steps**:
```bash
# Verify executable
bin\win_ds1edit_debug.exe --help

# Check single file manually
bin\win_ds1edit_debug.exe assets\tiles\ACT3\Jungle\clearing1.ds1 21 575

# Check INI file format
type assets\act3_clearing.ini | head -5
```

### Performance Issues

#### Slow Loading Times
**Problem**: DS1 files take very long to load

**Causes**:
- Large file sizes
- Hard drive performance
- Debug build overhead

**Solutions**:
1. Use release build for better performance:
   ```bash
   build.bat release
   bin\win_ds1edit.exe [file] [tileset] [def]
   ```

2. Move frequently used files to SSD if available
3. Close other applications using disk I/O

#### Memory Issues
**Problem**: Application crashes with large files or after extended use

**Solutions**:
1. Restart application periodically
2. Process files in smaller batches
3. Check available system memory
4. Use 64-bit build if available

### File Organization Issues

#### Missing Asset Files
**Problem**: Can't find DS1 files or configuration

**Check Project Structure**:
```bash
# Verify organized structure
dir assets\tiles
dir config
dir scripts\batch
```

**If files are missing**:
1. Re-run organization script:
   ```bash
   scripts\batch\organize_project.bat
   ```

2. Check if files were moved to different location
3. Restore from backup if necessary

#### Path Length Issues (Windows)
**Problem**: "Path too long" errors on Windows

**Solutions**:
1. Enable long path support in Windows 10/11:
   ```
   Group Policy: Computer Configuration > Administrative Templates > System > Filesystem > Enable Win32 long paths
   ```

2. Move project closer to root:
   ```bash
   # Instead of: C:\Users\Username\Very\Long\Path\DS1Edit
   # Use: C:\DS1Edit
   ```

3. Use shorter folder names in project structure

### Integration Issues

#### Python Scripts Not Working
**Problem**: `scripts\python\*.py` files fail to execute

**Requirements**:
1. Python 3.6+ installed and in PATH
2. Required modules available

**Test Python Setup**:
```bash
python --version
python -c "import sys; print(sys.executable)"
```

**Run Scripts**:
```bash
# From project root
python scripts\python\generate_level_inis.py

# Or specify full Python path
C:\Python39\python.exe scripts\python\generate_level_inis.py
```

#### VS Code Integration Issues
**Problem**: Build tasks don't work in VS Code

**Check**:
1. `.vscode\tasks.json` exists and is valid
2. C++ extension installed
3. Workspace opened from project root

**Reset VS Code Configuration**:
1. Close VS Code
2. Delete `.vscode\settings.json` (keeps tasks.json)
3. Reopen project
4. Configure C++ extension

### Advanced Debugging

#### Enable Verbose Logging
Add debug output to troubleshoot complex issues:

```bash
# Run with output redirection
bin\win_ds1edit_debug.exe [file] [tileset] [def] > logs\debug_output.txt 2>&1

# Check error logs
type logs\stderr.txt
```

#### Memory and Performance Profiling
For persistent issues:

1. **Monitor Task Manager**: Check CPU and memory usage
2. **Use Process Monitor**: Track file access patterns
3. **Enable heap checking**: Use debug builds with additional checks

#### Trace File Processing
Create detailed logs of file processing:

```bash
# Create trace log
@echo off
echo Starting DS1Edit trace at %date% %time% > logs\trace.log

for /f "tokens=1,2,3*" %%a in (assets\act3_clearing.ini) do (
    echo Processing %%c... >> logs\trace.log
    bin\win_ds1edit_debug.exe "%%c" %%a %%b >> logs\trace.log 2>&1
    echo Exit code: !errorlevel! >> logs\trace.log
    echo. >> logs\trace.log
)
```

## Getting Help

### Before Reporting Issues
1. **Check this troubleshooting guide**
2. **Try with a known working file**:
   ```bash
   bin\win_ds1edit_debug.exe assets\tiles\ACT3\Jungle\clearing1.ds1 21 575
   ```
3. **Run test scripts** to validate setup
4. **Check logs directory** for error details

### Information to Include
When reporting issues, include:
- **Operating system version**
- **Visual Studio/MSVC version**
- **Complete command line used**
- **Full error message or exit code**
- **Contents of logs\ directory**
- **Steps to reproduce**

### Quick Diagnostic Commands
```bash
# System information
systeminfo | findstr /C:"OS Name" /C:"OS Version"

# Build environment
where cl.exe
cl.exe 2>&1 | head -1

# Project state
dir bin\win_ds1edit*.exe
dir assets\tiles\ACT3\Jungle\clearing1.ds1
type assets\act3_clearing.ini | head -5
```

This diagnostic information helps identify the root cause quickly and provides the context needed for effective troubleshooting.

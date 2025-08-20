@echo off
REM Clean build script for DS1Edit - Updated for organized project structure

echo Cleaning build artifacts...

REM Remove object files from build directory
if exist build\*.obj (
    del build\*.obj
    echo Deleted object files from build\
)

REM Remove executables from bin directory
if exist bin\win_ds1edit.exe (
    del bin\win_ds1edit.exe
    echo Deleted bin\win_ds1edit.exe
)
if exist bin\win_ds1edit_debug.exe (
    del bin\win_ds1edit_debug.exe
    echo Deleted bin\win_ds1edit_debug.exe
)

REM Remove any temp files in root
if exist *.tmp (
    del *.tmp
    echo Deleted temporary files
)

REM Remove debug files
if exist *.pdb (
    del *.pdb
    echo Deleted debug files
)
if exist *.ilk (
    del *.ilk
    echo Deleted incremental link files
)

echo Clean completed!

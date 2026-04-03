@echo off
REM Build script for DS1Edit - Allegro 5 version
REM Uses direct cl compilation (same approach as original Allegro 4 build)

REM Set up Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"

REM Allegro 5 paths (via vcpkg)
set A5_INCLUDE=C:\vcpkg\installed\x86-windows\include
set A5_LIB=C:\vcpkg\installed\x86-windows\lib

REM Clean previous build artifacts
if exist build\*.obj del build\*.obj

REM Check if we're building debug or release
if "%1"=="release" goto RELEASE

:DEBUG
echo Building DS1Edit Debug version (Allegro 5)...
cl /I. /ISources /I%A5_INCLUDE% /DWIN32 /D_DEBUG /DUSE_CONSOLE /Zi /Od /MDd /Fo:build\ /Fe:bin\win_ds1edit_debug.exe Sources\*.c Sources\mpq\*.c /link /LIBPATH:%A5_LIB% allegro.lib allegro_image.lib allegro_font.lib allegro_primitives.lib allegro_main.lib user32.lib gdi32.lib ole32.lib winmm.lib shlwapi.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)
echo Debug build completed successfully!
echo Executable: bin\win_ds1edit_debug.exe
goto END

:RELEASE
echo Building DS1Edit Release version (Allegro 5)...
cl /I. /ISources /I%A5_INCLUDE% /DWIN32 /DNDEBUG /DUSE_CONSOLE /O2 /MD /Fo:build\ /Fe:bin\win_ds1edit.exe Sources\*.c Sources\mpq\*.c /link /LIBPATH:%A5_LIB% allegro.lib allegro_image.lib allegro_font.lib allegro_primitives.lib allegro_main.lib user32.lib gdi32.lib ole32.lib winmm.lib shlwapi.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)
echo Release build completed successfully!
echo Executable: bin\win_ds1edit.exe

:END

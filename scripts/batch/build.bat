@echo off
REM Build script for DS1Edit - Updated for organized project structure

REM Set up Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"

REM Clean previous build artifacts
if exist build\*.obj del build\*.obj

REM Check if we're building debug or release
if "%1"=="release" goto RELEASE

:DEBUG
echo Building DS1Edit Debug version...
cl /I. /ISources /Ithird_party\allegro-4.4.3.1\include /Ithird_party\allegro-build\include /DWIN32 /D_DEBUG /DUSE_CONSOLE /Zi /Od /MDd /Fo:build\ /Fe:bin\win_ds1edit_debug.exe Sources\*.c Sources\mpq\*.c /link bin\allegro-4.4.2-monolith-md.lib user32.lib gdi32.lib ole32.lib dinput8.lib ddraw.lib dxguid.lib winmm.lib dsound.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)
echo Debug build completed successfully!
echo Executable: bin\win_ds1edit_debug.exe
goto END

:RELEASE
echo Building DS1Edit Release version...
cl /I. /ISources /Ithird_party\allegro-4.4.3.1\include /Ithird_party\allegro-build\include /DWIN32 /DNDEBUG /DUSE_CONSOLE /O2 /MD /Fo:build\ /Fe:bin\win_ds1edit.exe Sources\*.c Sources\mpq\*.c /link bin\allegro-4.4.2-monolith-md.lib user32.lib gdi32.lib ole32.lib dinput8.lib ddraw.lib dxguid.lib winmm.lib dsound.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)
echo Release build completed successfully!
echo Executable: bin\win_ds1edit.exe

:END

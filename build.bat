@echo off
REM Convenience build script - calls the organized build script

echo Building DS1Edit from organized project structure...
call "scripts\batch\build.bat" %*

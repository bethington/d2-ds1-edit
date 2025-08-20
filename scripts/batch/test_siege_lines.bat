@echo off
setlocal enabledelayedexpansion

echo Testing each line in assets\act5_siege.ini individually...
echo.

set total_lines=0
set working_lines=0
set error_lines=0

:: Create temporary files for results
> assets\act5_siege_working.ini echo.
> assets\act5_siege_errors.txt echo.

:: Process each line in the act5_siege.ini file
for /f "tokens=1,2,3*" %%a in (assets\act5_siege.ini) do (
    set /a total_lines+=1
    
    echo Testing line !total_lines!: %%a %%b %%c
    
    :: Run the debug executable with the parameters
    bin\win_ds1edit_debug.exe "%%c" %%a %%b >nul 2>&1
    
    if !errorlevel! equ 0 (
        echo   [SUCCESS] %%c with tileset %%a and DEF %%b
        set /a working_lines+=1
        >> assets\act5_siege_working.ini echo %%a %%b %%c
    ) else (
        echo   [ERROR] %%c with tileset %%a and DEF %%b - Exit code: !errorlevel!
        set /a error_lines+=1
        >> assets\act5_siege_errors.txt echo ERROR: %%a %%b %%c - Exit code: !errorlevel!
    )
    echo.
)

echo ============================================
echo SUMMARY:
echo Total lines tested: !total_lines!
echo Working files: !working_lines!
echo Error files: !error_lines!
echo Success rate: !working_lines!/!total_lines!
echo ============================================
echo.
echo Working files saved to: assets\act5_siege_working.ini
echo Error details saved to: assets\act5_siege_errors.txt

pause

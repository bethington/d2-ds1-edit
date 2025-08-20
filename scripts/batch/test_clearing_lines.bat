@echo off
setlocal enabledelayedexpansion

echo Testing each line in assets\act3_clearing.ini individually...
echo.

set /a total=0
set /a successful=0
set /a failed=0

for /f "tokens=1,2,3 delims= " %%a in ('findstr /v "^#" assets\act3_clearing.ini') do (
    set /a total+=1
    echo Testing line !total!: %%c %%a %%b
    
    bin\win_ds1edit_debug.exe "%%c" %%a %%b > nul 2>&1
    
    if !errorlevel! equ 0 (
        echo   SUCCESS: %%c with tileset %%a and DEF %%b
        set /a successful+=1
    ) else (
        echo   FAILED: %%c with tileset %%a and DEF %%b ^(Exit code: !errorlevel!^)
        set /a failed+=1
    )
    echo.
)

echo.
echo ========================================
echo Testing Summary:
echo Total lines tested: !total!
echo Successful: !successful!
echo Failed: !failed!
echo ========================================

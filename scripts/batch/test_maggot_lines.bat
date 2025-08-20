@echo off
setlocal enabledelayedexpansion

echo Testing each line in assets\act2_maggot.ini individually...
echo Format: bin\win_ds1edit_debug.exe ^<filepath^> ^<tileset_id^> ^<def_value^>
echo.

set "successful=0"
set "failed=0"
set "line_num=0"

for /f "tokens=1,2,3" %%a in (assets\act2_maggot.ini) do (
    set /a line_num+=1
    echo Testing line !line_num!: bin\win_ds1edit_debug.exe %%c %%a %%b
    
    bin\win_ds1edit_debug.exe %%c %%a %%b
    set "exit_code=!errorlevel!"
    
    if !exit_code! equ 0 (
        echo   SUCCESS ^(exit code !exit_code!^)
        set /a successful+=1
    ) else (
        echo   ERROR ^(exit code !exit_code!^)
        set /a failed+=1
    )
    echo.
)

echo ============================================================
echo SUMMARY:
echo Total lines tested: !line_num!
echo Successful: !successful!
echo Failed: !failed!
echo ============================================================

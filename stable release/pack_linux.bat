@echo off
REM ============================================================
REM  Packs the Linux folder as .tar.gz
REM  Can be run standalone or called from stable_build_and_package.bat
REM ============================================================

set AP_VERSION=1.4.1
set DIST_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 with Archipelago-exp\dist
set LINUX_RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-linux-amd64
set LINUX_FILE=%LINUX_RELEASE_NAME%.tar.gz

if not exist "%DIST_DIR%\%LINUX_RELEASE_NAME%" (
    echo [ERROR] Linux folder not found: %DIST_DIR%\%LINUX_RELEASE_NAME%
    echo         Run stable_build_and_package.bat first!
    pause
    exit /b 1
)

if exist "%DIST_DIR%\%LINUX_FILE%" del /f "%DIST_DIR%\%LINUX_FILE%"

echo Creating %LINUX_FILE%...
cd /d "%DIST_DIR%"
"C:\Windows\System32\tar.exe" -czf "%LINUX_FILE%" "%LINUX_RELEASE_NAME%"

if exist "%DIST_DIR%\%LINUX_FILE%" (
    echo [OK] %DIST_DIR%\%LINUX_FILE%
    for %%A in ("%DIST_DIR%\%LINUX_FILE%") do echo     Size: %%~zA bytes
) else (
    echo [ERROR] tar.gz was not created!
)

pause
exit /b 0

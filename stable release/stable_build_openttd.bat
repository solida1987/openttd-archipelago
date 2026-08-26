@echo off
setlocal
:: ============================================================
::  OpenTTD Archipelago — STABLE Quick Build (no packaging)
::  Builds from EXP folder with stable branding.
::  Use this for quick compile-testing before full package.
:: ============================================================
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 with Archipelago-exp
set BUILD_DIR=%PROJECT_DIR%\build
set VCPKG_TOOLCHAIN=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

:: Find Visual Studio vcvars64.bat automatisk
set VCVARS=
for %%E in (Community Professional Enterprise) do (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
)
if not defined VCVARS (
    echo [FEJL] Kunne ikke finde Visual Studio 2022!
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  OpenTTD Archipelago STABLE — Quick Build
echo  Source: %PROJECT_DIR%
echo ============================================================
echo.

:: ── Patch game name til stable ─────────────────────────────
echo [1/5] Patcher game name: OpenTTD-Exp  -^>  OpenTTD ...
:: Gem backup
copy /Y "%PROJECT_DIR%\src\archipelago_gui.cpp" "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" > nul
copy /Y "%PROJECT_DIR%\src\archipelago.h"       "%PROJECT_DIR%\src\archipelago.h.bak"       > nul
:: Patch
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago_gui.cpp') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago_gui.cpp'"
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago.h') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago.h'"
echo       OK: game name = OpenTTD (stable)

echo [2/5] Aktiverer Visual Studio build-miljoe...
call "%VCVARS%" > nul 2>&1
echo       Aktiveret.

cd /d "%PROJECT_DIR%"

echo [3/5] Sletter gammel build-mappe...
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo       Slettet.
) else (
    echo       Ingen gammel build-mappe - springer over.
)

echo [4/5] Konfigurerer med CMake...
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
echo ============================================================
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN%
if errorlevel 1 (
    echo.
    echo [FEJL] CMake konfiguration fejlede!
    goto :revert_and_exit
)

echo.
echo [5/5] Bygger projektet (RelWithDebInfo)...
echo ============================================================
cmake --build . --config RelWithDebInfo
if errorlevel 1 (
    echo.
    echo ============================================================
    echo [FEJL] Bygning fejlede! Se fejl ovenfor.
    echo ============================================================
    goto :revert_and_exit
)

:: ── Revert game name ────────────────────────────────────────
echo.
echo Reverter source-filer fra backup ...
cd /d "%PROJECT_DIR%"
move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
echo       OK: game name revertet til OpenTTD-Exp

echo.
echo ============================================================
echo  STABLE BUILD SUCCESFULD!
echo  Output: %BUILD_DIR%\RelWithDebInfo\openttd.exe
echo ============================================================
echo.
pause
exit /b 0

:revert_and_exit
echo.
echo Reverter source-filer fra backup efter fejl...
cd /d "%PROJECT_DIR%"
if exist "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
if exist "%PROJECT_DIR%\src\archipelago.h.bak"       move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
echo       OK: game name revertet til OpenTTD-Exp
pause
exit /b 1

@echo off
setlocal
:: ============================================================
::  OpenTTD - Clean Build Script
::  Dobbeltklik denne fil direkte - kraever IKKE VS Developer Prompt
:: ============================================================
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\OpenTTD 15.2 with Archipelago-exp
set BUILD_DIR=%PROJECT_DIR%\build
set VCPKG_TOOLCHAIN=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

:: Find Visual Studio vcvars64.bat automatisk
set VCVARS=
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
    echo [FEJL] Kunne ikke finde Visual Studio 2022!
    pause
    exit /b 1
)
echo.
echo ============================================================
echo  OpenTTD Clean Build
echo ============================================================
echo.
echo [0/4] Aktiverer Visual Studio build-miljoe...
call "%VCVARS%" > nul 2>&1
echo       Aktiveret.
cd /d "%PROJECT_DIR%"
if errorlevel 1 (
    echo [FEJL] Kunne ikke finde projektmappen: %PROJECT_DIR%
    pause
    exit /b 1
)
if exist "%BUILD_DIR%" (
    echo [1/4] Sletter gammel build-mappe...
    rmdir /s /q "%BUILD_DIR%"
    echo       Slettet.
) else (
    echo [1/4] Ingen gammel build-mappe - springer over.
)
echo [2/4] Opretter ny build-mappe...
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
echo       Klar: %BUILD_DIR%
echo.
echo [3/4] Konfigurerer med CMake...
echo ============================================================
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN%
if errorlevel 1 (
    echo.
    echo [FEJL] CMake konfiguration fejlede!
    pause
    exit /b 1
)
echo.
echo [4/4] Bygger projektet (RelWithDebInfo)...
echo ============================================================
cmake --build . --config RelWithDebInfo
if errorlevel 1 (
    echo.
    echo ============================================================
    echo [FEJL] Bygning fejlede! Se fejl ovenfor.
    echo ============================================================
    pause
    exit /b 1
)
echo.
echo ============================================================
echo  BUILD SUCCESFULD!
echo  Output: %BUILD_DIR%\RelWithDebInfo\openttd.exe
echo ============================================================
echo.
pause

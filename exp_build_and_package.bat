@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago — Build + Package Script
::  Semantic versioning: v MAJOR.MINOR.PATCH
::    PATCH  = bugfix / text-rettelse
::    MINOR  = ny feature / nyt indhold
::    MAJOR  = stort gameplay-skift / breaking change
::  Dobbeltklik for at bygge. Kræver ikke VS Developer Prompt.
:: ============================================================

:: ── VERSION ─────────────────────────────────────────────────
set AP_VERSION=exp-4.0

:: ── STIER ───────────────────────────────────────────────────
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\OpenTTD 15.2 with Archipelago-exp
set BUILD_DIR=%PROJECT_DIR%\build
set DIST_DIR=%PROJECT_DIR%\dist
set VCPKG_ROOT=C:\vcpkg
set VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

:: ── Læs OpenTTD-version fra .version-filen ──────────────────
set /p OTTD_VERSION=<"%PROJECT_DIR%\.version"
if not defined OTTD_VERSION set OTTD_VERSION=15.2

set RELEASE_NAME=openttd-archipelago-%AP_VERSION%-win64
set ZIP_NAME=%RELEASE_NAME%.zip

echo.
echo ============================================================
echo  OpenTTD Archipelago Build + Package
echo  AP Version  : v%AP_VERSION%
echo  OpenTTD     : %OTTD_VERSION%
echo  Output      : %DIST_DIR%\%ZIP_NAME%
echo ============================================================
echo.

:: ── Find Visual Studio ───────────────────────────────────────
set VCVARS=
for %%E in (Community Professional Enterprise) do (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
)
if not defined VCVARS (
    echo [FEJL] Visual Studio 2022 ikke fundet!
    pause & exit /b 1
)

echo [1/6] Aktiverer Visual Studio build-miljoe...
call "%VCVARS%" > nul 2>&1
echo       OK: %VCVARS%

:: ── Tjek vcpkg ───────────────────────────────────────────────
echo [2/6] Tjekker vcpkg...
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo       vcpkg.exe ikke fundet - bootstrapper...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics > nul 2>&1
    if errorlevel 1 (
        echo [FEJL] vcpkg bootstrap fejlede!
        pause & exit /b 1
    )
)
echo       Installerer pakker fra vcpkg.json...
cd /d "%PROJECT_DIR%"
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows > nul 2>&1
echo       OK.

:: ── Ryd og opret build-mappe ─────────────────────────────────
echo [3/6] Forbereder build-mappe...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
echo       OK: %BUILD_DIR%

:: ── CMake konfiguration ──────────────────────────────────────
echo [4/6] Konfigurerer CMake...
echo ============================================================
cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
    -DCMAKE_INSTALL_PREFIX="%BUILD_DIR%\install" ^
    -DOPTION_USE_ASSERTS=OFF
if errorlevel 1 (
    echo.
    echo [FEJL] CMake konfiguration fejlede!
    pause & exit /b 1
)

:: ── Byg ─────────────────────────────────────────────────────
echo.
echo [5/6] Bygger (RelWithDebInfo)...
echo ============================================================
cmake --build . --config RelWithDebInfo --parallel
if errorlevel 1 (
    echo.
    echo [FEJL] Build fejlede!
    pause & exit /b 1
)

:: ── Installer til midlertidig mappe ─────────────────────────
echo.
echo [6/6] Pakker release...
echo ============================================================
if exist "%BUILD_DIR%\install" rmdir /s /q "%BUILD_DIR%\install"
cmake --install . --config RelWithDebInfo --prefix "%BUILD_DIR%\install"
if errorlevel 1 (
    echo [FEJL] cmake --install fejlede!
    pause & exit /b 1
)

:: ── Find den installerede openttd.exe ────────────────────────
set INSTALL_BIN=%BUILD_DIR%\install
if exist "%BUILD_DIR%\install\bin\openttd.exe" (
    set INSTALL_BIN=%BUILD_DIR%\install\bin
)

:: ── Byg endelig release-mappe ────────────────────────────────
if exist "%DIST_DIR%\%RELEASE_NAME%" rmdir /s /q "%DIST_DIR%\%RELEASE_NAME%"
mkdir "%DIST_DIR%\%RELEASE_NAME%"
set OUT=%DIST_DIR%\%RELEASE_NAME%

:: Eksekverbar
copy /Y "%INSTALL_BIN%\openttd.exe" "%OUT%\openttd.exe" > nul

:: DLL-filer fra vcpkg
for %%F in ("%BUILD_DIR%\RelWithDebInfo\*.dll") do copy /Y "%%F" "%OUT%\" > nul

:: baseset
if exist "%BUILD_DIR%\install\share\games\openttd\baseset" (
    xcopy /E /I /Q "%BUILD_DIR%\install\share\games\openttd\baseset" "%OUT%\baseset" > nul
) else if exist "%BUILD_DIR%\install\baseset" (
    xcopy /E /I /Q "%BUILD_DIR%\install\baseset" "%OUT%\baseset" > nul
) else (
    xcopy /E /I /Q "%PROJECT_DIR%\baseset" "%OUT%\baseset" > nul
)

:: lang
if exist "%BUILD_DIR%\install\share\games\openttd\lang" (
    xcopy /E /I /Q "%BUILD_DIR%\install\share\games\openttd\lang" "%OUT%\lang" > nul
) else if exist "%BUILD_DIR%\install\lang" (
    xcopy /E /I /Q "%BUILD_DIR%\install\lang" "%OUT%\lang" > nul
) else (
    xcopy /E /I /Q "%BUILD_DIR%\RelWithDebInfo\lang" "%OUT%\lang" > nul 2>&1
    if not exist "%OUT%\lang\english.lng" (
        xcopy /E /I /Q "%BUILD_DIR%\lang" "%OUT%\lang" > nul 2>&1
    )
)

:: ai / game / scripts / docs
for %%D in (share\games\openttd\ai ai) do (
    if exist "%BUILD_DIR%\install\%%D" xcopy /E /I /Q "%BUILD_DIR%\install\%%D" "%OUT%\ai" > nul
)
if not exist "%OUT%\ai" xcopy /E /I /Q "%PROJECT_DIR%\ai" "%OUT%\ai" > nul 2>&1

for %%D in (share\games\openttd\game game) do (
    if exist "%BUILD_DIR%\install\%%D" xcopy /E /I /Q "%BUILD_DIR%\install\%%D" "%OUT%\game" > nul
)
if not exist "%OUT%\game" xcopy /E /I /Q "%PROJECT_DIR%\game" "%OUT%\game" > nul 2>&1

for %%D in (share\games\openttd\scripts scripts) do (
    if exist "%BUILD_DIR%\install\%%D" xcopy /E /I /Q "%BUILD_DIR%\install\%%D" "%OUT%\scripts" > nul
)
if not exist "%OUT%\scripts" xcopy /E /I /Q "%PROJECT_DIR%\scripts" "%OUT%\scripts" > nul 2>&1

for %%D in (share\doc\openttd docs) do (
    if exist "%BUILD_DIR%\install\%%D" xcopy /E /I /Q "%BUILD_DIR%\install\%%D" "%OUT%\docs" > nul
)
if not exist "%OUT%\docs" xcopy /E /I /Q "%PROJECT_DIR%\docs" "%OUT%\docs" > nul 2>&1

:: Dokumenter (rod-niveau)
for %%F in (README.md CONTRIBUTING.md COPYING.md CREDITS.md) do (
    if exist "%PROJECT_DIR%\%%F" copy /Y "%PROJECT_DIR%\%%F" "%OUT%\%%F" > nul
)
copy /Y "%PROJECT_DIR%\CHANGELOG.md"  "%OUT%\CHANGELOG.md"  > nul
copy /Y "%PROJECT_DIR%\KNOWN_BUGS.md" "%OUT%\KNOWN_BUGS.md" > nul
copy /Y "%PROJECT_DIR%\INSTALL.md"    "%OUT%\INSTALL.md"    > nul

:: ── Server.bat launcher (multiplayer) ──────────────────────
if exist "%PROJECT_DIR%\Server.bat" (
    copy /Y "%PROJECT_DIR%\Server.bat" "%OUT%\Server.bat" > nul
    echo   [OK]      Server.bat
) else (
    echo   [ADVARSEL] Server.bat ikke fundet
)

:: ── AP Bridge (Python server) ──────────────────────────────
if exist "%PROJECT_DIR%\bridge\__main__.py" (
    xcopy /E /I /Q "%PROJECT_DIR%\bridge" "%OUT%\bridge" > nul
    echo   [OK]      bridge\
) else (
    echo   [ADVARSEL] bridge\ mappen ikke fundet
)

:: ── Archipelago APWorld ──────────────────────────────────────
if exist "%PROJECT_DIR%\apworld" (
    xcopy /E /I /Q "%PROJECT_DIR%\apworld" "%OUT%\apworld" > nul
    echo   [OK]      apworld\
)

:: ── Money Quests GameScript ──────────────────────────────────
if exist "%PROJECT_DIR%\gamescript\MoneyQuests" (
    xcopy /E /I /Q "%PROJECT_DIR%\gamescript\MoneyQuests" "%OUT%\game\MoneyQuests" > nul
    echo   [OK]      game\MoneyQuests\
) else (
    echo   [ADVARSEL] gamescript\MoneyQuests ikke fundet - springer over.
)

:: ── Bundlede base assets fra media\baseset\ ──────────────────
:: archipelago_icons.grf → OUT\baseset\
if exist "%PROJECT_DIR%\media\baseset\archipelago_icons.grf" (
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_icons.grf" "%OUT%\baseset\archipelago_icons.grf" > nul
    echo   [OK]      baseset\archipelago_icons.grf
) else (
    echo   [ADVARSEL] media\baseset\archipelago_icons.grf ikke fundet
)

:: OpenGFX → udpak fra lokal tar i media\baseset\
set OPENGFX_TAR=%PROJECT_DIR%\media\baseset\opengfx-8.0.tar
if exist "%OPENGFX_TAR%" (
    echo   Udpakker OpenGFX fra lokal bundle...
    powershell -NoProfile -Command "tar -xf '%OPENGFX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenGFX 8.0)
) else (
    echo   [ADVARSEL] media\baseset\opengfx-8.0.tar ikke fundet - springer over.
)

:: OpenSFX → udpak fra lokal tar i media\baseset\
set OPENSFX_TAR=%PROJECT_DIR%\media\baseset\opensfx-1.0.3.tar
if exist "%OPENSFX_TAR%" (
    echo   Udpakker OpenSFX fra lokal bundle...
    powershell -NoProfile -Command "tar -xf '%OPENSFX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenSFX 1.0.3)
) else (
    echo   [ADVARSEL] media\baseset\opensfx-1.0.3.tar ikke fundet - springer over.
)

:: OpenMSX → udpak fra lokal tar i media\baseset\
set OPENMSX_TAR=%PROJECT_DIR%\media\baseset\openmsx-0.4.2.tar
if exist "%OPENMSX_TAR%" (
    echo   Udpakker OpenMSX fra lokal bundle...
    powershell -NoProfile -Command "tar -xf '%OPENMSX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenMSX 0.4.2)
) else (
    echo   [ADVARSEL] media\baseset\openmsx-0.4.2.tar ikke fundet - springer over.
)

:: ── NewGRF (Iron Horse og andre GRF-filer) ───────────────────
if exist "%PROJECT_DIR%\newgrf" (
    xcopy /E /I /Q "%PROJECT_DIR%\newgrf" "%OUT%\newgrf" > nul
    echo   [OK]      newgrf\
) else (
    mkdir "%OUT%\newgrf"
    echo   [ADVARSEL] newgrf\ ikke fundet - oprettet tom mappe.
)

:: archipelago_ruins.grf skal OGSÅ ligge i newgrf\ (ikke kun baseset\)
:: OpenTTD scanner newgrf\ for aktive NewGRFs jf. openttd.cfg [newgrf] sektionen
if exist "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" (
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" "%OUT%\newgrf\archipelago_ruins.grf" > nul
    echo   [OK]      newgrf\archipelago_ruins.grf
) else (
    echo   [ADVARSEL] media\baseset\archipelago_ruins.grf ikke fundet til newgrf\
)

:: ── SimpleAI til data\ai\ (personal dir) ─────────────────────
set SIMPLEAI_TAR=%DIST_DIR%\data_template\content_download\ai\534d504c-SimpleAI-14.tar
if exist "%SIMPLEAI_TAR%" (
    echo   Udpakker SimpleAI til data\ai\...
    if not exist "%OUT%\data\ai" mkdir "%OUT%\data\ai"
    powershell -NoProfile -Command "tar -xf '%SIMPLEAI_TAR%' -C '%OUT%\data\ai'"
    echo   [OK]      data\ai\SimpleAI-14\
) else (
    echo   [ADVARSEL] SimpleAI-14.tar ikke fundet i data_template - springer over.
)

:: ── Data-mappe (portable config + content_download + AI libraries) ──
:: NB: data_template indeholder ai\library\ med alle 4 libraries,
::     saa en separat kopi er ikke noedvendig. /Y undgaar overwrite-prompt.
set DATA_TEMPLATE=%DIST_DIR%\data_template
if exist "%DATA_TEMPLATE%" (
    echo   Kopierer data-mappe fra template...
    xcopy /E /I /Y /Q "%DATA_TEMPLATE%" "%OUT%\data" > nul
    echo   [OK]      data\ (configs, AI libraries, NightGFX, musik)
) else (
    echo   [ADVARSEL] dist\data_template\ ikke fundet - ingen data-mappe.
)

:: ── Verificer vigtigste filer ────────────────────────────────
echo.
echo Verificerer output...
set MISSING=0
for %%F in (openttd.exe baseset\openttd.grf lang\english.lng) do (
    if not exist "%OUT%\%%F" (
        echo   [MANGLER] %%F
        set MISSING=1
    ) else (
        echo   [OK]      %%F
    )
)
if exist "%OUT%\apworld\openttd_exp.apworld" (
    echo   [OK]      apworld\openttd_exp.apworld
) else (
    echo   [ADVARSEL] apworld\openttd_exp.apworld mangler
)
if exist "%OUT%\game\MoneyQuests\main.nut" (
    echo   [OK]      game\MoneyQuests\main.nut
) else (
    echo   [ADVARSEL] game\MoneyQuests\main.nut mangler
)
if exist "%OUT%\data\ai\SimpleAI-14\info.nut" (
    echo   [OK]      data\ai\SimpleAI-14\info.nut
) else (
    echo   [ADVARSEL] data\ai\SimpleAI-14\ mangler (demigod AI)
)
if exist "%OUT%\data\ai\library\pathfinder\road\library.nut" (
    echo   [OK]      data\ai\library\ (pathfinder.road, pathfinder.rail, graph.aystar, queue.binary_heap)
) else (
    echo   [ADVARSEL] data\ai\library\ mangler - SimpleAI kan ikke finde pathfinder!
)
if exist "%OUT%\data\openttd.cfg" (
    echo   [OK]      data\openttd.cfg (portable config)
) else (
    echo   [ADVARSEL] data\openttd.cfg mangler
)
if exist "%OUT%\data\content_download\ai\534d504c-SimpleAI-14.tar" (
    echo   [OK]      data\content_download\ai\SimpleAI-14.tar
) else (
    echo   [ADVARSEL] SimpleAI.tar mangler i content_download
)
if exist "%OUT%\data\content_download\baseset\4e474658-NightGFX-1.3.0.tar" (
    echo   [OK]      data\content_download\baseset\NightGFX-1.3.0.tar
) else (
    echo   [ADVARSEL] NightGFX.tar mangler i content_download
)
if exist "%OUT%\newgrf\archipelago_ruins.grf" (
    echo   [OK]      newgrf\archipelago_ruins.grf
) else (
    echo   [ADVARSEL] archipelago_ruins.grf mangler i newgrf\
)
if exist "%OUT%\Server.bat" (
    echo   [OK]      Server.bat (multiplayer launcher)
) else (
    echo   [ADVARSEL] Server.bat mangler
)
if exist "%OUT%\bridge\__main__.py" (
    echo   [OK]      bridge\__main__.py (AP Bridge)
) else (
    echo   [ADVARSEL] bridge\ mangler
)
if "%MISSING%"=="1" (
    echo.
    echo [ADVARSEL] Kritiske filer mangler - tjek cmake --install.
)

:: ── Zip med PowerShell ───────────────────────────────────────
echo.
echo Opretter %ZIP_NAME%...
if exist "%DIST_DIR%\%ZIP_NAME%" del /f "%DIST_DIR%\%ZIP_NAME%"
powershell -NoProfile -Command ^
    "Compress-Archive -Path '%DIST_DIR%\%RELEASE_NAME%' -DestinationPath '%DIST_DIR%\%ZIP_NAME%' -CompressionLevel Optimal"
if errorlevel 1 (
    echo [FEJL] ZIP-oprettelse fejlede!
    pause & exit /b 1
)

echo.
echo ============================================================
echo  BUILD SUCCESFULD!
echo.
echo  Version : v%AP_VERSION%
echo  EXE     : %OUT%\openttd.exe
echo  SCRIPT  : %OUT%\game\MoneyQuests\
echo  APWORLD : %OUT%\apworld\openttd_exp.apworld
echo  ZIP     : %DIST_DIR%\%ZIP_NAME%
echo ============================================================
echo.
pause

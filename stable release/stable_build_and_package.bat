@echo off
setlocal EnableDelayedExpansion
REM ============================================================
REM  OpenTTD Archipelago -- STABLE Build + Package
REM  Builds from the EXP working directory with stable branding.
REM  Patches "OpenTTD-Exp" -> "OpenTTD" before build, reverts after.
REM
REM  Semantic versioning: v MAJOR.MINOR.PATCH
REM    PATCH  = bugfix / text fix
REM    MINOR  = new feature / new content
REM    MAJOR  = big gameplay shift / breaking change
REM ============================================================

REM -- VERSION ---------------------------------------------------
set AP_VERSION=1.4.1

REM -- PATHS -----------------------------------------------------
REM We build directly from the EXP folder -- no separate copy needed
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 with Archipelago-exp
set BUILD_DIR=%PROJECT_DIR%\build
set DIST_DIR=%PROJECT_DIR%\dist
set VCPKG_ROOT=C:\vcpkg
set VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

REM -- Read OpenTTD version from .version file -------------------
set /p OTTD_VERSION=<"%PROJECT_DIR%\.version"
if not defined OTTD_VERSION set OTTD_VERSION=15.2

set RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-win64
set ZIP_NAME=%RELEASE_NAME%.zip

echo.
echo ============================================================
echo  OpenTTD Archipelago STABLE Build + Package
echo  AP Version  : v%AP_VERSION%
echo  OpenTTD     : %OTTD_VERSION%
echo  Source      : %PROJECT_DIR%
echo  Output      : %DIST_DIR%\%ZIP_NAME%
echo ============================================================
echo.

REM -- Find Visual Studio ----------------------------------------
set VCVARS=
for %%E in (Community Professional Enterprise) do (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
)
if not defined VCVARS (
    echo [ERROR] Visual Studio 2022 not found!
    pause & exit /b 1
)

REM -- STEP 1: Patch game name + version tag to stable -----------
REM IMPORTANT: We back up files BEFORE patching, and restore them after.
REM This ensures uncommitted changes are NOT lost.
echo [1/7] Patching game name and version tag ...
REM Back up original files
copy /Y "%PROJECT_DIR%\src\archipelago_gui.cpp" "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" > nul
copy /Y "%PROJECT_DIR%\src\archipelago.h"       "%PROJECT_DIR%\src\archipelago.h.bak"       > nul
REM Patch OpenTTD-Exp -> OpenTTD
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago_gui.cpp') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago_gui.cpp'"
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago.h') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago.h'"
echo       OK: game name = OpenTTD (stable)
REM Write .ap_version so CMake uses stable version
cd /d "%PROJECT_DIR%"
echo v%AP_VERSION%> "%PROJECT_DIR%\.ap_version"
echo       OK: .ap_version = v%AP_VERSION% (FindVersion.cmake reads this)

REM -- STEP 2: Visual Studio -------------------------------------
echo [2/7] Activating Visual Studio build environment...
call "%VCVARS%" > nul 2>&1
echo       OK: %VCVARS%

REM -- STEP 3: vcpkg ---------------------------------------------
echo [3/7] Checking vcpkg...
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo       vcpkg.exe not found - bootstrapping...
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics > nul 2>&1
    if errorlevel 1 (
        echo [ERROR] vcpkg bootstrap failed!
        goto :revert_and_exit
    )
)
echo       Installing packages from vcpkg.json...
cd /d "%PROJECT_DIR%"
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows > nul 2>&1
echo       OK.

REM -- STEP 4: Clean build ---------------------------------------
echo [4/7] Preparing build folder...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
echo       OK: %BUILD_DIR%

REM -- STEP 5: CMake ---------------------------------------------
echo [5/7] Configuring CMake...
echo ============================================================
cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
    -DCMAKE_INSTALL_PREFIX="%BUILD_DIR%\install" ^
    -DOPTION_USE_ASSERTS=OFF
if errorlevel 1 (
    echo.
    echo [ERROR] CMake configuration failed!
    goto :revert_and_exit
)

REM -- STEP 6: Build ---------------------------------------------
echo.
echo [6/7] Building (RelWithDebInfo)...
echo ============================================================
cmake --build . --config RelWithDebInfo --parallel
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    goto :revert_and_exit
)

REM -- STEP 7: Package -------------------------------------------
echo.
echo [7/7] Packaging release...
echo ============================================================
if exist "%BUILD_DIR%\install" rmdir /s /q "%BUILD_DIR%\install"
cmake --install . --config RelWithDebInfo --prefix "%BUILD_DIR%\install"
if errorlevel 1 (
    echo [ERROR] cmake --install failed!
    goto :revert_and_exit
)

REM -- Find the installed openttd.exe ----------------------------
set INSTALL_BIN=%BUILD_DIR%\install
if exist "%BUILD_DIR%\install\bin\openttd.exe" (
    set INSTALL_BIN=%BUILD_DIR%\install\bin
)

REM -- Build final release folder --------------------------------
if exist "%DIST_DIR%\%RELEASE_NAME%" rmdir /s /q "%DIST_DIR%\%RELEASE_NAME%"
mkdir "%DIST_DIR%\%RELEASE_NAME%"
set OUT=%DIST_DIR%\%RELEASE_NAME%

REM Executable
copy /Y "%INSTALL_BIN%\openttd.exe" "%OUT%\openttd.exe" > nul

REM DLL files from vcpkg
for %%F in ("%BUILD_DIR%\RelWithDebInfo\*.dll") do copy /Y "%%F" "%OUT%\" > nul

REM baseset
if exist "%BUILD_DIR%\install\share\games\openttd\baseset" (
    xcopy /E /I /Q "%BUILD_DIR%\install\share\games\openttd\baseset" "%OUT%\baseset" > nul
) else if exist "%BUILD_DIR%\install\baseset" (
    xcopy /E /I /Q "%BUILD_DIR%\install\baseset" "%OUT%\baseset" > nul
) else (
    xcopy /E /I /Q "%PROJECT_DIR%\baseset" "%OUT%\baseset" > nul
)

REM lang
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

REM ai / game / scripts / docs
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

REM Root-level documents
for %%F in (README.md CONTRIBUTING.md COPYING.md CREDITS.md THIRD-PARTY-NOTICES.md) do (
    if exist "%PROJECT_DIR%\%%F" copy /Y "%PROJECT_DIR%\%%F" "%OUT%\%%F" > nul
)
copy /Y "%PROJECT_DIR%\CHANGELOG.md"  "%OUT%\CHANGELOG.md"  > nul
copy /Y "%PROJECT_DIR%\KNOWN_BUGS.md" "%OUT%\KNOWN_BUGS.md" > nul
copy /Y "%PROJECT_DIR%\INSTALL.md"    "%OUT%\INSTALL.md"    > nul

REM -- Server.bat launcher (multiplayer) -------------------------
if exist "%PROJECT_DIR%\Server.bat" (
    copy /Y "%PROJECT_DIR%\Server.bat" "%OUT%\Server.bat" > nul
    echo   [OK]      Server.bat
) else (
    echo   [WARNING] Server.bat not found
)

REM -- Archipelago APWorld (patch openttd_exp -> openttd for stable)
echo   Patching APWorld: openttd_exp -^> openttd ...
if not exist "%PROJECT_DIR%\apworld" goto :skip_apworld
mkdir "%OUT%\apworld"
mkdir "%OUT%\apworld\openttd"
xcopy /E /I /Q "%PROJECT_DIR%\apworld\openttd_exp" "%OUT%\apworld\openttd" > nul
if exist "%OUT%\apworld\openttd\__pycache__" rmdir /s /q "%OUT%\apworld\openttd\__pycache__"
REM Patch game name in archipelago.json AND __init__.py
powershell -NoProfile -Command "(Get-Content '%OUT%\apworld\openttd\archipelago.json') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%OUT%\apworld\openttd\archipelago.json'"
powershell -NoProfile -Command "(Get-Content '%OUT%\apworld\openttd\__init__.py') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%OUT%\apworld\openttd\__init__.py'"
REM Build openttd.apworld ZIP (with openttd/ as root folder inside ZIP)
REM IMPORTANT: Uses Python zipfile instead of PowerShell Compress-Archive!
REM Compress-Archive creates backslash paths in ZIP which breaks Linux.
REM Python zipfile uses forward slashes (correct ZIP standard).
if exist "%OUT%\apworld\openttd.apworld" del /f "%OUT%\apworld\openttd.apworld"
> "%TEMP%\ap_mkzip.py" (
    echo import zipfile, os
    echo src = r"%OUT%\apworld\openttd"
    echo dst = r"%OUT%\apworld\openttd.apworld"
    echo with zipfile.ZipFile^(dst, 'w', zipfile.ZIP_DEFLATED^) as z:
    echo     for root, dirs, files in os.walk^(src^):
    echo         for f in files:
    echo             full = os.path.join^(root, f^)
    echo             arc = 'openttd/' + os.path.relpath^(full, src^).replace^(os.sep, '/'^)
    echo             z.write^(full, arc^)
)
python "%TEMP%\ap_mkzip.py"
if errorlevel 1 (
    echo   [ERROR] Python zipfile failed - falling back to PowerShell
    powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\apworld\openttd' -DestinationPath '%OUT%\apworld\openttd.zip' -CompressionLevel Optimal"
    rename "%OUT%\apworld\openttd.zip" "openttd.apworld"
)
del /f "%TEMP%\ap_mkzip.py" 2>nul
echo   [OK]      apworld\openttd.apworld  (patched for stable)
echo   [OK]      apworld\openttd\          (patched source)
:skip_apworld

REM -- Bundled base assets from media\baseset\ -------------------
if exist "%PROJECT_DIR%\media\baseset\archipelago_icons.grf" (
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_icons.grf" "%OUT%\baseset\archipelago_icons.grf" > nul
    echo   [OK]      baseset\archipelago_icons.grf
) else (
    echo   [WARNING] media\baseset\archipelago_icons.grf not found
)

REM OpenGFX
set OPENGFX_TAR=%PROJECT_DIR%\media\baseset\opengfx-8.0.tar
if exist "%OPENGFX_TAR%" (
    echo   Extracting OpenGFX from local bundle...
    powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%OPENGFX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenGFX 8.0)
) else (
    echo   [WARNING] media\baseset\opengfx-8.0.tar not found - skipping.
)

REM OpenSFX
set OPENSFX_TAR=%PROJECT_DIR%\media\baseset\opensfx-1.0.3.tar
if exist "%OPENSFX_TAR%" (
    echo   Extracting OpenSFX from local bundle...
    powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%OPENSFX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenSFX 1.0.3)
) else (
    echo   [WARNING] media\baseset\opensfx-1.0.3.tar not found - skipping.
)

REM OpenMSX
set OPENMSX_TAR=%PROJECT_DIR%\media\baseset\openmsx-0.4.2.tar
if exist "%OPENMSX_TAR%" (
    echo   Extracting OpenMSX from local bundle...
    powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%OPENMSX_TAR%' --strip-components=1 -C '%OUT%\baseset'"
    echo   [OK]      baseset\ (OpenMSX 0.4.2)
) else (
    echo   [WARNING] media\baseset\openmsx-0.4.2.tar not found - skipping.
)

REM -- NewGRF ----------------------------------------------------
REM NAMED FILES ONLY. This was xcopy /E over the whole folder, which
REM shipped whatever sat there on the build machine - eight third-party
REM vehicle sets did, about 79 MB. Players install those themselves
REM through OpenTTD Check Online Content.
mkdir "%OUT%\newgrf" 2> nul
for %%G in (archipelago_ruins.grf archipelago_stars.grf) do (
    if exist "%PROJECT_DIR%\newgrf\%%G" (
        copy /Y "%PROJECT_DIR%\newgrf\%%G" "%OUT%\newgrf\%%G" > nul
        echo   [OK]      newgrf\%%G
    ) else (
        echo   [WARNING] newgrf\%%G not found
    )
)

REM archipelago_ruins.grf in newgrf\
if exist "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" (
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" "%OUT%\newgrf\archipelago_ruins.grf" > nul
    echo   [OK]      newgrf\archipelago_ruins.grf
) else (
    echo   [WARNING] media\baseset\archipelago_ruins.grf not found for newgrf\
)

REM -- SimpleAI to data\ai\ -------------------------------------
set SIMPLEAI_TAR=%DIST_DIR%\data_template\content_download\ai\534d504c-SimpleAI-14.tar
if exist "%SIMPLEAI_TAR%" (
    echo   Extracting SimpleAI to data\ai\...
    if not exist "%OUT%\data\ai" mkdir "%OUT%\data\ai"
    powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%SIMPLEAI_TAR%' -C '%OUT%\data\ai'"
    echo   [OK]      data\ai\SimpleAI-14\
) else (
    echo   [WARNING] SimpleAI-14.tar not found in data_template - skipping.
)

REM -- Data folder (portable config + content_download + AI libraries)
set DATA_TEMPLATE=%DIST_DIR%\data_template
if exist "%DATA_TEMPLATE%" (
    echo   Copying data folder from template...
    xcopy /E /I /Y /Q "%DATA_TEMPLATE%" "%OUT%\data" > nul
    echo   [OK]      data\ (configs, AI libraries, NightGFX, music)
) else (
    echo   [WARNING] dist\data_template\ not found - no data folder.
)

REM -- Fix resolution in openttd.cfg -----------------------------
if exist "%OUT%\data\openttd.cfg" (
    echo   Fixing resolution in openttd.cfg to 800x600 ...
    powershell -NoProfile -Command "(Get-Content '%OUT%\data\openttd.cfg') -replace 'resolution = \d+,\d+', 'resolution = 800,600' | Set-Content '%OUT%\data\openttd.cfg'"
    echo   [OK]      data\openttd.cfg resolution set to 800,600
)

REM -- Verify key files ------------------------------------------
echo.
echo Verifying output...
set MISSING=0
for %%F in (openttd.exe baseset\openttd.grf lang\english.lng) do (
    if not exist "%OUT%\%%F" (
        echo   [MISSING] %%F
        set MISSING=1
    ) else (
        echo   [OK]      %%F
    )
)
if exist "%OUT%\apworld\openttd.apworld" (
    echo   [OK]      apworld\openttd.apworld  (stable)
) else (
    echo   [WARNING] apworld\openttd.apworld missing!
)
if exist "%OUT%\apworld\openttd\archipelago.json" (
    echo   [OK]      apworld\openttd\          (stable source)
) else (
    echo   [WARNING] apworld\openttd\ source missing!
)
if exist "%OUT%\data\ai\SimpleAI-14\info.nut" (
    echo   [OK]      data\ai\SimpleAI-14\info.nut
) else (
    echo   [WARNING] data\ai\SimpleAI-14\ missing (demigod AI)
)
if exist "%OUT%\data\ai\library\pathfinder\road\library.nut" (
    echo   [OK]      data\ai\library\ (pathfinder.road, pathfinder.rail, graph.aystar, queue.binary_heap)
) else (
    echo   [WARNING] data\ai\library\ missing - SimpleAI cannot find pathfinder!
)
if exist "%OUT%\data\openttd.cfg" (
    echo   [OK]      data\openttd.cfg (portable config)
) else (
    echo   [WARNING] data\openttd.cfg missing
)
if exist "%OUT%\newgrf\archipelago_ruins.grf" (
    echo   [OK]      newgrf\archipelago_ruins.grf
) else (
    echo   [WARNING] archipelago_ruins.grf missing in newgrf\
)
if exist "%OUT%\Server.bat" (
    echo   [OK]      Server.bat (multiplayer launcher)
) else (
    echo   [WARNING] Server.bat missing
)
if "%MISSING%"=="1" (
    echo.
    echo [WARNING] Critical files missing - check cmake --install.
)

REM -- Zip with PowerShell ---------------------------------------
echo.
echo Creating %ZIP_NAME%...
if exist "%DIST_DIR%\%ZIP_NAME%" del /f "%DIST_DIR%\%ZIP_NAME%"
powershell -NoProfile -Command ^
    "Compress-Archive -Path '%DIST_DIR%\%RELEASE_NAME%' -DestinationPath '%DIST_DIR%\%ZIP_NAME%' -CompressionLevel Optimal"
if errorlevel 1 (
    echo [ERROR] ZIP creation failed!
    goto :revert_and_exit
)

REM ================================================================
REM -- LINUX PACKAGE -----------------------------------------------
REM Extracts the official Linux tarball and overlays all
REM Archipelago files (baseset, newgrf, apworld, data, etc.)
REM If GinjaNinja32's WSS binary is in Reference\ it is used
REM automatically instead of the vanilla binary.
REM ================================================================
echo.
echo ============================================================
echo  Linux package starting...
echo ============================================================

set LINUX_TAR=%PROJECT_DIR%\Reference\openttd-15.2-linux-generic-amd64.tar.xz
if not exist "%LINUX_TAR%" set LINUX_TAR=%PROJECT_DIR%\Reference\rod\openttd-15.2-linux-generic-amd64.tar.xz
set LINUX_RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-linux-amd64
set LINUX_ZIP_NAME=%LINUX_RELEASE_NAME%.tar.gz
set LOUT=%DIST_DIR%\%LINUX_RELEASE_NAME%

if not exist "%LINUX_TAR%" (
    echo [WARNING] Linux tarball not found in Reference\ or Reference\rod\
    echo           Skipping Linux package.
    goto :skip_linux
)

REM Clear old Linux release folder
if exist "%LOUT%" rmdir /s /q "%LOUT%"
mkdir "%LOUT%"

REM Extract official Linux tarball (strip top-level dir)
echo   Extracting Linux base from tarball...
powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%LINUX_TAR%' --strip-components=1 -C '%LOUT%'"
if errorlevel 1 (
    echo [ERROR] Could not extract Linux tarball!
    goto :skip_linux
)
echo   [OK]      Linux base extracted

REM -- Replace binary with GinjaNinja32's WSS build (if available)
REM Look for a custom binary in Reference folder.
REM Supports multiple possible names.
set LINUX_AP_BIN=
for %%N in (openttd-linux-ap openttd-linux-wss openttd-ap openttd) do (
    if exist "%PROJECT_DIR%\Reference\%%N" (
        if not defined LINUX_AP_BIN set "LINUX_AP_BIN=%PROJECT_DIR%\Reference\%%N"
    )
)
if defined LINUX_AP_BIN (
    echo   Replacing vanilla binary with AP WSS build...
    copy /Y "%LINUX_AP_BIN%" "%LOUT%\openttd" > nul
    echo   [OK]      openttd binary replaced from: %LINUX_AP_BIN%
) else (
    echo   [INFO]    No AP Linux binary found in Reference\ - using vanilla.
    echo             Place GinjaNinja32's binary as Reference\openttd-linux-ap
)

REM -- Overlay: baseset (same as Windows) ------------------------
REM Copies ON TOP of the existing baseset from the tarball
if exist "%OUT%\baseset" (
    xcopy /E /I /Y /Q "%OUT%\baseset" "%LOUT%\baseset" > nul
    echo   [OK]      baseset\ (overlay from Windows build)
)

REM -- Overlay: lang ---------------------------------------------
REM Only overlay Windows-built lang files if NO pre-built Linux binary was used.
REM Pre-built binaries have their own LANGUAGE_PACK_VERSION hash that must match
REM the .lng files; overlaying Windows lang files causes "No available language
REM packs (invalid versions?)" on Linux.
if not defined LINUX_AP_BIN (
    if exist "%OUT%\lang" (
        xcopy /E /I /Y /Q "%OUT%\lang" "%LOUT%\lang" > nul
        echo   [OK]      lang\ (from Windows build)
    )
) else (
    echo   [INFO]    Lang overlay skipped - pre-built binary uses its own lang files
    if not exist "%LOUT%\lang\english.lng" (
        echo   [WARNING] Linux lang/ missing english.lng - rebuild Linux binary!
    )
)

REM -- newgrf ----------------------------------------------------
if exist "%PROJECT_DIR%\newgrf" (
    xcopy /E /I /Q "%PROJECT_DIR%\newgrf" "%LOUT%\newgrf" > nul
    echo   [OK]      newgrf\
)
if exist "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" (
    if not exist "%LOUT%\newgrf" mkdir "%LOUT%\newgrf"
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_ruins.grf" "%LOUT%\newgrf\archipelago_ruins.grf" > nul
    echo   [OK]      newgrf\archipelago_ruins.grf
)
if exist "%PROJECT_DIR%\media\baseset\archipelago_stars.grf" (
    if not exist "%LOUT%\newgrf" mkdir "%LOUT%\newgrf"
    copy /Y "%PROJECT_DIR%\media\baseset\archipelago_stars.grf" "%LOUT%\newgrf\archipelago_stars.grf" > nul
    echo   [OK]      newgrf\archipelago_stars.grf
)

REM -- apworld (reuses the patched stable version from Windows) --
if exist "%OUT%\apworld" (
    xcopy /E /I /Q "%OUT%\apworld" "%LOUT%\apworld" > nul
    echo   [OK]      apworld\ (stable, patched)
)

REM -- data folder (portable config, AI libraries, SimpleAI) -----
if exist "%DATA_TEMPLATE%" (
    xcopy /E /I /Y /Q "%DATA_TEMPLATE%" "%LOUT%\data" > nul
    echo   [OK]      data\ (configs, AI libraries)
)
if exist "%SIMPLEAI_TAR%" (
    if not exist "%LOUT%\data\ai" mkdir "%LOUT%\data\ai"
    powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -xf '%SIMPLEAI_TAR%' -C '%LOUT%\data\ai'"
    echo   [OK]      data\ai\SimpleAI-14\
)

REM -- Fix resolution in Linux config ----------------------------
if exist "%LOUT%\data\openttd.cfg" (
    powershell -NoProfile -Command "(Get-Content '%LOUT%\data\openttd.cfg') -replace 'resolution = \d+,\d+', 'resolution = 800,600' | Set-Content '%LOUT%\data\openttd.cfg'"
    echo   [OK]      data\openttd.cfg resolution set to 800,600
)

REM -- Documents -------------------------------------------------
for %%F in (README.md CHANGELOG.md COPYING.md CREDITS.md THIRD-PARTY-NOTICES.md) do (
    if exist "%PROJECT_DIR%\%%F" copy /Y "%PROJECT_DIR%\%%F" "%LOUT%\%%F" > nul
)

REM -- Server launcher for Linux (Unix line endings via Python) ----
if not exist "%LOUT%\server.sh" (
    python -c "import sys;open(sys.argv[1],'wb').write(b'#!/bin/bash\nDIR=$( cd $( dirname $0 ) && pwd )\nexport LD_LIBRARY_PATH=$DIR/lib:$LD_LIBRARY_PATH\nexec $DIR/openttd -D -d net=2\n')" "%LOUT%\server.sh"
    if not exist "%LOUT%\server.sh" (
        echo   [WARNING] server.sh not created by Python - creating basic version
        echo #!/bin/bash> "%LOUT%\server.sh"
        echo exec ./openttd -D -d net=2>> "%LOUT%\server.sh"
    )
    echo   [OK]      server.sh
)

REM -- Verify Linux output ---------------------------------------
echo.
echo Verifying Linux output...
for %%F in (openttd baseset\openttd.grf lang\english.lng) do (
    if exist "%LOUT%\%%F" (
        echo   [OK]      %%F
    ) else (
        echo   [MISSING] %%F
    )
)
if exist "%LOUT%\apworld\openttd.apworld" (
    echo   [OK]      apworld\openttd.apworld
) else (
    echo   [WARNING] apworld\ missing
)
if exist "%LOUT%\newgrf\archipelago_ruins.grf" (
    echo   [OK]      newgrf\archipelago_ruins.grf
) else (
    echo   [WARNING] archipelago_ruins.grf missing
)
REM -- Pack Linux as .tar.gz directly ----------------------------
echo.
echo Creating %LINUX_ZIP_NAME%...
if exist "%DIST_DIR%\%LINUX_ZIP_NAME%" del /f "%DIST_DIR%\%LINUX_ZIP_NAME%"
cd /d "%DIST_DIR%"
powershell -NoProfile -Command "& 'C:\Windows\System32\tar.exe' -czf '%LINUX_ZIP_NAME%' '%LINUX_RELEASE_NAME%'"
if exist "%DIST_DIR%\%LINUX_ZIP_NAME%" (
    echo   [OK]      %LINUX_ZIP_NAME%
) else (
    echo   [WARNING] Linux tar.gz was not created.
)

:skip_linux

REM -- Revert game name + tag back to experimental ---------------
REM Restores backup files (preserves all uncommitted changes).
echo.
echo Reverting source files from backup and removing git tag ...
cd /d "%PROJECT_DIR%"
move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
echo       OK: game name reverted to OpenTTD-Exp
if exist "%PROJECT_DIR%\.ap_version" del /f "%PROJECT_DIR%\.ap_version"
echo       OK: .ap_version removed

echo.
echo ============================================================
echo  STABLE BUILD SUCCESSFUL!
echo.
echo  Version : v%AP_VERSION%
echo  Windows : %DIST_DIR%\%ZIP_NAME%
echo  Linux   : %DIST_DIR%\%LINUX_ZIP_NAME%
echo.
echo  Next step: Run stable_release.bat to push
echo  to GitHub and create release.
echo ============================================================
echo.
pause
exit /b 0

REM -- Error handler: revert game name and exit ------------------
:revert_and_exit
echo.
echo Reverting source files from backup after error...
cd /d "%PROJECT_DIR%"
if exist "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
if exist "%PROJECT_DIR%\src\archipelago.h.bak"       move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
if exist "%PROJECT_DIR%\.ap_version" del /f "%PROJECT_DIR%\.ap_version"
echo       OK: game name and .ap_version reverted to OpenTTD-Exp
pause
exit /b 1

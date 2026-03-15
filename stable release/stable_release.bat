@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago — STABLE Release to GitHub
::
::  This script:
::    1. Temporarily switches git remote to the STABLE repo
::    2. Patches game name OpenTTD-Exp → OpenTTD
::    3. Stages all relevant files
::    4. Commits + tags with stable version
::    5. Pushes to stable repo
::    6. Creates GitHub Release and uploads the ZIP
::    7. Switches remote back to EXP repo
::    8. Reverts game name back to OpenTTD-Exp
::
::  Prerequisites:
::    - Run stable_build_and_package.bat FIRST to create the ZIP
::    - gh CLI must be installed (https://cli.github.com/)
::    - Git must be authenticated for both repos
:: ============================================================

:: ── CONFIG ──────────────────────────────────────────────────
set AP_VERSION=1.2.1
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\OpenTTD 15.2 with Archipelago-exp
set DIST_DIR=%PROJECT_DIR%\dist
set RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-win64
set ZIP_PATH=%DIST_DIR%\%RELEASE_NAME%.zip

set STABLE_REPO=https://github.com/solida1987/openttd-archipelago.git
set EXP_REPO=https://github.com/solida1987/openttd-archipelago-exp.git
set TAG=v%AP_VERSION%

echo.
echo ============================================================
echo  OpenTTD Archipelago — STABLE RELEASE
echo  Version : v%AP_VERSION%
echo  Tag     : %TAG%
echo  ZIP     : %ZIP_PATH%
echo  Target  : github.com/solida1987/openttd-archipelago
echo ============================================================
echo.

:: ── Preflight checks ────────────────────────────────────────
cd /d "%PROJECT_DIR%"

git --version > nul 2>&1
if errorlevel 1 (
    echo [FEJL] git ikke fundet i PATH!
    pause & exit /b 1
)

gh --version > nul 2>&1
if errorlevel 1 (
    echo [FEJL] gh CLI ikke fundet! Installer fra https://cli.github.com/
    pause & exit /b 1
)

if not exist "%ZIP_PATH%" (
    echo [FEJL] ZIP ikke fundet: %ZIP_PATH%
    echo        Koer stable_build_and_package.bat foerst!
    pause & exit /b 1
)

echo [OK] Preflight checks bestaaet.
echo.

:: ── Bekraeft med bruger ─────────────────────────────────────
echo ============================================================
echo  ADVARSEL: Dette vil:
echo    - Pushe til STABLE repo (openttd-archipelago)
echo    - Oprette tag %TAG%
echo    - Oprette GitHub Release med ZIP upload
echo ============================================================
echo.
set /p CONFIRM="Fortsaet? (ja/nej): "
if /i not "%CONFIRM%"=="ja" (
    echo Afbrudt.
    pause & exit /b 0
)
echo.

:: ── STEP 1: Switch remote til stable ────────────────────────
echo [1/8] Skifter remote til stable repo...
git remote set-url origin %STABLE_REPO%
echo       Remote: %STABLE_REPO%

:: ── STEP 2: Patch game name ─────────────────────────────────
echo [2/8] Patcher game name: OpenTTD-Exp  -^>  OpenTTD ...
:: Gem backup af originale filer
copy /Y "%PROJECT_DIR%\src\archipelago_gui.cpp" "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" > nul
copy /Y "%PROJECT_DIR%\src\archipelago.h"       "%PROJECT_DIR%\src\archipelago.h.bak"       > nul
copy /Y "%PROJECT_DIR%\README.md"               "%PROJECT_DIR%\README.md.bak"               > nul
:: Patch
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago_gui.cpp') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago_gui.cpp'"
powershell -NoProfile -Command ^
  "(Get-Content '%PROJECT_DIR%\src\archipelago.h') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content '%PROJECT_DIR%\src\archipelago.h'"
echo       OK: game name = OpenTTD

:: ── STEP 2b: Use stable README ──────────────────────────────
echo       Kopierer stable README.md over exp README...
copy /Y "%PROJECT_DIR%\stable release\README.md" "%PROJECT_DIR%\README.md" > nul
echo       OK: README.md = stable version

:: ── STEP 3: Stage all relevant files ────────────────────────
echo [3/8] Stager filer...

:: APWorld (Python + metadata)
git add apworld\

:: GameScript
git add gamescript\

:: Core Archipelago source
git add src\archipelago.cpp
git add src\archipelago.h
git add src\archipelago_gui.cpp
git add src\archipelago_gui.h
git add src\archipelago_manager.cpp

:: Modified game source files
git add src\cargo_type.h
git add src\console_cmds.cpp
git add src\economy.cpp
git add src\engine.cpp
git add src\engine_func.h
git add src\fileio.cpp
git add src\gfx.cpp
git add src\gfxinit.cpp
git add src\industry_gui.cpp
git add src\intro_gui.cpp
git add src\object_cmd.cpp
git add src\os\windows\win32.cpp
git add src\rail_cmd.cpp
git add src\rail_gui.cpp
git add src\road_cmd.cpp
git add src\road_gui.cpp
git add src\settings_type.h
git add src\settingentry_gui.cpp
git add src\station_cmd.cpp
git add src\terraform_cmd.cpp
git add src\terraform_gui.cpp
git add src\toolbar_gui.cpp
git add src\town_cmd.cpp
git add src\tree_cmd.cpp
git add src\tree_gui.cpp
git add src\tunnelbridge_cmd.cpp
git add src\vehicle_cmd.cpp
git add src\window_type.h
git add src\aircraft_cmd.cpp
git add src\train_cmd.cpp
git add src\roadveh_cmd.cpp
git add src\openttd.cpp
git add src\company_cmd.cpp
git add src\company_gui.cpp
git add src\genworld.cpp
git add src\strings.cpp
git add src\statusbar_gui.cpp
git add src\statusbar_gui.h

:: Network / Bridge (multiplayer)
git add src\network\network_bridge.cpp
git add src\network\network_bridge_client.cpp
git add src\network\network_bridge_client.h
git add src\network\network_bridge.h
git add src\network\core\tcp_game.cpp
git add src\network\core\config.h
git add src\network\network_client.cpp
git add src\network\network.cpp
git add src\network\network_server.cpp
git add src\network\CMakeLists.txt
git add src\video\dedicated_v.cpp

:: Tables / data
git add src\table\cargo_const.h
git add src\table\sprites.h
git add src\table\settings\game_settings.ini

:: Widgets
git add src\widgets\toolbar_widget.h
git add src\widgets\intro_widget.h
git add src\widgets\statusbar_widget.h

:: Saveload
git add src\saveload\archipelago_sl.cpp
git add src\saveload\saveload.cpp
git add src\saveload\CMakeLists.txt

:: Language
git add src\lang\english.txt

:: Music (OGG support)
git add src\music\midifile.cpp
if exist src\music\ogg_music.cpp git add src\music\ogg_music.cpp
if exist src\music\ogg_music.h   git add src\music\ogg_music.h

:: Script API
git add src\script\api\script_controller.cpp
git add src\script\squirrel.cpp

:: Build system
git add src\CMakeLists.txt
git add CMakeLists.txt
git add cmake\InstallAndPackage.cmake

:: Assets
git add baseset\archipelago_icons.grf
git add newgrf\iron_horse.grf
git add newgrf\archipelago_ruins.grf
git add media\baseset\CMakeLists.txt
git add media\baseset\archipelago_icons.grf
git add media\baseset\archipelago_ruins.grf
git add media\baseset\archipelago_ruins\

:: Documentation
git add CHANGELOG.md
git add KNOWN_BUGS.md
git add INSTALL.md
git add README.md
git add docs\yaml_options.md
git add wiki_page.txt

:: Gitignore
git add .gitignore

:: Fjern ting der IKKE skal med i stable
git rm -r --cached build\ > nul 2>&1
git rm -r --cached dist\ > nul 2>&1
git rm -r --cached .claude\ > nul 2>&1
git rm -r --cached backup\ > nul 2>&1
git rm -r --cached "stable release\" > nul 2>&1
git rm -r --cached Reference\ > nul 2>&1
git rm -r --cached vcpkg_installed\ > nul 2>&1
git rm -r --cached src\3rdparty\stb_vorbis\ > nul 2>&1
git rm --cached exp_build_and_package.bat > nul 2>&1
git rm --cached exp_build_incremental.bat > nul 2>&1
git rm --cached exp_build_openttd.bat > nul 2>&1
git rm --cached exp_git_push_release.bat > nul 2>&1
git rm --cached temp_build.bat > nul 2>&1
git rm --cached build_inc.bat > nul 2>&1
git rm --cached build_test.bat > nul 2>&1
git rm --cached LaunchBridge.bat > nul 2>&1
git rm --cached Server.bat > nul 2>&1
git rm --cached wiki_page.txt > nul 2>&1
git rm --cached FEATURE_BACKLOG.md > nul 2>&1
:: NewGRF binaries that are NOT source (only keep iron_horse + archipelago_ruins)
git rm --cached newgrf\Aircraft2025.grf > nul 2>&1
git rm --cached newgrf\firs.grf > nul 2>&1
git rm --cached newgrf\heqs.grf > nul 2>&1
git rm --cached newgrf\hoverv.grf > nul 2>&1
git rm --cached newgrf\military-items.grf > nul 2>&1
git rm --cached newgrf\shark.grf > nul 2>&1
git rm --cached newgrf\vactrain_1.0.1.grf > nul 2>&1
echo       OK.

:: ── STEP 4: Commit ──────────────────────────────────────────
echo [4/8] Committer...
git commit -m "v%AP_VERSION%: Fix victory sphere logic, improve foreign item placement, fix music baseset"
if errorlevel 1 (
    echo       Intet nyt at committe - fortsaetter til push.
)

:: ── STEP 5: Push to stable repo ─────────────────────────────
echo [5/8] Pusher til stable repo...
git push origin HEAD --force
if errorlevel 1 (
    echo.
    echo [FEJL] Push fejlede! Tjek din GitHub-forbindelse.
    goto :revert_all
)
echo       OK.

:: ── STEP 6: Tag ─────────────────────────────────────────────
echo [6/8] Opretter release-tag %TAG%...
git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1
git tag -a %TAG% -m "OpenTTD Archipelago v%AP_VERSION% — Stable Release"
git push origin %TAG%
if errorlevel 1 (
    echo [FEJL] Tag-push fejlede!
    goto :revert_all
)
echo       OK.

:: ── STEP 7: Create GitHub Release + upload ZIP ──────────────
echo [7/8] Opretter GitHub Release og uploader ZIP...
gh release create %TAG% "%ZIP_PATH%" ^
    --repo solida1987/openttd-archipelago ^
    --title "OpenTTD Archipelago v%AP_VERSION%" ^
    --notes "## OpenTTD Archipelago v%AP_VERSION% (Patch)

**Bugfix release** built from OpenTTD 15.2.

### Fixes
- **Victory sphere logic**: Victory no longer appears reachable in sphere 2 of spoiler logs. Now requires 15+ vehicles, cargo capability, and full infrastructure (bridges/tunnels, terraform, rail/road directions, signals) when sphere progression is enabled.
- **Foreign item placement**: Other players' progression items now land in early, reachable locations (Easy/Medium missions, first 50 shop slots) instead of being randomly scattered. Late shop slots (51+) are excluded from receiving foreign progression items.
- **Music baseset**: Rebuilt openmsx-0.4.2.tar from working files to fix potential MIDI playback issues on some systems.

### Installation
1. Extract the ZIP to any folder
2. Copy \`apworld/openttd/\` to your Archipelago \`custom_worlds\` directory
3. Run \`openttd.exe\`

See [CHANGELOG.md](CHANGELOG.md) for full details." ^
    --latest
if errorlevel 1 (
    echo [ADVARSEL] gh release create fejlede - opret manuelt paa GitHub.
    echo            ZIP: %ZIP_PATH%
    echo            Tag: %TAG%
) else (
    echo       OK: Release oprettet med ZIP upload.
)

:: ── STEP 8: Switch remote back to EXP + revert game name ───
echo [8/8] Skifter remote tilbage til EXP repo...
git remote set-url origin %EXP_REPO%
echo       Remote: %EXP_REPO%

echo Reverter source-filer og README fra backup ...
cd /d "%PROJECT_DIR%"
move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
move /Y "%PROJECT_DIR%\README.md.bak"               "%PROJECT_DIR%\README.md"               > nul
echo       OK: game name revertet til OpenTTD-Exp
echo       OK: README.md revertet til exp version

echo.
echo ============================================================
echo  STABLE RELEASE GENNEMFOERT!
echo.
echo  Version  : v%AP_VERSION%
echo  Tag      : %TAG%
echo  Release  : https://github.com/solida1987/openttd-archipelago/releases/tag/%TAG%
echo  Remote   : Sat tilbage til EXP repo
echo  Game name: Revertet til OpenTTD-Exp
echo.
echo  Du kan nu fortsaette med at arbejde i experimental.
echo ============================================================
echo.
pause
exit /b 0

:: ── Fejl-handler ────────────────────────────────────────────
:revert_all
echo.
echo Reverter efter fejl...
git remote set-url origin %EXP_REPO%
echo       Remote: %EXP_REPO%
cd /d "%PROJECT_DIR%"
if exist "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
if exist "%PROJECT_DIR%\src\archipelago.h.bak"       move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
if exist "%PROJECT_DIR%\README.md.bak"               move /Y "%PROJECT_DIR%\README.md.bak"               "%PROJECT_DIR%\README.md"               > nul
echo       OK: Alt revertet (remote, game name, README).
pause
exit /b 1

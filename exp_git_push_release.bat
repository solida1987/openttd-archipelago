@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago EXP — GitHub Push + Release Script
::  Pusher til: github.com/solida1987/openttd-archipelago-exp
:: ============================================================
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\OpenTTD 15.2 with Archipelago-exp
set AP_VERSION=exp-1.2.3

echo.
echo ============================================================
echo  GitHub Push + Release — EXPERIMENTAL
echo  Version: %AP_VERSION%
echo  Remote : github.com/solida1987/openttd-archipelago-exp
echo ============================================================
echo.

cd /d "%PROJECT_DIR%"

:: ── Tjek git er tilgængeligt ─────────────────────────────────
git --version > nul 2>&1
if errorlevel 1 (
    echo [FEJL] git ikke fundet i PATH!
    echo Installer Git fra https://git-scm.com/
    pause & exit /b 1
)

:: ── Sørg for remote peger på EXP repo ───────────────────────
git remote set-url origin https://github.com/solida1987/openttd-archipelago-exp.git

:: ── Vis status inden commit ──────────────────────────────────
echo [1/5] Nuværende git status:
echo ============================================================
git status --short
echo.

:: ── Stage alle relevante filer ───────────────────────────────
echo [2/5] Stager ændringer...

:: APWorld (Python + metadata)
git add apworld\

:: Bat scripts
git add exp_build_and_package.bat
git add exp_build_openttd.bat
git add exp_git_push_release.bat

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
git add src\window.cpp
git add src\industry_cmd.cpp
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

:: Network / Bridge (multiplayer foundation)
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

:: Bridge (Python multiplayer server)
git add bridge\

:: Documentation
git add CHANGELOG.md
git add changelog.md
git add KNOWN_BUGS.md
git add INSTALL.md
git add README.md
git add docs\yaml_options.md

:: Gitignore
git add .gitignore

:: Fjern deleted filer fra index
git add -u CODINGSTYLE.md > nul 2>&1
git add -u known-bugs.md > nul 2>&1

:: Fjern ting der IKKE skal med i exp repo
git rm -r --cached build\ > nul 2>&1
git rm -r --cached dist\ > nul 2>&1
git rm -r --cached .claude\ > nul 2>&1
git rm -r --cached Reference\ > nul 2>&1
git rm -r --cached backup\ > nul 2>&1
git rm -r --cached vcpkg_installed\ > nul 2>&1
git rm -r --cached "stable release\" > nul 2>&1
git rm -r --cached src\3rdparty\stb_vorbis\ > nul 2>&1
git rm --cached build_inc.bat > nul 2>&1
git rm --cached build_test.bat > nul 2>&1
git rm --cached temp_build.bat > nul 2>&1
git rm --cached wiki_page.txt > nul 2>&1
:: NewGRF binaries (bundled, not source — distributed in ZIP only)
git rm --cached newgrf\Aircraft2025.grf > nul 2>&1
git rm --cached newgrf\firs.grf > nul 2>&1
git rm --cached newgrf\heqs.grf > nul 2>&1
git rm --cached newgrf\hoverv.grf > nul 2>&1
git rm --cached newgrf\military-items.grf > nul 2>&1
git rm --cached newgrf\shark.grf > nul 2>&1
git rm --cached newgrf\vactrain_1.0.1.grf > nul 2>&1
echo       OK.

:: ── Commit ───────────────────────────────────────────────────
echo [3/5] Committer...
git commit -m "exp-1.2.3: Bugfixes, industry protection, balanced item distribution, Colby reopen"
if errorlevel 1 (
    echo       Intet nyt at committe - fortsætter til push.
)

:: ── Push til GitHub ──────────────────────────────────────────
echo [4/5] Pusher til GitHub (exp)...
git pull --no-rebase origin main
git push origin HEAD
if errorlevel 1 (
    echo.
    echo [FEJL] Push fejlede! Tjek din GitHub-forbindelse.
    pause & exit /b 1
)
echo       OK.

:: ── Tag og push release ──────────────────────────────────────
echo [5/5] Opretter release-tag...
set TAG=%AP_VERSION%

git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1

git tag -a %TAG% -m "OpenTTD Archipelago %AP_VERSION% — Experimental branch"
git push origin %TAG%
if errorlevel 1 (
    echo [FEJL] Tag-push fejlede!
    pause & exit /b 1
)

echo.
echo ============================================================
echo  PUSH SUCCESFULD!
echo.
echo  Tag    : %TAG%
echo  Remote : github.com/solida1987/openttd-archipelago-exp
echo ============================================================
echo.
pause

@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago — GitHub Push + Release Script
::  Beta 9
:: ============================================================
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\openttd-15.2
set /p OTTD_VERSION=<"%PROJECT_DIR%\.version"
if not defined OTTD_VERSION set OTTD_VERSION=15.2

echo.
echo ============================================================
echo  GitHub Push + Release
echo  Version: %OTTD_VERSION% — BETA 9
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

:: ── Vis status inden commit ──────────────────────────────────
echo [1/5] Nuværende git status:
echo ============================================================
git status --short
echo.

:: ── Stage alle relevante filer ───────────────────────────────
echo [2/5] Stager ændringer...

:: APWorld
git add apworld\

:: Build scripts
git add git_push_release.bat
git add build_and_package.bat
git add build_openttd.bat
git add Build-OpenTTD-AP.ps1

:: GameScript (Money Quests)
git add gamescript\

:: Kerne AP-filer
git add src\archipelago.cpp
git add src\archipelago.h
git add src\archipelago_gui.cpp
git add src\archipelago_gui.h
git add src\archipelago_manager.cpp

:: Console (ny fil: console_cmds.cpp med AP-kommando registrering)
git add src\console_cmds.cpp

:: GUI og engine
git add src\intro_gui.cpp
git add src\toolbar_gui.cpp
git add src\gfx.cpp
git add src\gfxinit.cpp
git add src\settingentry_gui.cpp
git add src\engine.cpp
git add src\engine_func.h
git add src\economy.cpp
git add src\aircraft_cmd.cpp
git add src\train_cmd.cpp
git add src\roadveh_cmd.cpp

:: Settings og window types
git add src\settings_type.h
git add src\window_type.h
git add src\table\settings\game_settings.ini

:: Build
git add src\CMakeLists.txt
git add src\widgets\toolbar_widget.h
git add src\widgets\intro_widget.h

:: Saveload
git add src\saveload\archipelago_sl.cpp
git add src\saveload\saveload.cpp
git add src\saveload\CMakeLists.txt

:: Sprog
git add src\lang\english.txt

:: Assets
git add baseset\archipelago_icons.grf
git add baseset\ap_intro_bg.png
git add newgrf\iron_horse.grf

:: Docs
git add CHANGELOG.md
git add KNOWN_BUGS.md
git add INSTALL.md

:: Slet build og dist fra tracking hvis de er staged ved en fejl
git rm -r --cached build\ > nul 2>&1
git rm -r --cached dist\ > nul 2>&1
echo       OK.

:: ── Commit ───────────────────────────────────────────────────
echo [3/5] Committer...
git commit -m "beta9: Mission pool system, cargo access rules, guide window, console AP commands, money quests gamescript, station rating fixes, fast-forward cap"
if errorlevel 1 (
    echo       Intet nyt at committe - fortsætter til push.
)

:: ── Push til GitHub ──────────────────────────────────────────
echo [4/5] Pusher til GitHub...
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
set TAG=v%OTTD_VERSION%-beta9

:: Slet eksisterende tag lokalt og remote hvis det findes
git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1

git tag -a %TAG% -m "OpenTTD %OTTD_VERSION% Archipelago beta9 — Mission pool system, cargo access rules, guide window, console AP commands, money quests gamescript, station rating fixes, fast-forward cap"
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
echo  Branch : pushed til GitHub
echo.
echo  VIGTIGT: Kør nu build_and_package.bat for at bygge beta9.
echo  Versionen viser nu korrekt "beta9" i titellinjen.
echo.
echo  Gaa til GitHub og opret et Release fra tagget %TAG%
echo  og upload dist\openttd-%OTTD_VERSION%-archipelago-windows-win64.zip
echo ============================================================
echo.
pause

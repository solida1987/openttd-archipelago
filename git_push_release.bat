@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago — GitHub Push + Release Script
::  Beta 6
:: ============================================================
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\openttd-15.2
set /p OTTD_VERSION=<"%PROJECT_DIR%\.version"
if not defined OTTD_VERSION set OTTD_VERSION=15.2
echo.
echo ============================================================
echo  GitHub Push + Release
echo  Version: %OTTD_VERSION% — BETA 6
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
git add apworld\
git add git_push_release.bat
git add src\archipelago.cpp
git add src\archipelago.h
git add src\archipelago_gui.cpp
git add src\archipelago_gui.h
git add src\archipelago_manager.cpp
git add src\intro_gui.cpp
git add src\saveload\archipelago_sl.cpp
git add src\saveload\saveload.cpp
git add src\saveload\CMakeLists.txt
git add src\lang\english.txt
git add newgrf\iron_horse.grf
git add changelog.md
git add known-bugs.md
git add build_and_package.bat
:: Slet build og dist fra tracking hvis de er staged ved en fejl
git rm -r --cached build\ > nul 2>&1
git rm -r --cached dist\ > nul 2>&1
echo       OK.
:: ── Commit ───────────────────────────────────────────────────
echo [3/5] Committer...
git commit -m "beta6: Iron Horse support, UI scale fix, currency fix, shop_slots YAML fix, starter fallback, window drag fix, engine map fixes"
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
set TAG=v%OTTD_VERSION%-beta6
:: Slet eksisterende tag lokalt og remote hvis det findes
git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1
git tag -a %TAG% -m "OpenTTD %OTTD_VERSION% Archipelago beta6 - Iron Horse, UI scale fix, currency fix, shop_slots YAML, starter fallback, window drag"
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
echo  Gaa til GitHub og opret et Release fra tagget %TAG%
echo  og upload dist\openttd-%OTTD_VERSION%-archipelago-windows-win64.zip
echo ============================================================
echo.
pause

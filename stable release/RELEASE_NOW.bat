@echo off
REM ============================================================
REM  OpenTTD Archipelago -- ONE-CLICK STABLE RELEASE
REM
REM  Runs both steps in sequence:
REM    1. stable_build_and_package.bat  (build + package zip)
REM    2. stable_release.bat            (push + GitHub release)
REM
REM  Press Ctrl+C to abort between steps.
REM ============================================================

echo.
echo ============================================================
echo   ONE-CLICK STABLE RELEASE
echo   Step 1: Build + Package
echo   Step 2: Push + GitHub Release
echo ============================================================
echo.

set SCRIPT_DIR=%~dp0

REM -- Step 1: Build + Package -----------------------------------
echo *** STEP 1: Building and packaging stable release... ***
echo.
call "%SCRIPT_DIR%stable_build_and_package.bat"
if errorlevel 1 (
    echo.
    echo [ERROR] Build + Package failed! Release aborted.
    pause & exit /b 1
)

REM -- Step 2: Push + Release ------------------------------------
echo.
echo *** STEP 2: Pushing to GitHub and creating release... ***
echo.
call "%SCRIPT_DIR%stable_release.bat"
if errorlevel 1 (
    echo.
    echo [ERROR] Release failed!
    pause & exit /b 1
)

echo.
echo ============================================================
echo  ALL DONE! Stable release is live.
echo ============================================================
echo.
pause

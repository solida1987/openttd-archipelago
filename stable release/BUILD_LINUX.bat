@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  OpenTTD Archipelago — BUILD LINUX RELEASE (via GitHub Actions)
::
::  Trigger en Linux-build i skyen fra din Windows-maskine.
::  Kræver: gh CLI (https://cli.github.com/) + gh auth login
:: ============================================================

set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\OpenTTD 15.2 with Archipelago-exp
set DIST_DIR=%PROJECT_DIR%\dist

echo.
echo ============================================================
echo  OpenTTD Archipelago — Linux Build (GitHub Actions)
echo ============================================================
echo.

:: ── Tjek gh CLI ──────────────────────────────────────────────
where gh >nul 2>&1
if errorlevel 1 (
    echo [FEJL] gh CLI ikke fundet!
    echo        Installer fra: https://cli.github.com/
    echo        Kør derefter:  gh auth login
    pause & exit /b 1
)

:: ── Tjek auth ────────────────────────────────────────────────
gh auth status >nul 2>&1
if errorlevel 1 (
    echo [FEJL] Du er ikke logget ind i gh CLI!
    echo        Kør:  gh auth login
    pause & exit /b 1
)
echo [OK] gh CLI fundet og autentificeret.
echo.

:: ── Push seneste ændringer så workflow har den nyeste kode ───
echo [1/4] Pusher seneste kode til GitHub...
cd /d "%PROJECT_DIR%"
git push origin 2>nul
if errorlevel 1 (
    echo [ADVARSEL] git push fejlede — workflowet bruger hvad der allerede er pushet.
) else (
    echo       OK: Kode pushet.
)
echo.

:: ── Trigger workflow ─────────────────────────────────────────
echo [2/4] Trigger Linux build workflow...
gh workflow run build-linux-release.yml
if errorlevel 1 (
    echo [FEJL] Kunne ikke trigger workflow!
    echo        Sørg for at .github/workflows/build-linux-release.yml er pushet til GitHub.
    pause & exit /b 1
)
echo       OK: Workflow triggered!
echo.

:: ── Vent på at build starter ─────────────────────────────────
echo [3/4] Venter paa at build starter og faerdiggøres...
echo       (Dette tager typisk 3-8 minutter)
echo.

:: Vent lidt så GitHub når at registrere workflow run
timeout /t 10 /nobreak > nul

:: Find det seneste run ID
for /f "tokens=1" %%i in ('gh run list --workflow=build-linux-release.yml --limit 1 --json databaseId --jq ".[0].databaseId"') do set RUN_ID=%%i

if not defined RUN_ID (
    echo [FEJL] Kunne ikke finde workflow run ID.
    echo        Tjek GitHub Actions: https://github.com/solida1987/openttd-archipelago-exp/actions
    pause & exit /b 1
)

echo       Run ID: %RUN_ID%
echo       Følger build-log (Ctrl+C for at afbryde)...
echo.
gh run watch %RUN_ID%

:: Tjek resultat
gh run view %RUN_ID% --json conclusion --jq ".conclusion" > "%TEMP%\gh_result.txt"
set /p BUILD_RESULT=<"%TEMP%\gh_result.txt"
del "%TEMP%\gh_result.txt"

if not "%BUILD_RESULT%"=="success" (
    echo.
    echo [FEJL] Linux build fejlede! Status: %BUILD_RESULT%
    echo        Se logs: gh run view %RUN_ID% --log
    pause & exit /b 1
)

echo.
echo       BUILD SUCCEEDED!
echo.

:: ── Download artifact ────────────────────────────────────────
echo [4/4] Downloader Linux release...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
cd /d "%DIST_DIR%"

:: Slet evt. gammel download
if exist "linux-release" rmdir /s /q "linux-release"

gh run download %RUN_ID% --name linux-release --dir "%DIST_DIR%\linux-release"
if errorlevel 1 (
    echo [FEJL] Download af artifact fejlede!
    echo        Prøv manuelt: gh run download %RUN_ID% --name linux-release
    pause & exit /b 1
)

:: Flyt tar.gz ud af undermappen
for %%F in ("%DIST_DIR%\linux-release\*.tar.gz") do (
    move /Y "%%F" "%DIST_DIR%\" > nul
    echo       Hentet: %%~nxF
)
rmdir /s /q "%DIST_DIR%\linux-release" 2>nul

echo.
echo ============================================================
echo  LINUX BUILD KLAR!
echo.
echo  Filen ligger i: %DIST_DIR%\
echo.
echo  Linux-brugere udpakker med:
echo    tar xzf openttd-archipelago-v*-linux-x64.tar.gz
echo    cd openttd-archipelago-v*-linux-x64
echo    chmod +x openttd start.sh
echo    ./start.sh
echo ============================================================
echo.
pause
exit /b 0

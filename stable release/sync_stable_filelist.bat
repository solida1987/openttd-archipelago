@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  SYNC STABLE FILE LIST
::
::  Scanner ALLE modificerede/nye filer i git og viser dig
::  hvilke der MANGLER i stable_release.bat.
::  Tilfoej dem automatisk med eet tryk.
:: ============================================================

set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 with Archipelago-exp
set RELEASE_BAT=%PROJECT_DIR%\stable release\stable_release.bat

cd /d "%PROJECT_DIR%"

echo.
echo ============================================================
echo  STABLE RELEASE FILE SYNC
echo  Scanner for manglende filer...
echo ============================================================
echo.

:: ── Hent alle modificerede filer fra git ──────────────────────
:: Baade staged, unstaged og untracked (men kun relevante)
set TMPFILE=%TEMP%\ap_missing_files.txt
if exist "%TMPFILE%" del /f "%TMPFILE%"

:: Alle ændrede filer (tracked)
for /f "delims=" %%F in ('git diff --name-only HEAD 2^>nul') do (
    call :check_file "%%F"
)

:: Alle untracked filer i src/ apworld/ cmake/ .github/
for /f "delims=" %%F in ('git ls-files --others --exclude-standard -- src/ apworld/ cmake/ .github/ media/ newgrf/ baseset/ gamescript/ 2^>nul') do (
    call :check_file "%%F"
)

:: ── Vis resultat ──────────────────────────────────────────────
if not exist "%TMPFILE%" (
    echo [OK] Alle modificerede filer er allerede i stable_release.bat!
    echo      Intet at goere.
    echo.
    pause
    exit /b 0
)

echo MANGLENDE FILER i stable_release.bat:
echo ────────────────────────────────────────
set COUNT=0
for /f "delims=" %%L in (%TMPFILE%) do (
    echo   %%L
    set /a COUNT+=1
)
echo ────────────────────────────────────────
echo %COUNT% fil(er) mangler.
echo.

:: ── Spoerg bruger ─────────────────────────────────────────────
set /p DOIT="Tilfoej dem alle til stable_release.bat? (ja/nej): "
if /i not "%DOIT%"=="ja" (
    echo Afbrudt. Du kan tilfoeje dem manuelt.
    pause
    exit /b 0
)

:: ── Tilfoej manglende filer ───────────────────────────────────
:: Indsaet lige foer ":: Fjern ting der IKKE skal med i stable"
set TMPBAT=%TEMP%\ap_insert_lines.txt
if exist "%TMPBAT%" del /f "%TMPBAT%"

echo.>> "%TMPBAT%"
echo :: ── Auto-tilfoejede filer (%date%) ──────────────────>> "%TMPBAT%"
for /f "delims=" %%L in (%TMPFILE%) do (
    :: Konverter forward slash til backslash for git add
    set "FPATH=%%L"
    set "FPATH=!FPATH:/=\!"
    echo git add !FPATH!>> "%TMPBAT%"
)

:: Find linjenummer for "Fjern ting der IKKE skal med"
set INSERT_BEFORE=0
set LINENUM=0
for /f "delims=" %%A in ('findstr /n "Fjern ting der IKKE" "%RELEASE_BAT%"') do (
    for /f "tokens=1 delims=:" %%N in ("%%A") do set INSERT_BEFORE=%%N
)

if %INSERT_BEFORE%==0 (
    echo [FEJL] Kunne ikke finde indsaetningspunktet i stable_release.bat!
    echo        Tilfoej filerne manuelt.
    type "%TMPBAT%"
    pause
    exit /b 1
)

:: Byg ny version af filen med indsat blok
set TMPOUT=%TEMP%\ap_release_new.bat
if exist "%TMPOUT%" del /f "%TMPOUT%"

set LINENUM=0
for /f "usebackq delims=" %%A in ("%RELEASE_BAT%") do (
    set /a LINENUM+=1
    if !LINENUM!==%INSERT_BEFORE% (
        :: Indsaet vores nye linjer foer denne linje
        type "%TMPBAT%" >> "%TMPOUT%"
        echo.>> "%TMPOUT%"
    )
    echo %%A>> "%TMPOUT%"
)

:: Overskriv original
copy /Y "%TMPOUT%" "%RELEASE_BAT%" > nul

echo.
echo [OK] %COUNT% fil(er) tilfoejet til stable_release.bat!
echo.
echo Tilfoejede:
type "%TMPBAT%"
echo.

:: Ryd op
del /f "%TMPFILE%" 2>nul
del /f "%TMPBAT%" 2>nul
del /f "%TMPOUT%" 2>nul

pause
exit /b 0

:: ════════════════════════════════════════════════════════════════
::  SUBROUTINE: check_file
::  Tjekker om en fil allerede er i stable_release.bat
:: ════════════════════════════════════════════════════════════════
:check_file
set "FILEPATH=%~1"

:: Skip filer vi ALDRIG vil have i stable
echo "%FILEPATH%" | findstr /i "build/ dist/ backup/ Reference/ vcpkg_installed/ __pycache__ .bak stable.release/ exp.release/ exp_build temp_build build_inc build_test LaunchBridge Server.bat FEATURE_BACKLOG .nmlcache" > nul 2>&1
if not errorlevel 1 exit /b

:: Skip newgrf binaries (undtagen iron_horse + archipelago_ruins)
echo "%FILEPATH%" | findstr /i "newgrf/" > nul 2>&1
if not errorlevel 1 (
    echo "%FILEPATH%" | findstr /i "iron_horse archipelago_ruins" > nul 2>&1
    if errorlevel 1 exit /b
)

:: Skip .apworld binaries
echo "%FILEPATH%" | findstr /i ".apworld" > nul 2>&1
if not errorlevel 1 exit /b

:: Tjek om filen allerede er i stable_release.bat
:: Konverter / til \ for soegning
set "SEARCHPATH=%FILEPATH:/=\%"
findstr /i /c:"%SEARCHPATH%" "%RELEASE_BAT%" > nul 2>&1
if errorlevel 1 (
    :: Proev ogsaa med forward slash
    findstr /i /c:"%FILEPATH%" "%RELEASE_BAT%" > nul 2>&1
    if errorlevel 1 (
        :: Tjek om parent directory allerede er staged (f.eks. apworld\ dækker alt)
        for %%P in ("%FILEPATH%") do set "PARENT=%%~pF"
        set "PARENT=!PARENT:/=\!"
        :: Simpel check: er "apworld\" eller "gamescript\" allerede staged?
        echo "%FILEPATH%" | findstr /i "^apworld/ ^gamescript/" > nul 2>&1
        if not errorlevel 1 (
            :: Parent directory er staged med wildcard — skip
            exit /b
        )
        :: Filen mangler — tilfoej til listen
        echo %FILEPATH%>> "%TMPFILE%"
    )
)
exit /b

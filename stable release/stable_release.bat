@echo off
setlocal EnableDelayedExpansion
REM ============================================================
REM  OpenTTD Archipelago -- STABLE Release to GitHub
REM
REM  This script:
REM    1. Points git remote at the public repo
REM    3. Stages all relevant files
REM    4. Commits + tags with stable version
REM    5. Pushes to stable repo
REM    6. Creates GitHub Release and uploads ZIP + tar.gz
REM
REM  Prerequisites:
REM    - Run stable_build_and_package.bat FIRST to create the ZIP
REM    - gh CLI must be installed (https://cli.github.com/)
REM    - Git must be authenticated for both repos
REM ============================================================

REM -- CONFIG ----------------------------------------------------
set AP_VERSION=2.1.0
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 Archipelago
set DIST_DIR=%PROJECT_DIR%\dist
set RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-win64
set ZIP_PATH=%DIST_DIR%\%RELEASE_NAME%.zip
set LINUX_RELEASE_NAME=openttd-archipelago-v%AP_VERSION%-linux-amd64
set LINUX_ZIP_PATH=%DIST_DIR%\%LINUX_RELEASE_NAME%.tar.gz
set STABLE_REPO=https://github.com/solida1987/openttd-archipelago.git
set TAG=v%AP_VERSION%

echo.
echo ============================================================
echo  OpenTTD Archipelago -- STABLE RELEASE
echo  Version : v%AP_VERSION%
echo  Tag     : %TAG%
echo  Windows : %ZIP_PATH%
if exist "%LINUX_ZIP_PATH%" (
    echo  Linux   : %LINUX_ZIP_PATH%
) else (
    echo  Linux   : [not built - skipping]
)
echo  Target  : github.com/solida1987/openttd-archipelago
echo ============================================================
echo.

REM -- Preflight checks ------------------------------------------
cd /d "%PROJECT_DIR%"
git --version > nul 2>&1
if errorlevel 1 (
    echo [ERROR] git not found in PATH
    pause & exit /b 1
)
gh --version > nul 2>&1
if errorlevel 1 (
    echo [ERROR] gh CLI not found - install from cli.github.com
    pause & exit /b 1
)
if not exist "%ZIP_PATH%" (
    echo [ERROR] ZIP not found: %ZIP_PATH%
    echo         Run stable_build_and_package.bat first
    pause & exit /b 1
)
echo [OK] Preflight checks passed.
if exist "%LINUX_ZIP_PATH%" (
    echo [OK] Linux tar.gz found - will upload with Windows ZIP.
) else (
    echo [INFO] No Linux tar.gz - only Windows ZIP will be uploaded.
)
echo.

REM -- Confirm with user -----------------------------------------
echo ============================================================
echo  WARNING: This will:
echo    - Push to STABLE repo (openttd-archipelago)
echo    - Create tag %TAG%
echo    - Create GitHub Release with file upload
echo ============================================================
echo.
set /p CONFIRM="Continue? (yes/no): "
if /i not "%CONFIRM%"=="yes" (
    if /i not "%CONFIRM%"=="ja" (
        echo Aborted.
        pause & exit /b 0
    )
)
echo.

REM -- STEP 1: Switch remote to stable --------------------------
echo [1/8] Switching remote to stable repo...
git remote set-url origin %STABLE_REPO%
echo       Remote: %STABLE_REPO%

REM -- STEP 2: Patch game name -----------------------------------
REM Back up original files
copy /Y "%PROJECT_DIR%\src\archipelago_gui.cpp" "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" > nul
copy /Y "%PROJECT_DIR%\src\archipelago.h"       "%PROJECT_DIR%\src\archipelago.h.bak"       > nul
copy /Y "%PROJECT_DIR%\README.md"               "%PROJECT_DIR%\README.md.bak"               > nul
REM Patch
echo       OK: game name = OpenTTD

REM -- STEP 2b: Use stable README --------------------------------
echo       Copying stable README.md over exp README...
if exist "%PROJECT_DIR%\stable release\README.md" (
    copy /Y "%PROJECT_DIR%\stable release\README.md" "%PROJECT_DIR%\README.md" > nul
    echo       OK: README.md = stable version
) else (
    echo       [INFO] No stable README found, using current README
)

REM -- STEP 3: Stage all relevant files --------------------------
echo [3/8] Staging files...

REM Stage ALL project source and assets (broad approach)
git add src\
git add apworld\
git add baseset\
git add media\baseset\
git add newgrf\iron_horse.grf
git add newgrf\archipelago_ruins.grf
if exist newgrf\archipelago_stars.grf git add newgrf\archipelago_stars.grf
git add cmake\
git add CMakeLists.txt
git add .gitignore
git add README.md
git add CHANGELOG.md
if exist changelog.md git add changelog.md
git add KNOWN_BUGS.md
git add INSTALL.md
git add COPYING.md
git add CREDITS.md
git add CONTRIBUTING.md
if exist docs\yaml_options.md git add docs\yaml_options.md
if exist .github\workflows\build-linux-release.yml git add .github\workflows\build-linux-release.yml
if exist gamescript\ git add gamescript\
if exist bridge\ git add bridge\

REM Remove things that should NOT be in stable
git rm -r --cached build\ > nul 2>&1
git rm -r --cached dist\ > nul 2>&1
git rm -r --cached backup\ > nul 2>&1
git rm -r --cached "stable release\" > nul 2>&1
git rm -r --cached "exp release\" > nul 2>&1
git rm -r --cached Reference\ > nul 2>&1
git rm -r --cached vcpkg_installed\ > nul 2>&1
git rm -r --cached docs\devnotes\ > nul 2>&1
git rm -r --cached docs\gamedocs\ > nul 2>&1
git rm -r --cached docs\source\ > nul 2>&1
git rm --cached exp_build_and_package.bat > nul 2>&1
git rm --cached exp_build_incremental.bat > nul 2>&1
git rm --cached exp_build_openttd.bat > nul 2>&1
git rm --cached exp_git_push_release.bat > nul 2>&1
git rm --cached temp_build.bat > nul 2>&1
git rm --cached temp_build2.bat > nul 2>&1
git rm --cached build_inc.bat > nul 2>&1
git rm --cached build_test.bat > nul 2>&1
git rm --cached LaunchBridge.bat > nul 2>&1
git rm --cached Server.bat > nul 2>&1
git rm --cached wiki_page.txt > nul 2>&1
git rm --cached FEATURE_BACKLOG.md > nul 2>&1
git rm --cached CHECK_IDEAS.md > nul 2>&1
git rm --cached prompt.txt > nul 2>&1
git rm --cached item_pool_setop.xlsx > nul 2>&1
git rm --cached item_pool.txt > nul 2>&1
git rm --cached ih_wagons_out.txt > nul 2>&1
git rm --cached populate_excel.py > nul 2>&1
git rm --cached update_fillsheet.py > nul 2>&1
git rm --cached xlsx_out.txt > nul 2>&1
REM Remove temp scripts
for %%F in (temp_build.bat temp_build2.bat temp_compare.py temp_compare2.py temp_fix_all.py temp_ih_names.py temp_ih_railtype.py temp_ih_wagons.py temp_read_xlsx.py temp_run.bat temp_run2.bat temp_update_xlsx.py temp_update_xlsx2.py temp_build_xlsx.py) do (
    git rm --cached %%F > nul 2>&1
)
REM Third-party NewGRFs are gone from newgrf and the packer copies
REM named files only, so there is nothing left to hide from git.
REM Remove .apworld binaries from apworld folder (only source gets committed)
git rm --cached apworld\openttd.apworld > nul 2>&1
git rm --cached apworld\openttd_exp.apworld > nul 2>&1
git rm --cached apworld\xcopy_exclude.txt > nul 2>&1
echo       OK: All files staged.

REM -- STEP 4: Commit --------------------------------------------
echo [4/8] Committing...
git commit -m "v%AP_VERSION%: Fix bridge unlock button, star checks, star counter direction"
if errorlevel 1 (
    echo       Nothing new to commit - continuing to push.
)

REM -- STEP 5: Push to stable repo --------------------------------
echo [5/8] Pushing to stable repo...
git push origin HEAD --force
if errorlevel 1 (
    echo.
    echo [ERROR] Push failed - check your GitHub connection.
    goto :revert_all
)
echo       OK.

REM -- STEP 6: Tag ------------------------------------------------
echo [6/8] Creating release tag %TAG%...
git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1
git tag -a %TAG% -m "OpenTTD Archipelago v%AP_VERSION% -- Stable Release"
git push origin %TAG%
if errorlevel 1 (
    echo [ERROR] Tag push failed
    goto :revert_all
)
echo       OK.

REM -- STEP 7: Create GitHub Release + upload files ----------------
echo [7/8] Creating GitHub Release and uploading files...

REM Build upload command - always include Windows ZIP
set "GH_CMD=gh release create %TAG% "%ZIP_PATH%""

REM Add Linux tar.gz if it exists
if exist "%LINUX_ZIP_PATH%" (
    set "GH_CMD=!GH_CMD! "%LINUX_ZIP_PATH%""
)

REM Write release notes to temp file to avoid quoting issues
> "%TEMP%\ap_release_notes.md" (
    echo ## OpenTTD Archipelago v%AP_VERSION%
    echo.
    echo **Stable release** built from OpenTTD 15.2.
    echo.
    echo ### Downloads
    echo - **Windows**: `openttd-archipelago-v%AP_VERSION%-win64.zip`
    echo - **Linux**: `openttd-archipelago-v%AP_VERSION%-linux-amd64.tar.gz`
    echo.
    echo ### Installation
    echo 1. Extract the archive to any folder
    echo 2. Copy `apworld/openttd/` to your Archipelago `custom_worlds` directory
    echo 3. Run `openttd.exe` ^(Windows^) or `./openttd` ^(Linux^)
    echo.
    echo See [CHANGELOG.md] for full details.
)

%GH_CMD% --repo solida1987/openttd-archipelago --title "OpenTTD Archipelago v%AP_VERSION%" --notes-file "%TEMP%\ap_release_notes.md" --latest
if errorlevel 1 (
    echo [WARNING] gh release create failed - create manually on GitHub.
    echo           Windows ZIP: %ZIP_PATH%
    if exist "%LINUX_ZIP_PATH%" echo           Linux tar.gz: %LINUX_ZIP_PATH%
    echo           Tag: %TAG%
) else (
    echo       OK: Release created with file upload.
)
del /f "%TEMP%\ap_release_notes.md" 2>nul

REM -- STEP 8: Switch remote back to EXP + revert game name -------
echo [8/8] Switching remote back to EXP repo...
git remote set-url origin %EXP_REPO%
echo       Remote: %EXP_REPO%
echo Reverting source files and README from backup ...
cd /d "%PROJECT_DIR%"
move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
move /Y "%PROJECT_DIR%\README.md.bak"               "%PROJECT_DIR%\README.md"               > nul
echo       OK: README.md reverted to exp version

echo.
echo ============================================================
echo  STABLE RELEASE COMPLETE
echo.
echo  Version  : v%AP_VERSION%
echo  Tag      : %TAG%
echo  Windows  : %RELEASE_NAME%.zip
if exist "%LINUX_ZIP_PATH%" (
    echo  Linux    : %LINUX_RELEASE_NAME%.tar.gz
)
echo  Release  : https://github.com/solida1987/openttd-archipelago/releases/tag/%TAG%
echo  Remote   : Set back to EXP repo
echo.
echo  You can now continue working in experimental.
echo ============================================================
echo.
pause
exit /b 0

REM -- Error handler ---------------------------------------------
:revert_all
echo.
echo Reverting after error...
git remote set-url origin %EXP_REPO%
echo       Remote: %EXP_REPO%
cd /d "%PROJECT_DIR%"
if exist "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" move /Y "%PROJECT_DIR%\src\archipelago_gui.cpp.bak" "%PROJECT_DIR%\src\archipelago_gui.cpp" > nul
if exist "%PROJECT_DIR%\src\archipelago.h.bak"       move /Y "%PROJECT_DIR%\src\archipelago.h.bak"       "%PROJECT_DIR%\src\archipelago.h"       > nul
if exist "%PROJECT_DIR%\README.md.bak"               move /Y "%PROJECT_DIR%\README.md.bak"               "%PROJECT_DIR%\README.md"               > nul
echo       OK: Everything reverted (remote, game name, README).
pause
exit /b 1

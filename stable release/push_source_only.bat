@echo off
setlocal EnableDelayedExpansion
REM ============================================================
REM  Push source code to STABLE repo (openttd-archipelago)
REM  No build, no release, no zip -- just source push.
REM ============================================================

set AP_VERSION=1.4.0
set PROJECT_DIR=C:\Users\marco\OneDrive\Desktop\AP Spil og Launcher\OpenTTD 15.2 with Archipelago-exp
set STABLE_REPO=https://github.com/solida1987/openttd-archipelago.git
set EXP_REPO=https://github.com/solida1987/openttd-archipelago-exp.git
set TAG=v%AP_VERSION%

cd /d "%PROJECT_DIR%"

echo.
echo ============================================================
echo  Push source to stable repo
echo  Version: v%AP_VERSION%
echo ============================================================
echo.

REM -- Backup files before patching --------------------------------
copy /Y src\archipelago_gui.cpp src\archipelago_gui.cpp.bak > nul
copy /Y src\archipelago.h src\archipelago.h.bak > nul
copy /Y README.md README.md.bak > nul

REM -- Patch game name OpenTTD-Exp -> OpenTTD ----------------------
powershell -NoProfile -Command "(Get-Content 'src\archipelago_gui.cpp') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content 'src\archipelago_gui.cpp'"
powershell -NoProfile -Command "(Get-Content 'src\archipelago.h') -replace 'OpenTTD-Exp', 'OpenTTD' | Set-Content 'src\archipelago.h'"
echo [OK] Patched game name to OpenTTD

REM -- Use stable README ------------------------------------------
if exist "stable release\README.md" (
    copy /Y "stable release\README.md" README.md > nul
    echo [OK] Stable README copied
)

REM -- Switch remote to stable -------------------------------------
git remote set-url origin %STABLE_REPO%
echo [OK] Remote set to stable

REM -- Stage everything relevant -----------------------------------
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

REM -- Remove things that should NOT be in stable ------------------
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
for %%F in (temp_build.bat temp_build2.bat temp_compare.py temp_compare2.py temp_fix_all.py temp_ih_names.py temp_ih_railtype.py temp_ih_wagons.py temp_read_xlsx.py temp_run.bat temp_run2.bat temp_update_xlsx.py temp_update_xlsx2.py temp_build_xlsx.py) do (
    git rm --cached %%F > nul 2>&1
)
git rm --cached newgrf\Aircraft2025.grf > nul 2>&1
git rm --cached newgrf\firs.grf > nul 2>&1
git rm --cached newgrf\heqs.grf > nul 2>&1
git rm --cached newgrf\hoverv.grf > nul 2>&1
git rm --cached newgrf\military-items.grf > nul 2>&1
git rm --cached newgrf\shark.grf > nul 2>&1
git rm --cached newgrf\vactrain_1.0.1.grf > nul 2>&1
git rm --cached apworld\openttd.apworld > nul 2>&1
git rm --cached apworld\openttd_exp.apworld > nul 2>&1
git rm --cached apworld\xcopy_exclude.txt > nul 2>&1
echo [OK] Files staged

REM -- Commit ------------------------------------------------------
git commit -m "v%AP_VERSION%: Stars, dynamic pools, improved ruins/demigod"
echo [OK] Committed

REM -- Push ---------------------------------------------------------
git push origin HEAD --force
if errorlevel 1 (
    echo [ERROR] Push failed!
    goto :revert
)
echo [OK] Pushed to stable

REM -- Tag ----------------------------------------------------------
git tag -d %TAG% > nul 2>&1
git push origin :refs/tags/%TAG% > nul 2>&1
git tag -a %TAG% -m "OpenTTD Archipelago v%AP_VERSION%"
git push origin %TAG%
echo [OK] Tag %TAG% pushed

REM -- Revert everything back to exp --------------------------------
:revert
git remote set-url origin %EXP_REPO%
echo [OK] Remote back to exp
move /Y src\archipelago_gui.cpp.bak src\archipelago_gui.cpp > nul
move /Y src\archipelago.h.bak src\archipelago.h > nul
move /Y README.md.bak README.md > nul
echo [OK] Source reverted to OpenTTD-Exp

echo.
echo Done.
pause

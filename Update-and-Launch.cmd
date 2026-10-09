@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Do not update a running CMD file in place. Execute a stable temporary worker.
if /I "%~1"=="--worker" goto :worker
set "COREMODEL_ROOT=%~dp0"
set "COREMODEL_WORKER=%TEMP%\CoreModel-Update-%RANDOM%-%RANDOM%.cmd"
copy /Y "%~f0" "%COREMODEL_WORKER%" >nul
if errorlevel 1 goto :worker_copy_failed
call "%COREMODEL_WORKER%" --worker
set "COREMODEL_RESULT=%ERRORLEVEL%"
del /Q "%COREMODEL_WORKER%" >nul 2>&1
exit /b %COREMODEL_RESULT%

:worker_copy_failed
echo ERROR: Unable to create temporary launcher.
pause
exit /b 1

:worker
if not defined COREMODEL_ROOT goto :missing_root
cd /d "%COREMODEL_ROOT%"
if errorlevel 1 goto :missing_root
title CoreModel - Update Build Launch
rem The splash is cosmetic: failures still appear in this console.
set "COREMODEL_SPLASH_SIGNAL=%TEMP%\CoreModel-Splash-%RANDOM%-%RANDOM%.done"
set "COREMODEL_SPLASH_PROGRESS=%COREMODEL_SPLASH_SIGNAL%.progress"
call :progress 5 Checking project tools...
if exist "CoreModel-Splash.ps1" (
  start "" powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "%CD%\CoreModel-Splash.ps1" -SignalFile "%COREMODEL_SPLASH_SIGNAL%" -ProgressFile "%COREMODEL_SPLASH_PROGRESS%"
)
echo.
echo === CoreModel automatic updater ===
echo Project: %CD%
echo.

where git.exe >nul 2>&1
if errorlevel 1 goto :missing_git
where cmake.exe >nul 2>&1
if errorlevel 1 goto :missing_cmake

if not exist ".git" goto :not_git
call :progress 15 Checking local changes...
echo [1/4] Checking local source changes...
for /f "delims=" %%A in ('git status --porcelain --untracked-files=no 2^>nul') do goto :dirty

call :progress 30 Updating from GitHub...
echo [2/4] Updating from GitHub...
git pull --ff-only origin main
if errorlevel 1 goto :failed

rem Skip compilation when the previously verified build matches this source revision.
for /f %%H in ('git rev-parse HEAD 2^>nul') do set "COREMODEL_REVISION=%%H"
if exist "build\Release\CoreModel.exe" if exist "build\Release\CoreModel-built-revision.txt" (
  set /p COREM_MODEL_BUILT=<"build\Release\CoreModel-built-revision.txt"
  call :check_build_revision
  if not errorlevel 1 goto :launch
)
call :progress 50 Configuring build...
echo [3/4] Configuring build...
if exist "build\CMakeCache.txt" (
  cmake -S . -B build
) else (
  cmake -S . -B build -G "Visual Studio 18 2026" -A x64
)
if errorlevel 1 goto :failed

call :progress 65 Building CoreModel...
echo [4/4] Building CoreModel.exe...
rem Preserve the last available executable before rebuilding; never overwrite this backup automatically.
if exist "build\Release\CoreModel.exe" if not exist "KnownGood\CoreModel.exe" (
  if not exist "KnownGood" mkdir "KnownGood"
  copy /Y "build\Release\CoreModel.exe" "KnownGood\CoreModel.exe" >nul
  if errorlevel 1 echo WARNING: Could not save KnownGood backup.
)
rem Capture the actual build output so we only retry a known corrupt-library error.
cmake --build build --config Release --parallel 2 > "CoreModel-build.log" 2>&1
set "BUILD_RESULT=%ERRORLEVEL%"
type "CoreModel-build.log"
if "%BUILD_RESULT%"=="0" goto :build_ok
call :progress 95 Finalizing build...
findstr /C:"LNK1136" "CoreModel-build.log" >nul 2>&1
if errorlevel 1 goto :failed
echo.
echo [Recovery] LNK1136 detected: rebuilding all generated libraries once...
cmake --build build --config Release --clean-first --parallel 2 > "CoreModel-build-recovery.log" 2>&1
set "RECOVERY_RESULT=%ERRORLEVEL%"
type "CoreModel-build-recovery.log"
if not "%RECOVERY_RESULT%"=="0" goto :failed
:build_ok
if not exist "build\Release\CoreModel.exe" goto :missing_exe
>"build\Release\CoreModel-built-revision.txt" echo %COREMODEL_REVISION%
goto :launch

:check_build_revision
rem The helper runs in a fresh parse context so variables read above are expanded correctly.
if "%COREM_MODEL_BUILT%"=="%COREMODEL_REVISION%" exit /b 0
exit /b 1

:launch
call :progress 100 Launching CoreModel...
call :progress
if not defined COREMODEL_SPLASH_PROGRESS exit /b 0
set "COREMODEL_PROGRESS_VALUE=%~1"
shift
>"%COREMODEL_SPLASH_PROGRESS%" echo %COREMODEL_PROGRESS_VALUE%^|%*
exit /b 0

:close_splash
echo.
echo CoreModel is up to date; launching editor.

echo.
echo Build successful. Launching CoreModel.exe directly.
echo This launcher does not open Visual Studio or run a solution file.
echo Source revision:
git rev-parse --short HEAD
for %%F in ("build\Release\CoreModel.exe") do echo Executable: %%~fF  ^(%%~zF bytes^)
echo Previous executable backup: KnownGood\CoreModel.exe
start "" /D "%CD%" "%CD%\build\Release\CoreModel.exe"
if errorlevel 1 goto :failed
exit /b 0

:close_splash
if defined COREMODEL_SPLASH_SIGNAL (
  >"%COREMODEL_SPLASH_SIGNAL%" echo done
)
exit /b 0

:missing_root
echo ERROR: Could not locate the CoreModel project folder.
goto :failed
:dirty
echo ERROR: Local tracked source changes prevent GitHub update.
echo No old editor will be launched.
echo Run: git status --short
echo Back up your changes before using git restore or git stash.
goto :failed
:missing_git
echo ERROR: git.exe was not found on PATH.
goto :failed
:missing_cmake
echo ERROR: cmake.exe was not found on PATH.
goto :failed
:not_git
echo ERROR: This folder is not the GitHub checkout.
goto :failed
:missing_exe
echo ERROR: Build completed but CoreModel.exe was not found.
:failed
call :close_splash
echo.
echo CoreModel update/build failed. Nothing will be launched.
pause
exit /b 1

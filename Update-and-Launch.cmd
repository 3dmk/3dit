@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem N3DLite update/build/launch. Run from a temporary copy to permit git pull.
if /I "%~1"=="--worker" goto :worker
set "N3D_ROOT=%~dp0"
set "N3D_WORKER=%TEMP%\N3DLite-Update-%RANDOM%-%RANDOM%.cmd"
copy /Y "%~f0" "%N3D_WORKER%" >nul
if errorlevel 1 (
 echo ERROR: Cannot create temporary launcher.
 pause
 exit /b 1
)
call "%N3D_WORKER%" --worker
set "N3D_RESULT=%ERRORLEVEL%"
del /Q "%N3D_WORKER%" >nul 2>&1
exit /b %N3D_RESULT%

:worker
if not defined N3D_ROOT goto :failed
cd /d "%N3D_ROOT%"
if errorlevel 1 goto :failed
title N3DLite - Update and Launch
set "N3D_SIGNAL=%TEMP%\N3DLite-Splash-%RANDOM%-%RANDOM%.done"
set "N3D_PROGRESS=%N3D_SIGNAL%.progress"
call :progress 5 "Checking project tools..."
if exist "N3DLite-Splash.ps1" (
 start "" powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "%CD%\N3DLite-Splash.ps1" -SignalFile "%N3D_SIGNAL%" -ProgressFile "%N3D_PROGRESS%"
 rem Minimize the updater console after the splash process is started.
 rem The console is restored automatically if an update/build fails.
 call :minimize_console
)
echo === N3DLite Update and Launch ===
echo Project: %CD%
where git.exe >nul 2>&1
if errorlevel 1 (
 echo ERROR: Git not found.
 goto :failed
)
where cmake.exe >nul 2>&1
if errorlevel 1 (
 echo ERROR: CMake not found.
 goto :failed
)
if not exist ".git" (
 echo ERROR: Not a Git checkout.
 goto :failed
)
call :progress 15 "Checking local changes..."
for /f "delims=" %%A in ('git status --porcelain --untracked-files=no 2^>nul') do goto :dirty
call :progress 30 "Updating from GitHub..."
git pull --ff-only origin main
if errorlevel 1 goto :failed
for /f %%H in ('git rev-parse HEAD 2^>nul') do set "N3D_REVISION=%%H"
if not defined N3D_REVISION goto :failed
if exist "build\Release\N3DLite.exe" if exist "build\Release\N3DLite-built-revision.txt" (
 set /p N3D_BUILT=<"build\Release\N3DLite-built-revision.txt"
 call :check_revision
 if not errorlevel 1 goto :launch
)
call :progress 50 "Configuring build..."
if exist "build\CMakeCache.txt" (
 cmake -S . -B build
) else (
 cmake -S . -B build -G "Visual Studio 18 2026" -A x64
)
if errorlevel 1 goto :failed
call :progress 65 "Building N3DLite..."
if exist "build\Release\N3DLite.exe" if not exist "KnownGood\N3DLite.exe" (
 if not exist "KnownGood" mkdir "KnownGood"
 copy /Y "build\Release\N3DLite.exe" "KnownGood\N3DLite.exe" >nul
)
cmake --build build --config Release --parallel 2 >"N3DLite-build.log" 2>&1
if not errorlevel 1 goto :built
type "N3DLite-build.log"
findstr /C:"LNK1136" "N3DLite-build.log" >nul 2>&1
if errorlevel 1 goto :failed
echo Retrying after LNK1136 with clean build...
cmake --build build --config Release --clean-first --parallel 2 >"N3DLite-build-recovery.log" 2>&1
if errorlevel 1 (
 type "N3DLite-build-recovery.log"
 goto :failed
)
:built
call :progress 95 "Finalizing build..."
if not exist "build\Release\N3DLite.exe" goto :failed
>"build\Release\N3DLite-built-revision.txt" echo %N3D_REVISION%
:launch
call :progress 100 "Launching N3DLite..."
echo Starting N3DLite...
start "" /D "%CD%" "%CD%\build\Release\N3DLite.exe"
if errorlevel 1 goto :failed
call :close_splash
exit /b 0

:check_revision
if "%N3D_BUILT%"=="%N3D_REVISION%" exit /b 0
exit /b 1

:progress
if not defined N3D_PROGRESS exit /b 0
>"%N3D_PROGRESS%" echo %~1^|%~2
exit /b 0

:close_splash
if defined N3D_SIGNAL >"%N3D_SIGNAL%" echo done
exit /b 0

:minimize_console
powershell.exe -NoProfile -NonInteractive -Command "Add-Type -Namespace N3D -Name Win32 -MemberDefinition '[DllImport(\"kernel32.dll\")] public static extern System.IntPtr GetConsoleWindow(); [DllImport(\"user32.dll\")] public static extern bool ShowWindow(System.IntPtr hWnd, int nCmdShow);' -ErrorAction SilentlyContinue; $h=[N3D.Win32]::GetConsoleWindow(); if($h -ne [IntPtr]::Zero){[N3D.Win32]::ShowWindow($h,6) | Out-Null}" >nul 2>&1
exit /b 0

:restore_console
powershell.exe -NoProfile -NonInteractive -Command "Add-Type -Namespace N3D -Name Win32 -MemberDefinition '[DllImport(\"kernel32.dll\")] public static extern System.IntPtr GetConsoleWindow(); [DllImport(\"user32.dll\")] public static extern bool ShowWindow(System.IntPtr hWnd, int nCmdShow);' -ErrorAction SilentlyContinue; $h=[N3D.Win32]::GetConsoleWindow(); if($h -ne [IntPtr]::Zero){[N3D.Win32]::ShowWindow($h,9) | Out-Null}" >nul 2>&1
exit /b 0

:dirty
echo ERROR: Local tracked changes prevent automatic update.
echo Run: git status --short
echo Preserve your work before restoring any files.
goto :failed

:failed
call :close_splash
call :restore_console
echo.
echo ERROR: N3DLite update or launch failed.
echo Check the output above. This window will stay open.
pause
exit /b 1

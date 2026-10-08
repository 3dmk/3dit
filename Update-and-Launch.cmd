@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
title CoreModel - Update Build Launch
echo.
echo === CoreModel automatic updater ===
echo Project: %CD%
echo.

where git.exe >nul 2>&1
if errorlevel 1 goto :missing_git
where cmake.exe >nul 2>&1
if errorlevel 1 goto :missing_cmake

if not exist ".git" goto :not_git
echo [1/4] Checking local source changes...
for /f "delims=" %%A in ('git status --porcelain --untracked-files=no 2^>nul') do goto :dirty

echo [2/4] Updating from GitHub...
git pull --ff-only origin main
if errorlevel 1 goto :failed

echo [3/4] Configuring build...
if exist "build\CMakeCache.txt" (
  cmake -S . -B build
) else (
  cmake -S . -B build -G "Visual Studio 18 2026" -A x64
)
if errorlevel 1 goto :failed

echo [4/4] Building CoreModel.exe...
cmake --build build --config Release --parallel 2
if errorlevel 1 goto :failed
if not exist "build\Release\CoreModel.exe" goto :missing_exe

echo.
echo Build successful. Launching CoreModel.exe directly.
echo This launcher does not open Visual Studio or run a solution file.
start "" /D "%CD%" "%CD%\build\Release\CoreModel.exe"
if errorlevel 1 goto :failed
exit /b 0

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
echo.
echo CoreModel update/build failed. Nothing will be launched.
pause
exit /b 1

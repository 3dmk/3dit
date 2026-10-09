@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title N3DLite - Recovery Launcher
echo.
echo === N3DLite recovery launcher ===
echo Working directory: %CD%
echo.
if not exist ".git" goto :nogit
where git.exe >nul 2>&1
if errorlevel 1 goto :nogittool
echo Updating repository...
git pull --ff-only origin main
if errorlevel 1 goto :pullfailed
where cmake.exe >nul 2>&1
if errorlevel 1 goto :nocmake
echo Configuring...
cmake -S . -B build
if errorlevel 1 goto :buildfailed
echo Building...
cmake --build build --config Release --parallel 2
if errorlevel 1 goto :buildfailed
if not exist "build\Release\N3DLite.exe" goto :buildfailed
echo Starting N3DLite...
start "" /D "%CD%" "%CD%\build\Release\N3DLite.exe"
if errorlevel 1 goto :buildfailed
exit /b 0
:nogit
echo ERROR: Not a Git checkout.
goto :error
:nogittool
echo ERROR: Git not found.
goto :error
:nocmake
echo ERROR: CMake not found.
goto :error
:pullfailed
echo ERROR: Git pull failed. Run git status --short.
goto :error
:buildfailed
echo ERROR: Configuration, build, or startup failed.
:error
echo.
echo Recovery launcher stopped. The console will stay open.
pause
exit /b 1

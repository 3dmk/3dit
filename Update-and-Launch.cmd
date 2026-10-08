@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CoreModel
set "EXE=%~dp0build\Release\CoreModel.exe"
if exist "%EXE%" (
  echo Starting CoreModel...
  start "" "%EXE%"
  exit /b 0
)
echo CoreModel.exe not found at:
echo %EXE%
echo.
echo Building CoreModel using the updater...
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
  echo ERROR: Windows PowerShell is missing.
  pause
  exit /b 1
)
if not exist "%~dp0Update-and-Launch.ps1" (
  echo ERROR: Update-and-Launch.ps1 is missing.
  pause
  exit /b 1
)
"%PS%" -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
if errorlevel 1 (
  echo.
  echo Build failed. Check Update-and-Launch.log.
  pause
  exit /b 1
)
exit /b 0

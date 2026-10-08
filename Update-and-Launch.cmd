@echo off
setlocal
cd /d "%~dp0"
rem Keep the command launcher minimal. PowerShell owns update, logging and errors.
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
  echo ERROR: Windows PowerShell was not found.
  pause
  exit /b 1
)
if not exist "%~dp0Update-and-Launch.ps1" (
  echo ERROR: Update-and-Launch.ps1 is missing.
  pause
  exit /b 1
)
"%PS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
if errorlevel 1 (
  echo.
  echo CoreModel update failed. Check Update-and-Launch.log in this folder.
  pause
  exit /b 1
)
exit /b 0

@echo off
setlocal
cd /d "%~dp0"
rem Refresh first so an old launcher can recover itself.
where git >nul 2>&1
if not errorlevel 1 git pull --ff-only origin main >nul 2>&1
rem Start the native Windows splash without leaving a console window open.
start "" powershell.exe -NoProfile -STA -WindowStyle Hidden -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
exit /b 0

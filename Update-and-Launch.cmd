@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CoreModel Update and Launch
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
 echo ERROR: Windows PowerShell was not found.
 pause
 exit /b 1
)
if not exist "%~dp0Update-and-Launch.ps1" (
 echo ERROR: Update-and-Launch.ps1 was not found.
 pause
 exit /b 1
)
echo Checking GitHub updates and building CoreModel if needed...
"%PS%" -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
if errorlevel 1 (
 echo.
 echo Update or build failed. See Update-and-Launch.log.
 pause
 exit /b 1
)
exit /b 0

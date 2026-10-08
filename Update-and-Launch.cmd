@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CoreModel Launcher
set "EXE=%~dp0build\Release\CoreModel.exe"
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%EXE%" (
  echo CoreModel executable not found:
  echo "%EXE%"
  echo Running updater to build it...
  if not exist "%~dp0Update-and-Launch.ps1" goto :failed
  if not exist "%PS%" goto :failed
  "%PS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
  if errorlevel 1 goto :failed
  exit /b 0
)
echo Launching "%EXE%"
rem Direct execution avoids PowerShell -Command argument binding errors and CMD start ambiguity.
"%EXE%"
set "RESULT=%ERRORLEVEL%"
echo CoreModel process exit code: %RESULT%
if not "%RESULT%"=="0" goto :failed
exit /b 0
:failed
echo.
echo CoreModel failed to start or exited with an error.
echo To test directly, run this in PowerShell:
echo ^& "C:\GPT\CoreModel_GitHub\build\Release\CoreModel.exe"
pause
exit /b 1

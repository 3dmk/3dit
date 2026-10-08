@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CoreModel Update and Launch
set "ROOT=%~dp0"
set "LOG=%ROOT%Update-and-Launch-bootstrap.log"
echo CoreModel launcher started at %date% %time% > "%LOG%"
echo Working folder: %CD% >> "%LOG%"
echo.
echo === CoreModel Update and Launch ===
echo Folder: %CD%
echo.
if not exist "%ROOT%Update-and-Launch.ps1" (
 echo ERROR: Update-and-Launch.ps1 is missing.
 echo Missing PowerShell launcher >> "%LOG%"
 goto :failed
)
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
 echo ERROR: Windows PowerShell is missing.
 echo Windows PowerShell not found >> "%LOG%"
 goto :failed
)
rem The PowerShell launcher owns Git pull, CMake and editor startup.
rem Keep this console visible to expose startup errors.
"%PS%" -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%ROOT%Update-and-Launch.ps1"
set "RESULT=%ERRORLEVEL%"
echo PowerShell launcher exit code: %RESULT% >> "%LOG%"
if not "%RESULT%"=="0" goto :failed
exit /b 0
:failed
echo.
echo CoreModel did not launch successfully.
echo Check "%LOG%" and "%ROOT%Update-and-Launch.log"
pause
exit /b 1

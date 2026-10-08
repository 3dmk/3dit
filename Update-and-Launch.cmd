@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CoreModel Launcher
set "EXE=%~dp0build\Release\CoreModel.exe"
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
  echo ERROR: Windows PowerShell not found.
  pause
  exit /b 1
)
if not exist "%EXE%" (
  echo CoreModel.exe not found. Running build updater...
  if not exist "%~dp0Update-and-Launch.ps1" (
    echo ERROR: Update-and-Launch.ps1 not found.
    pause
    exit /b 1
  )
  "%PS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
  if errorlevel 1 (
    echo Build failed. See Update-and-Launch.log
    pause
    exit /b 1
  )
  exit /b 0
)
rem Run the exact executable directly, matching the verified PowerShell command.
rem Do not use CMD start, which has different argument and working-directory behavior.
"%PS%" -NoProfile -ExecutionPolicy Bypass -Command "$exe = Join-Path $args[0] 'build\Release\CoreModel.exe'; if (Test-Path -LiteralPath $exe) { & $exe; exit $LASTEXITCODE } else { Write-Host 'CoreModel.exe does not exist'; exit 2 }" "%~dp0"
if errorlevel 1 (
  echo.
  echo CoreModel exited with an error.
  pause
  exit /b 1
)
exit /b 0

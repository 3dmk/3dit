@echo off
setlocal
cd /d "%~dp0"
title CoreModel - Loading
mode con: cols=82 lines=28 >nul 2>&1
color 0B
rem Refresh the launcher before starting PowerShell, including recovery from an older broken script.
where git >nul 2>&1
if not errorlevel 1 git pull --ff-only origin main >nul 2>&1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
if errorlevel 1 pause

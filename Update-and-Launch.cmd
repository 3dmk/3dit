@echo off
setlocal
cd /d "%~dp0"
title CoreModel - Loading
mode con: cols=82 lines=28 >nul 2>&1
color 0B
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Update-and-Launch.ps1"
if errorlevel 1 pause

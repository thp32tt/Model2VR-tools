@echo off
setlocal EnableExtensions
title Daytona USA - Quest 3 OpenXR VR Display
cd /d "%~dp0"
if not exist "%~dp0Start-DaytonaVR.ps1" (
  echo Missing Start-DaytonaVR.ps1
  pause
  exit /b 2
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-DaytonaVR.ps1" -Emulator "%~1"
set "RESULT=%errorlevel%"
if not "%RESULT%"=="0" pause
exit /b %RESULT%

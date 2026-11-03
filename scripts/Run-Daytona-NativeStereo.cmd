@echo off
setlocal EnableExtensions
title Daytona USA native-eye stereo VR (experimental)
cd /d "%~dp0"
set "EMULATOR=%~1"
if not defined EMULATOR (
  set /p "EMULATOR=Original EMULATOR.EXE path: "
)
if not exist "%EMULATOR%" (
  echo ERROR: Cannot find emulator EXE: "%EMULATOR%"
  pause
  exit /b 7
)
rem Safe mode: launch the unchanged emulator without any VR DLL injection.
rem Presence of this local flag never changes ROMs, saves or MOZA FFB.
if exist "%~dp0daytona_vr_safe_mode.flag" (
  echo Daytona VR SAFE MODE: launching untouched original emulator.
  start "" "%EMULATOR%"
  exit /b %ERRORLEVEL%
)
if not exist "%~dp0Model2VRLoader.exe" (
  echo ERROR: Model2VRLoader.exe missing.
  pause
  exit /b 2
)
if not exist "%~dp0Model2VRProbe.dll" (
  echo ERROR: Model2VRProbe.dll missing.
  pause
  exit /b 3
)
if not exist "%~dp0Model2VROpenXRHost.exe" (
  echo ERROR: Model2VROpenXRHost.exe missing.
  pause
  exit /b 4
)
if not exist "%~dp0daytona_native_stereo.ini" (
  echo ERROR: daytona_native_stereo.ini missing.
  pause
  exit /b 5
)
copy /Y "%~dp0daytona_native_stereo.ini" "%~dp0model2vr_probe.ini" >nul
if errorlevel 1 (
  echo ERROR: Cannot write probe config. Extract ZIP to writable folder.
  pause
  exit /b 6
)
echo Quest 3 OpenXR runtime must be active.
echo Original game simulation/input/FFB executes once.
echo Experimental stereo: left/right original GPU scene draws, not a 2D theater.
"%~dp0Model2VRLoader.exe" "%EMULATOR%"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
  echo ERROR: loader exited with code %RESULT%.
  pause
)
exit /b %RESULT%

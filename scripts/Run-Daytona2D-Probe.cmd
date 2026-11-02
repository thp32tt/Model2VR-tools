@echo off
setlocal EnableExtensions
title Daytona USA - Model 2 D3D9 2D Capture
cd /d "%~dp0"
echo.
echo Daytona USA - read-only 2D render capture (no VR or FFB UI)
echo.
if not exist "%~dp0Model2VRLoader.exe" (
  echo ERROR: Model2VRLoader.exe is missing from this folder.
  pause
  exit /b 2
)
if not exist "%~dp0Model2VRProbe.dll" (
  echo ERROR: Model2VRProbe.dll is missing from this folder.
  pause
  exit /b 3
)
set "EMULATOR=%~1"
if not defined EMULATOR (
  echo Drag the original Model 2 emulator EXE onto this BAT,
  echo or enter the full emulator EXE path below.
  set /p "EMULATOR=Emulator EXE path: "
)
if not exist "%EMULATOR%" (
  echo ERROR: Emulator EXE was not found: "%EMULATOR%"
  pause
  exit /b 4
)
copy /y "%~dp0daytona_2d_probe.ini" "%~dp0model2vr_probe.ini" >nul
if errorlevel 1 (
  echo ERROR: Cannot write the probe INI; move the package to a writable folder.
  pause
  exit /b 5
)
echo Using original emulator: "%EMULATOR%"
echo Probe configuration: "%~dp0model2vr_probe.ini"
echo Logs will appear alongside this BAT as:
echo   daytona_2d_probe_YYYYMMDD_HHMMSS_PID.csv
echo.
echo Start Daytona USA in the emulator, play in normal 2D, then exit.
echo The loader can exit before the emulator game finishes.
echo.
"%~dp0Model2VRLoader.exe" "%EMULATOR%"
set "RESULT=%errorlevel%"
if not "%RESULT%"=="0" (
  echo.
  echo Probe launch failed with error %RESULT%.
  echo If no CSV is created, the existing D3D9 runtime may not use
  echo Direct3DCreate9 / CreateDevice or the probe failed to initialize.
)
echo.
echo Check this folder for daytona_2d_probe_*.csv after exiting the emulator.
pause
exit /b %RESULT%

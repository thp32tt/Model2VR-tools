@echo off
setlocal EnableExtensions
title Daytona USA VR
cd /d "%~dp0"

set "EMULATOR=%~1"
if not defined EMULATOR if exist "%~dp0daytona_emulator_path.txt" set /p "EMULATOR="<"%~dp0daytona_emulator_path.txt"
if not defined EMULATOR if exist "%~dp0EMULATOR.EXE" set "EMULATOR=%~dp0EMULATOR.EXE"
if not defined EMULATOR if exist "%~dp0..\EMULATOR.EXE" set "EMULATOR=%~dp0..\EMULATOR.EXE"

if not defined EMULATOR (
  echo First run only: enter the original Model 2 EMULATOR.EXE path.
  set /p "EMULATOR=EMULATOR.EXE: "
)
if not exist "%EMULATOR%" (
  echo ERROR: Cannot find emulator EXE: "%EMULATOR%"
  pause
  exit /b 7
)

> "%~dp0daytona_emulator_path.txt" echo %EMULATOR%
for %%I in ("%EMULATOR%") do set "EMULATOR_DIR=%%~dpI"

if exist "%~dp0daytona_vr_safe_mode.flag" (
  start "" /D "%EMULATOR_DIR%" "%EMULATOR%"
  exit /b %ERRORLEVEL%
)

for %%F in (Model2VRLoader.exe Model2VRProbe.dll Model2VROpenXRHost.exe daytona_native_stereo.ini) do (
  if not exist "%~dp0%%F" (
    echo ERROR: %%F is missing from this folder.
    pause
    exit /b 2
  )
)

copy /Y "%~dp0daytona_native_stereo.ini" "%~dp0model2vr_probe.ini" >nul
if errorlevel 1 (
  echo ERROR: Cannot write model2vr_probe.ini.
  pause
  exit /b 6
)

if not exist "%~dp0logs" mkdir "%~dp0logs" >nul 2>&1
set "STAMP=%RANDOM%_%RANDOM%"
for /f %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss" 2^>nul') do set "STAMP=%%T"
set "LAUNCHLOG=%~dp0logs\daytona_vr_launcher_%STAMP%.log"

> "%LAUNCHLOG%" echo Daytona VR launcher
>>"%LAUNCHLOG%" echo emulator=%EMULATOR%
>>"%LAUNCHLOG%" echo started=%DATE% %TIME%

"%~dp0Model2VRLoader.exe" "%EMULATOR%" >>"%LAUNCHLOG%" 2>&1
set "RESULT=%ERRORLEVEL%"
>>"%LAUNCHLOG%" echo loader_exit=%RESULT%

if not "%RESULT%"=="0" (
  echo.
  echo Daytona VR did not start. The desktop game was stopped instead of silently falling back to 2D.
  echo Startup log: "%LAUNCHLOG%"
  echo OpenXR host log is in: "%~dp0logs"
  echo.
  type "%LAUNCHLOG%"
  pause
  exit /b %RESULT%
)

echo Daytona VR started.
echo Logs: "%~dp0logs"
exit /b 0

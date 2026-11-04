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

> "%~dp0daytona_emulator_path.txt" (
  <nul set /p "=%EMULATOR%"
  echo(
)
for %%I in ("%EMULATOR%") do set "EMULATOR_DIR=%%~dpI"

if not exist "%~dp0logs" mkdir "%~dp0logs" >nul 2>&1
set "STAMP=%RANDOM%_%RANDOM%"
for /f %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss" 2^>nul') do set "STAMP=%%T"
set "STAMP=%STAMP%_%RANDOM%"
set "LAUNCHLOG=%~dp0logs\daytona_vr_launcher_%STAMP%.log"
> "%LAUNCHLOG%" echo Daytona VR launcher
>>"%LAUNCHLOG%" echo emulator=%EMULATOR%
>>"%LAUNCHLOG%" echo started=%DATE% %TIME%

if exist "%~dp0daytona_vr_safe_mode.flag" (
  echo SAFE MODE: daytona_vr_safe_mode.flag is present. Starting original 2D only.
  echo Remove the flag to try the OpenXR native VR path.
  >>"%LAUNCHLOG%" echo SAFE_MODE_FLAG_PRESENT: original 2D launch, VR disabled.
  start "" /D "%EMULATOR_DIR%" "%EMULATOR%"
  exit /b %ERRORLEVEL%
)

for %%F in (Model2VRLoader.exe Model2VRProbe.dll Model2VROpenXRHost.exe daytona_native_stereo.ini) do (
  if not exist "%~dp0%%F" (
    echo ERROR: %%F is missing from this folder.
    >>"%LAUNCHLOG%" echo MISSING_DEPENDENCY=%%F
    echo Startup log: "%LAUNCHLOG%"
    pause
    exit /b 2
  )
)

copy /Y "%~dp0daytona_native_stereo.ini" "%~dp0model2vr_probe.ini" >nul
if errorlevel 1 (
  echo ERROR: Cannot write model2vr_probe.ini.
  >>"%LAUNCHLOG%" echo CONFIG_COPY_FAILED
  echo Startup log: "%LAUNCHLOG%"
  pause
  exit /b 6
)

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

echo Daytona VR loader completed. OpenXR host startup is not proof of game pixels.
echo If the headset shows one solid color per eye, check the host log for:
echo   Daytona VR eye delivery: ... action=...
echo Detailed original D3D9 producer reason and Draw counters are logged there.
echo Press F9 while OpenXR is running to append local diagnosis in logs.
>>"%LAUNCHLOG%" echo LOADER_COMPLETE_ONLY: see OpenXR host eye delivery status before declaring native game stereo
echo Logs: "%~dp0logs"
exit /b 0

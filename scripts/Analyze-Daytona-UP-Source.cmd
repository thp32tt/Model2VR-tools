@echo off
setlocal EnableExtensions
title Daytona original UP Draw and Present evidence
cd /d "%~dp0"
set "SOURCE=%~1"
if not defined SOURCE (
  for /f "delims=" %%F in ('dir /b /a-d /o-d "%~dp0logs\daytona_vr_startup_*.csv" 2^>nul') do (
    if not defined SOURCE set "SOURCE=%~dp0logs\%%F"
  )
)
if not defined SOURCE (
  echo No original Daytona x86 startup CSV found in logs.
  echo You can also pass an explicit private CSV file as the first argument.
  pause
  exit /b 2
)
if not exist "%SOURCE%" (
  echo Missing private source log: "%SOURCE%"
  pause
  exit /b 2
)
if not exist "%~dp0Analyze-OriginalUP-Present.py" (
  echo Missing optional source analyzer in matched release package.
  pause
  exit /b 3
)
if not exist "%~dp0logs" mkdir "%~dp0logs" >nul 2>&1
set "OUTPUT=%~dp0logs\daytona_up_physical_%RANDOM%_%RANDOM%.json"
echo Input:  "%SOURCE%"
echo Output: "%OUTPUT%"
echo This is a local read-only candidate report, NEVER world camera proof.
where py >nul 2>&1
if not errorlevel 1 (
  py -3 "%~dp0Analyze-OriginalUP-Present.py" "%SOURCE%" --output "%OUTPUT%"
  if errorlevel 1 goto failed
  goto success
)
where python >nul 2>&1
if errorlevel 1 (
  echo Python 3 was not found. Install Python to use this optional analyzer.
  pause
  exit /b 4
)
python "%~dp0Analyze-OriginalUP-Present.py" "%SOURCE%" --output "%OUTPUT%"
if errorlevel 1 goto failed
:success
echo Candidate metadata saved ONLY on this computer.
echo World/HUD and OpenXR 6DoF still require independent original-game proof.
pause
exit /b 0
:failed
echo Original UP result audit failed. Retain the private CSV locally.
pause
exit /b 5

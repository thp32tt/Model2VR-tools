@echo off
setlocal EnableExtensions
title Daytona original D3D9 Set and Multiply transform evidence
cd /d "%~dp0"
set "SOURCE=%~1"
if not defined SOURCE (
  for /f "delims=" %%F in ('dir /b /a-d /o-d "%~dp0logs\daytona_vr_startup_*.csv" 2^>nul') do (
    if not defined SOURCE set "SOURCE=%~dp0logs\%%F"
  )
)
if not defined SOURCE (
  echo No original Daytona x86 source CSV found in logs.
  echo Drag your own private CSV onto this launcher to select another capture.
  pause
  exit /b 2
)
if not exist "%SOURCE%" (
  echo Missing original private source CSV: "%SOURCE%"
  pause
  exit /b 2
)
if not exist "%~dp0Analyze-OriginalCamera-Phase.py" (
  echo Missing camera-state auditor from matched release package.
  pause
  exit /b 3
)
if not exist "%~dp0logs" mkdir "%~dp0logs" >nul 2>&1
set "OUTPUT=%~dp0logs\daytona_transform_physical_%RANDOM%_%RANDOM%.json"
echo Input:  "%SOURCE%"
echo Output: "%OUTPUT%"
echo Source evidence only: original SetTransform, MultiplyTransform, VS and region.
where py >nul 2>&1
if not errorlevel 1 (
  py -3 "%~dp0Analyze-OriginalCamera-Phase.py" "%SOURCE%" --output "%OUTPUT%"
  if errorlevel 1 goto failed
  goto success
)
where python >nul 2>&1
if errorlevel 1 (
  echo Python 3 was not found. Install Python to run this optional auditor.
  pause
  exit /b 4
)
python "%~dp0Analyze-OriginalCamera-Phase.py" "%SOURCE%" --output "%OUTPUT%"
if errorlevel 1 goto failed
:success
echo Source evidence saved ONLY on this computer. Original CSV is unchanged.
echo World/HUD and OpenXR 6DoF still require independent original game proof.
pause
exit /b 0
:failed
echo Original transform/camera phase audit failed. Retain private CSV locally.
pause
exit /b 5

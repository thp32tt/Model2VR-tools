@echo off
setlocal EnableExtensions
set "PACKAGE_ROOT=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-Daytona-Original-Evidence.ps1" -PackageRoot "%PACKAGE_ROOT%" %*
set "CODE=%ERRORLEVEL%"
if not "%CODE%"=="0" (
  echo.
  echo Private Daytona source collection/audit was NOT completed. No world proof granted.
  pause
)
exit /b %CODE%

@echo off
if not exist "%~dp0native\Remove-NativeMarkers.ps1" (
 echo Extract ALL files from the ZIP before running this remover.
 pause
 exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0native\Remove-NativeMarkers.ps1"
set "install_result=%errorlevel%"
pause
exit /b %install_result%

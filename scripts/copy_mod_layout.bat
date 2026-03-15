@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
cd /d "%PROJECT_ROOT%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0copy_mod_layout.ps1" %*
exit /b %ERRORLEVEL%

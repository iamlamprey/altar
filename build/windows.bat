@echo off
set SCRIPT_DIR=%~dp0
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%windows.ps1" %*
pause

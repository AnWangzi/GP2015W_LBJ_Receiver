@echo off
setlocal
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
if not defined LBJ_PYTHON set "LBJ_PYTHON=python.exe"
"%LBJ_PYTHON%" "%~dp0tools\serial_flash_wizard.py"
set "LBJ_RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %LBJ_RESULT%

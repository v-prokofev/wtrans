@echo off
title DVU-01 Sensor Emulator

echo.
echo  === DVU-01 Sensor Emulator ===
echo.

set "PYTHON="

python --version >nul 2>&1
if %errorlevel% == 0 (
    set "PYTHON=python"
    goto found
)

py --version >nul 2>&1
if %errorlevel% == 0 (
    set "PYTHON=py"
    goto found
)

echo  [ERROR] Python not found. Get it from https://python.org
pause
exit /b 1

:found
echo  [OK] Python: %PYTHON%
"%PYTHON%" --version
echo.

"%PYTHON%" -c "import PySide6" >nul 2>&1
if %errorlevel% neq 0 (
    echo  Installing PySide6...
    "%PYTHON%" -m pip install --quiet PySide6
    if %errorlevel% neq 0 ( echo  [ERROR] pip failed. & pause & exit /b 1 )
    echo  [OK] PySide6 installed.
) else (
    echo  [OK] PySide6 OK.
)

"%PYTHON%" -c "import serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo  Installing pyserial...
    "%PYTHON%" -m pip install --quiet pyserial
    if %errorlevel% neq 0 ( echo  [ERROR] pip failed. & pause & exit /b 1 )
    echo  [OK] pyserial installed.
) else (
    echo  [OK] pyserial OK.
)

echo.
echo  Starting emulator...
echo.
cd /d "%~dp0"
"%PYTHON%" emulator.py
if %errorlevel% neq 0 ( echo. & echo  [ERROR] Exit code %errorlevel%. & pause )
@echo off
REM === start_proxy.bat ===
REM Adjust these as needed:
set SERIAL_PORT=COM12
set BAUD=115200
set LISTEN_HOST=127.0.0.1
set LISTEN_PORT=7777

REM If you use a virtualenv, activate it here:
REM call C:\path\to\venv\Scripts\activate.bat

python "%~dp0rotator_proxy.py" ^
  --serial-port %SERIAL_PORT% ^
  --baud %BAUD% ^
  --listen-host %LISTEN_HOST% ^
  --listen-port %LISTEN_PORT%

pause

@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (py -3 -m venv .venv || goto fail)
.venv\Scripts\python.exe -m pip install -r requirements.txt || goto fail
.venv\Scripts\python.exe companion.py --lan
:fail
pause

@echo off
cd /d "%~dp0"
if not exist "venv\Scripts\activate.bat" (
  echo Virtual environment not found.
  echo Run: python -m venv venv
  echo Then: venv\Scripts\python -m pip install -r requirements.txt
  pause
  exit /b 1
)
call "venv\Scripts\activate.bat"
python main.py
if errorlevel 1 pause

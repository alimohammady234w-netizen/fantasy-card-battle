@echo off
setlocal
cd /d "%~dp0"
if not exist "venv\Scripts\python.exe" (
  echo Virtual environment not found. Install Python 3.12 or newer, then run:
  echo python -m venv venv
  echo venv\Scripts\python -m pip install -r requirements-build.txt
  pause
  exit /b 1
)
"venv\Scripts\python.exe" tools\build_release.py
if errorlevel 1 (
  echo Build failed. See build\release-report.json and build\release-checks\ logs.
  echo Dependencies: venv\Scripts\python -m pip install -r requirements-build.txt
  pause
  exit /b 1
)
echo Verified release created in dist. Build report: build\release-report.json
pause
exit /b 0

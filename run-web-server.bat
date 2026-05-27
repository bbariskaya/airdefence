@echo off
cd /d "%~dp0\web"
echo RadarSim web server: http://localhost:8765
echo Press Ctrl+C to stop.
where python >nul 2>&1
if %errorlevel%==0 (
  start http://localhost:8765
  python -m http.server 8765
  exit /b 0
)
where py >nul 2>&1
if %errorlevel%==0 (
  start http://localhost:8765
  py -m http.server 8765
  exit /b 0
)
echo Python not found. Open web\index.html manually or install Python.
pause

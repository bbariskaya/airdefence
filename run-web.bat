@echo off
cd /d "%~dp0\web"
echo Opening RadarSim 2D game in browser...
if exist index.html (
  start "" "%~dp0web\index.html"
) else (
  echo index.html not found.
  exit /b 1
)

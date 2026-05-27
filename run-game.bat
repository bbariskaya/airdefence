@echo off
cd /d "%~dp0"
if exist bin\radar_sim.exe (
    bin\radar_sim.exe
    exit /b 0
)
if exist build\radar_sim.exe (
    build\radar_sim.exe
    exit /b 0
)
echo GUI not built. Run: build-gui.bat
pause
exit /b 1

@echo off
cd /d "%~dp0"
if not exist bin\radar_console.exe (
  echo Run build-console.bat first
  pause
  exit /b 1
)
bin\radar_console.exe

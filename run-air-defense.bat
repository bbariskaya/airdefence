@echo off
REM Run script for Iron Dome Air Defense Game

if not exist build\air_defense_game.exe (
    echo Error: Executable not found. Please run build-air-defense.bat first.
    exit /b 1
)

echo Launching Iron Dome Battery Game...
build\air_defense_game.exe

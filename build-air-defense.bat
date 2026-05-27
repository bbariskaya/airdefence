@echo off
REM Build script for Iron Dome Air Defense Game
REM Uses CMake and MinGW-w64

if not exist build mkdir build
cd build
cmake -G "MinGW Makefiles" ..
if errorlevel 1 (
    echo Build configuration failed!
    exit /b 1
)
mingw32-make air_defense_game
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
echo Build successful! Executable: bin\air_defense_game.exe
cd ..

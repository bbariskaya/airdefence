@echo off
cd /d "%~dp0"
set PATH=C:\Program Files\CMake\bin;%PATH%

echo Building RadarSim GUI...
if not exist build mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DRADAR_BUILD_GRAPHICS=ON ..
if errorlevel 1 goto fail
cmake --build . -j 4 --target radar_sim
if errorlevel 1 goto fail
if not exist ..\bin mkdir ..\bin
copy /Y radar_sim.exe ..\bin\ >nul
echo.
echo OK - run: run-game.bat
cd ..
exit /b 0
:fail
echo FAILED
cd ..
pause
exit /b 1

@echo off
cd /d "%~dp0"
where g++ >nul 2>&1 || (echo Install MinGW g++ & pause & exit /b 1)
if not exist bin mkdir bin
echo Building console...
g++ -std=c++17 -O2 -I core/include -c core/src/radar_engine.cpp -o bin/radar_engine.o || goto fail
g++ -std=c++17 -O2 -I core/include -c core/src/simulated_world.cpp -o bin/simulated_world.o || goto fail
g++ -std=c++17 -O2 -I core/include -c apps/radar_console/main.cpp -o bin/main.o || goto fail
g++ -std=c++17 -O2 -o bin/radar_console.exe bin/radar_engine.o bin/simulated_world.o bin/main.o || goto fail
echo OK: bin\radar_console.exe
exit /b 0
:fail
echo Build failed
pause
exit /b 1

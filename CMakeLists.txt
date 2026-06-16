cmake_minimum_required(VERSION 3.16)
project(RadarSim VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(radar_core
    core/src/radar_engine.cpp
    core/src/simulated_world.cpp
)
target_include_directories(radar_core PUBLIC core/include)

add_executable(radar_console apps/radar_console/main.cpp)
target_link_libraries(radar_console PRIVATE radar_core)

# Debug/headless air defense game (no GUI)
add_executable(air_defense_debug
    apps/air_defense_game/main_debug.cpp
    apps/air_defense_game/threat_world.cpp
    apps/air_defense_game/interceptor.cpp
    apps/air_defense_game/battery_controller.cpp
)
target_include_directories(air_defense_debug PRIVATE apps/air_defense_game)
target_link_libraries(air_defense_debug PRIVATE radar_core)

# Unit tests
add_executable(test_unit
    apps/tests/test_unit.cpp
    apps/air_defense_game/threat_world.cpp
    apps/air_defense_game/interceptor.cpp
    apps/air_defense_game/battery_controller.cpp
)
target_include_directories(test_unit PRIVATE apps/air_defense_game)
target_link_libraries(test_unit PRIVATE radar_core)

# Integration tests
add_executable(test_integration
    apps/tests/test_integration.cpp
    apps/air_defense_game/threat_world.cpp
    apps/air_defense_game/interceptor.cpp
    apps/air_defense_game/battery_controller.cpp
)
target_include_directories(test_integration PRIVATE apps/air_defense_game)
target_link_libraries(test_integration PRIVATE radar_core)

option(RADAR_BUILD_GRAPHICS "Build Raylib PPI game" ON)
if(RADAR_BUILD_GRAPHICS)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)

    add_executable(radar_sim apps/radar_sim/main.cpp)
    target_link_libraries(radar_sim PRIVATE radar_core raylib)
    if(WIN32)
        target_link_libraries(radar_sim PRIVATE winmm)
    endif()

    add_executable(air_defense_game
        apps/air_defense_game/main.cpp
        apps/air_defense_game/threat_world.cpp
        apps/air_defense_game/interceptor.cpp
        apps/air_defense_game/battery_controller.cpp
        apps/air_defense_game/game_hud.cpp
    )
    target_include_directories(air_defense_game PRIVATE apps/air_defense_game)
    target_link_libraries(air_defense_game PRIVATE radar_core raylib)
    if(WIN32)
        target_link_libraries(air_defense_game PRIVATE winmm)
    endif()
endif()

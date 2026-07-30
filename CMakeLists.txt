cmake_minimum_required(VERSION 3.24)

project(ProceduralRaylibStarter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(Engine SHARED
    Engine/Source/Application.cpp
    Engine/Source/DynamicLibrary.cpp
    Engine/Source/InputSystem.cpp
    Engine/Source/Renderer2D.cpp
)

target_include_directories(Engine PUBLIC Engine/Include)
target_link_libraries(Engine PRIVATE raylib)
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)

add_library(Game SHARED
    Game/Source/GameModule.cpp
    Game/Source/Game.cpp
    Game/Source/GameInput.cpp
    Game/Source/Player.cpp
)

target_include_directories(Game PRIVATE Game/Include)
target_link_libraries(Game PRIVATE Engine)
target_compile_definitions(Game PRIVATE GAME_MODULE_BUILD)

add_executable(Launcher Launcher/Source/main.cpp)
target_link_libraries(Launcher PRIVATE Engine)

foreach(target Engine Game Launcher)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

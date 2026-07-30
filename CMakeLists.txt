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

add_executable(ProceduralRaylibStarter
    src/main.cpp
    src/Application.cpp
    src/Game.cpp
    src/Input.cpp
    src/Player.cpp
)

target_include_directories(ProceduralRaylibStarter PRIVATE src)
target_link_libraries(ProceduralRaylibStarter PRIVATE raylib)

if(MSVC)
    target_compile_options(ProceduralRaylibStarter PRIVATE /W4 /permissive-)
else()
    target_compile_options(ProceduralRaylibStarter PRIVATE -Wall -Wextra -Wpedantic)
endif()


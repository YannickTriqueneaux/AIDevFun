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

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rlimgui)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(EngineImGui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(EngineImGui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/cpp
    ${rlimgui_SOURCE_DIR}
)
target_link_libraries(EngineImGui PUBLIC raylib)

add_library(Engine SHARED
    Engine/Source/Application.cpp
    Engine/Source/AssistantApplication.cpp
    Engine/Source/DynamicLibrary.cpp
    Engine/Source/InputSystem.cpp
    Engine/Source/OpenAIClient.cpp
    Engine/Source/PromptConsole.cpp
    Engine/Source/PromptProcessor.cpp
    Engine/Source/Renderer2D.cpp
    Engine/Source/Settings.cpp
    Engine/Source/UiSystem.cpp
)

target_include_directories(Engine PUBLIC Engine/Include)
target_link_libraries(Engine PRIVATE raylib EngineImGui nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(Engine PRIVATE winhttp)
endif()
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

add_executable(GameHost GameHost/Source/main.cpp)
target_link_libraries(GameHost PRIVATE Engine)

add_executable(AssistantHost AssistantHost/Source/main.cpp)
target_link_libraries(AssistantHost PRIVATE Engine)

add_executable(Launcher Launcher/Source/main.cpp)
if(WIN32)
    target_link_libraries(Launcher PRIVATE shell32)
endif()

add_custom_command(
    TARGET Launcher
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Launcher/settings.json
        $<TARGET_FILE_DIR:Launcher>/settings.json
)

foreach(target Engine Game GameHost AssistantHost Launcher)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

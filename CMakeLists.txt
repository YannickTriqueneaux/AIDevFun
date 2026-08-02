cmake_minimum_required(VERSION 3.24)

project(MakeYourOwnGame.AI LANGUAGES CXX)

set(MAKE_YOUR_OWN_GAME_AI_VERSION_MAJOR 1)
set(MAKE_YOUR_OWN_GAME_AI_VERSION_MINOR 4)
set(MAKE_YOUR_OWN_GAME_AI_VERSION_PATCH 0)
set(MAKE_YOUR_OWN_GAME_AI_VERSION
    "${MAKE_YOUR_OWN_GAME_AI_VERSION_MAJOR}.${MAKE_YOUR_OWN_GAME_AI_VERSION_MINOR}.${MAKE_YOUR_OWN_GAME_AI_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

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

set(GAME_PROJECT "BaseGame" CACHE STRING
    "Name of the game project under Games/ to build and edit")
set(ACTIVE_GAME_DIR "${CMAKE_SOURCE_DIR}/Games/${GAME_PROJECT}")

if(NOT EXISTS "${ACTIVE_GAME_DIR}/Source/GameModule.cpp")
    message(FATAL_ERROR
        "Invalid GAME_PROJECT '${GAME_PROJECT}'. Expected "
        "${ACTIVE_GAME_DIR}/Source/GameModule.cpp")
endif()

message(STATUS "Active game project: ${GAME_PROJECT}")

option(ENGINE_PROFILE_ENABLED "Enable Tracy profiling instrumentation" OFF)
set(TRACY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../tracy-master" CACHE PATH
    "Path to the Tracy source tree")
if(ENGINE_PROFILE_ENABLED)
    if(NOT EXISTS "${TRACY_SOURCE_DIR}/public/TracyClient.cpp")
        message(FATAL_ERROR
            "ENGINE_PROFILE_ENABLED requires a valid TRACY_SOURCE_DIR: "
            "${TRACY_SOURCE_DIR}")
    endif()
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_STATIC OFF CACHE BOOL "" FORCE)
    set(TRACY_DELAYED_INIT ON CACHE BOOL "" FORCE)
    set(TRACY_MANUAL_LIFETIME ON CACHE BOOL "" FORCE)
    add_subdirectory(
        "${TRACY_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/tracy-build"
        EXCLUDE_FROM_ALL)
endif()

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
    Engine/Source/ProceduralAudio.cpp
    Engine/Source/CrashDiagnostics.cpp
    Engine/Source/DesktopWindow.cpp
    Engine/Source/DynamicLibrary.cpp
    Engine/Source/GameInstance.cpp
    Engine/Source/InputSystem.cpp
    Engine/Source/Logger.cpp
    Engine/Source/Memory.cpp
    Engine/Source/NamedPipe.cpp
    Engine/Source/ObjectManager.cpp
    Engine/Source/ObjectMemory.cpp
    Engine/Source/RenderContext.cpp
    Engine/Source/Renderer2D.cpp
    Engine/Source/VectorShape.cpp
    Engine/Source/Renderer3D.cpp
    Engine/Source/ShaderProgram.cpp
    Engine/Source/World.cpp
    Engine/Source/UiSystem.cpp
    Engine/Source/WindowFocus.cpp
)

target_include_directories(Engine PUBLIC Engine/Include)
target_link_libraries(Engine PRIVATE raylib EngineImGui ${CMAKE_DL_LIBS})
if(ENGINE_PROFILE_ENABLED)
    target_link_libraries(Engine PUBLIC Tracy::TracyClient)
    target_compile_definitions(Engine PUBLIC ENGINE_PROFILE_ENABLED)
    add_custom_command(
        TARGET Engine
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:TracyClient>
            $<TARGET_FILE_DIR:Engine>)
endif()
if(WIN32)
    target_link_libraries(
        Engine PRIVATE
        "$<$<NOT:$<CONFIG:Release>>:dbghelp>")
    target_compile_definitions(
        Engine PRIVATE
        "$<$<NOT:$<CONFIG:Release>>:ENGINE_CRASH_DIAGNOSTICS>")
endif()
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)
target_compile_definitions(
    Engine PUBLIC "$<$<NOT:$<CONFIG:Release>>:ENGINE_AUTOTESTS>")

file(GLOB_RECURSE ACTIVE_GAME_SOURCES CONFIGURE_DEPENDS
    "${ACTIVE_GAME_DIR}/Source/*.cpp")
file(GLOB_RECURSE ACTIVE_GAME_HEADERS CONFIGURE_DEPENDS
    "${ACTIVE_GAME_DIR}/Include/*.h")
set(ACTIVE_GAME_RUNTIME_SOURCES ${ACTIVE_GAME_SOURCES})
list(REMOVE_ITEM ACTIVE_GAME_RUNTIME_SOURCES
    "${ACTIVE_GAME_DIR}/Source/GameModule.cpp")

add_library(Game SHARED ${ACTIVE_GAME_SOURCES} ${ACTIVE_GAME_HEADERS})

target_include_directories(Game PRIVATE ${ACTIVE_GAME_DIR}/Include)
target_link_libraries(Game PRIVATE Engine)
target_compile_definitions(Game PRIVATE GAME_MODULE_BUILD)

add_executable(GameHost
    GameHost/Source/main.cpp
    GameHost/Source/GameToolService.cpp
    GameHost/Source/ReloadableGame.cpp
)
target_include_directories(
    GameHost PRIVATE GameHost/Include Development/Include)
target_link_libraries(GameHost PRIVATE Engine nlohmann_json::nlohmann_json)
target_compile_definitions(
    GameHost PRIVATE MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR="${ACTIVE_GAME_DIR}")
target_compile_definitions(
    GameHost PRIVATE MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT="${GAME_PROJECT}")

add_executable(AssistantHost
    AssistantHost/Source/AssistantApplication.cpp
    AssistantHost/Source/AssistantPromptConfig.cpp
    AssistantHost/Source/AssistantProviderLoader.cpp
    AssistantHost/Source/GameToolClient.cpp
    AssistantHost/Source/main.cpp
    AssistantHost/Source/MidiAttachmentProvider.cpp
    AssistantHost/Source/PromptConsole.cpp
    AssistantHost/Source/PromptProcessor.cpp
    AssistantHost/Source/Settings.cpp
)
target_include_directories(
    AssistantHost PRIVATE AssistantHost/Include Development/Include)
target_link_libraries(
    AssistantHost PRIVATE Engine raylib nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(AssistantHost PRIVATE shell32 winhttp)
endif()
target_compile_definitions(
    AssistantHost PRIVATE MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT="${GAME_PROJECT}")
target_compile_definitions(
    AssistantHost PRIVATE MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR="${ACTIVE_GAME_DIR}")

add_library(AssistantProviderOpenAI SHARED
    AssistantProviders/OpenAI/Source/OpenAIProvider.cpp
    AssistantHost/Source/OpenAIClient.cpp
    AssistantHost/Source/OpenAIPricing.cpp)
target_include_directories(AssistantProviderOpenAI PRIVATE
    AssistantHost/Include Development/Include)
target_link_libraries(AssistantProviderOpenAI PRIVATE
    nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(AssistantProviderOpenAI PRIVATE winhttp)
endif()

add_library(AssistantProviderCodex SHARED
    AssistantProviders/Codex/Source/CodexProvider.cpp
    AssistantProviders/Codex/Source/CodexEventParser.cpp)
target_include_directories(AssistantProviderCodex PRIVATE
    AssistantProviders/Codex/Include Development/Include)
target_link_libraries(AssistantProviderCodex PRIVATE nlohmann_json::nlohmann_json)

add_executable(GameToolsMcpServer
    AssistantProviders/Codex/Source/GameToolsMcpServer.cpp)
target_include_directories(
    GameToolsMcpServer PRIVATE Development/Include Engine/Include)
target_link_libraries(
    GameToolsMcpServer PRIVATE Engine nlohmann_json::nlohmann_json)
target_compile_definitions(GameToolsMcpServer PRIVATE
    MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT="${GAME_PROJECT}")

add_dependencies(AssistantHost AssistantProviderOpenAI AssistantProviderCodex
                 GameToolsMcpServer)
add_custom_command(
    TARGET AssistantHost
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/AssistantHost/Config/settings.json
        $<TARGET_FILE_DIR:AssistantHost>/settings.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/AssistantHost/Config/AssistantPrompts.json
        $<TARGET_FILE_DIR:AssistantHost>/AssistantPrompts.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/AssistantProviders/OpenAI/OpenAIProvider.settings.json
        $<TARGET_FILE_DIR:AssistantHost>/OpenAIProvider.settings.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/AssistantProviders/Codex/CodexProvider.settings.json
        $<TARGET_FILE_DIR:AssistantHost>/CodexProvider.settings.json
)

add_executable(Launcher Launcher/Source/main.cpp)
target_link_libraries(Launcher PRIVATE Engine)
if(WIN32)
    target_link_libraries(Launcher PRIVATE shell32)
endif()

add_executable(GameRelease WIN32
    GameHost/Source/ReleaseMain.cpp
    Engine/Source/Application.cpp
    Engine/Source/ProceduralAudio.cpp
    Engine/Source/CrashDiagnostics.cpp
    Engine/Source/GameInstance.cpp
    Engine/Source/InputSystem.cpp
    Engine/Source/Memory.cpp
    Engine/Source/ObjectManager.cpp
    Engine/Source/ObjectMemory.cpp
    Engine/Source/RenderContext.cpp
    Engine/Source/Renderer2D.cpp
    Engine/Source/VectorShape.cpp
    Engine/Source/Renderer3D.cpp
    Engine/Source/ShaderProgram.cpp
    Engine/Source/UiSystem.cpp
    Engine/Source/WindowFocus.cpp
    Engine/Source/World.cpp
    ${ACTIVE_GAME_RUNTIME_SOURCES}
)
if(WIN32)
    enable_language(RC)
    set(GAME_RELEASE_IDENTITY_RESOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/Generated/GameReleaseIdentity.rc")
    configure_file(
        cmake/GameReleaseIdentity.rc.in
        "${GAME_RELEASE_IDENTITY_RESOURCE}"
        @ONLY
    )
    target_sources(GameRelease PRIVATE
        "$<$<CONFIG:Release>:${GAME_RELEASE_IDENTITY_RESOURCE}>")
endif()
target_include_directories(GameRelease PRIVATE
    Engine/Include
    ${ACTIVE_GAME_DIR}/Include
)
target_link_libraries(GameRelease PRIVATE raylib EngineImGui)
if(WIN32)
    target_link_libraries(
        GameRelease PRIVATE
        "$<$<NOT:$<CONFIG:Release>>:dbghelp>")
    target_compile_definitions(
        GameRelease PRIVATE
        "$<$<NOT:$<CONFIG:Release>>:ENGINE_CRASH_DIAGNOSTICS>")
endif()
target_compile_definitions(GameRelease PRIVATE
    ENGINE_STATIC
    GAME_RELEASE_BUILD
    MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT="${GAME_PROJECT}"
)
set_target_properties(GameRelease PROPERTIES
    OUTPUT_NAME "${GAME_PROJECT}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ACTIVE_GAME_DIR}/Release"
)

foreach(target Engine Game GameHost AssistantHost AssistantProviderOpenAI
               AssistantProviderCodex GameToolsMcpServer Launcher GameRelease)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_library(BaseGameTestModule SHARED
        Games/BaseGame/Source/GameModule.cpp
        Games/BaseGame/Source/Game.cpp
        Games/BaseGame/Source/GameInput.cpp
        Games/BaseGame/Source/GameplayComponents.cpp
    )
    target_include_directories(BaseGameTestModule PRIVATE Games/BaseGame/Include)
    target_link_libraries(BaseGameTestModule PRIVATE Engine)
    target_compile_definitions(BaseGameTestModule PRIVATE GAME_MODULE_BUILD)

    add_executable(EngineTests EXCLUDE_FROM_ALL
        Tests/EngineTests.cpp
    )
    target_link_libraries(EngineTests PRIVATE Engine)

    add_executable(AssistantHostTests EXCLUDE_FROM_ALL
        Tests/AssistantHostTests.cpp
        AssistantHost/Source/AssistantPromptConfig.cpp
        AssistantHost/Source/MidiAttachmentProvider.cpp
        AssistantProviders/Codex/Source/CodexEventParser.cpp
    )
    target_include_directories(
        AssistantHostTests PRIVATE AssistantHost/Include
        AssistantProviders/Codex/Include Development/Include)
    target_link_libraries(
        AssistantHostTests PRIVATE Engine nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(AssistantHostTests PRIVATE shell32)
    endif()

    add_executable(HotReloadTests EXCLUDE_FROM_ALL
        Tests/HotReloadTests.cpp
        GameHost/Source/ReloadableGame.cpp
    )
    target_include_directories(HotReloadTests PRIVATE GameHost/Include)
    target_link_libraries(HotReloadTests PRIVATE Engine)
    add_dependencies(HotReloadTests BaseGameTestModule)

    add_executable(GameToolServiceTests EXCLUDE_FROM_ALL
        Tests/GameToolServiceTests.cpp
        GameHost/Source/GameToolService.cpp
        GameHost/Source/ReloadableGame.cpp
    )
    target_include_directories(
        GameToolServiceTests PRIVATE GameHost/Include Development/Include)
    target_link_libraries(
        GameToolServiceTests PRIVATE Engine nlohmann_json::nlohmann_json)

    foreach(target EngineTests AssistantHostTests HotReloadTests
                   GameToolServiceTests BaseGameTestModule)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_custom_target(
        AutoTests DEPENDS EngineTests AssistantHostTests HotReloadTests
                          GameToolServiceTests GameToolsMcpServer)
    add_test(NAME Engine.Pure COMMAND EngineTests)
    add_test(NAME AssistantHost.Midi COMMAND AssistantHostTests)
    add_test(NAME AssistantProvider.CodexMcp
             COMMAND GameToolsMcpServer --self-test)
    add_test(
        NAME GameTools.CodeFiles
        COMMAND GameToolServiceTests
            $<TARGET_FILE_DIR:GameToolServiceTests>/GameCodeFileToolSandbox)
    add_test(
        NAME Game.HeadlessHotReload
        COMMAND HotReloadTests
            $<TARGET_FILE:BaseGameTestModule>
            $<TARGET_FILE_DIR:HotReloadTests>/AutoTestHotReload)
endif()

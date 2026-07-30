#include "Engine/Application/Application.h"
#include "Engine/Core/CrashDiagnostics.h"
#include "Engine/Core/Logger.h"
#include "GameHost/GameToolService.h"
#include "GameHost/ReloadableGame.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

#ifndef MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR
    #define MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR ""
#endif
#ifndef MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
    #define MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT "UnknownGame"
#endif

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{})
                .parent_path();
        Engine::Logger::Initialize(
            executableDirectory / "Logs" / "GameHost.log");
        Engine::CrashDiagnostics::Install(
            executableDirectory / "Crashes",
            "GameHost");
        Engine::Logger::Info("GameHost started.");
        const bool recoveryMode =
            argc > 1 && std::string_view(argv[1]) == "--recovery";

        const std::filesystem::path activeGameRoot =
            MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR;
        if (!std::filesystem::exists(
                activeGameRoot / "Source" / "GameModule.cpp"))
        {
            throw std::runtime_error(
                "Configured active Game project does not exist: " +
                activeGameRoot.string());
        }
        Engine::Logger::Info(
            "Active Game project: " + activeGameRoot.string());

        if (recoveryMode)
        {
            Engine::Logger::Warning(
                "GameHost entered crash recovery mode; Game.dll will not load.");
            GameToolService tools(
                nullptr,
                activeGameRoot,
                executableDirectory.parent_path(),
                executableDirectory,
                true);
            Engine::Logger::Info(
                "Recovery tools are ready for the AI Assistant.");
            {
                std::ofstream request(
                    executableDirectory / "AIRecovery.prompt",
                    std::ios::binary | std::ios::trunc);
                request
                    << "The Game entered crash recovery mode after a crash. "
                       "Inspect the crash "
                       "diagnostics and process logs, identify the cause in the "
                       "Game source, implement the smallest safe fix, build the "
                       "Game, and call launch_game only if the build succeeds.";
            }
            while (!tools.IsLaunchRequested())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            Engine::Logger::Info(
                "AI Assistant requested a repaired Game launch.");
            return 42;
        }

        ReloadableGame game(
            executableDirectory / "Game.dll",
            executableDirectory / "GameHotReload");

        const Engine::GameApplicationConfig gameConfig =
            game.GetApplicationConfig();
        Engine::ApplicationConfig config{
            .windowWidth = gameConfig.windowWidth,
            .windowHeight = gameConfig.windowHeight,
            .targetFramesPerSecond = gameConfig.targetFramesPerSecond,
            .windowTitle =
                std::string(MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT) + " - Game",
            .focusWindowTitle =
                std::string(MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT) +
                    " - AI Assistant",
            .verticalSync = gameConfig.verticalSync
        };

        Engine::Application application(std::move(config));
        {
            GameToolService tools(
                &game,
                activeGameRoot,
                executableDirectory.parent_path(),
                executableDirectory);
            Engine::Logger::Info("Game tools IPC server is ready.");
            application.Run(game);
        }
        Engine::Logger::Info("Game tools IPC server stopped.");
        game.Unload();
        Engine::Logger::Info("Game DLL unloaded before graphics shutdown.");
        Engine::Logger::Info("GameHost application loop ended.");
    }
    catch (const std::exception& exception)
    {
        Engine::Logger::Error(exception.what());
        std::cerr << "GameHost fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}

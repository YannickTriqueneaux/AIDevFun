#include "Engine/Application/Application.h"
#include "Engine/Core/Logger.h"
#include "GameHost/GameToolService.h"
#include "GameHost/ReloadableGame.h"

#include <exception>
#include <filesystem>
#include <iostream>
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
        Engine::Logger::Info("GameHost started.");

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
        GameToolService tools(
            game,
            activeGameRoot,
            executableDirectory.parent_path());
        Engine::Logger::Info("Game tools IPC server is ready.");
        application.Run(game);
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

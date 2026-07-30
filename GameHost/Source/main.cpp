#include "Engine/Application/Application.h"
#include "GameHost/GameToolService.h"
#include "GameHost/ReloadableGame.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <utility>

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{})
                .parent_path();

        const std::filesystem::path workspaceRoot =
            executableDirectory.parent_path().parent_path();
        ReloadableGame game(
            executableDirectory / "Game.dll",
            executableDirectory / "GameHotReload");

        const Engine::GameApplicationConfig gameConfig =
            game.GetApplicationConfig();
        Engine::ApplicationConfig config{
            .windowWidth = gameConfig.windowWidth,
            .windowHeight = gameConfig.windowHeight,
            .targetFramesPerSecond = gameConfig.targetFramesPerSecond,
            .windowTitle = gameConfig.windowTitle,
            .verticalSync = gameConfig.verticalSync
        };

        Engine::Application application(std::move(config));
        GameToolService tools(
            game,
            workspaceRoot,
            workspaceRoot / "build");
        application.Run(game);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "GameHost fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}

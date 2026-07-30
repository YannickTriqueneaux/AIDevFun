#include "Engine/Application/Application.h"
#include "Engine/Application/GameModule.h"
#include "Engine/Platform/DynamicLibrary.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{})
                .parent_path();

        Engine::DynamicLibrary gameModule(executableDirectory / "Game.dll");

        const auto getGameModuleApi =
            reinterpret_cast<Engine::GetGameModuleApiFunction>(
                gameModule.GetFunction(Engine::GetGameModuleApiFunctionName));
        const Engine::GameModuleApi* gameApi = getGameModuleApi();

        if (gameApi == nullptr ||
            gameApi->apiVersion != Engine::GameModuleApiVersion ||
            gameApi->getApplicationConfig == nullptr ||
            gameApi->createGame == nullptr ||
            gameApi->destroyGame == nullptr)
        {
            throw std::runtime_error("The Game module API is incompatible.");
        }

        using GamePointer = std::unique_ptr<
            Engine::GameInterface,
            void (*)(Engine::GameInterface*)>;
        GamePointer game(gameApi->createGame(), gameApi->destroyGame);

        const Engine::GameApplicationConfig gameConfig =
            gameApi->getApplicationConfig();
        Engine::ApplicationConfig config{
            .windowWidth = gameConfig.windowWidth,
            .windowHeight = gameConfig.windowHeight,
            .targetFramesPerSecond = gameConfig.targetFramesPerSecond,
            .windowTitle = gameConfig.windowTitle,
            .verticalSync = gameConfig.verticalSync
        };

        Engine::Application application(std::move(config));
        application.Run(*game);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "GameHost fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}


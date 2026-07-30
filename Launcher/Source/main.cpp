#include "Engine/Application/Application.h"
#include "Engine/Application/GameModule.h"
#include "Engine/Core/Settings.h"
#include "Engine/Platform/DynamicLibrary.h"

#include <filesystem>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

int main()
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                std::filesystem::path(__argv[0])).parent_path();
        const Engine::LauncherSettings settings = Engine::Settings::Load(
            executableDirectory / "settings.json");

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
            .verticalSync = gameConfig.verticalSync,
            .openAI = settings.openAI
        };

        Engine::Application application(std::move(config));
        application.Run(*game);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}

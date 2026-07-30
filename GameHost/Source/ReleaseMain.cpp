#include "Engine/Application/Application.h"

#include "Game/Game.h"
#include "Game/GameConfig.h"

#include <string>
#include <utility>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

#ifndef MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
    #define MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT "Game"
#endif

namespace
{
    int RunReleasedGame()
    {
        const Engine::GameApplicationConfig gameConfig =
            GameConfig::CreateApplicationConfig();
        Engine::ApplicationConfig applicationConfig{
            .windowWidth = gameConfig.windowWidth,
            .windowHeight = gameConfig.windowHeight,
            .targetFramesPerSecond = gameConfig.targetFramesPerSecond,
            .windowTitle = MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT,
            .focusWindowTitle = {},
            .verticalSync = gameConfig.verticalSync
        };

        ProceduralGame game;
        Engine::Application application(std::move(applicationConfig));
        application.Run(game);
        return 0;
    }
}

#if defined(_WIN32)
int WINAPI WinMain(
    HINSTANCE,
    HINSTANCE,
    LPSTR,
    int)
{
    return RunReleasedGame();
}
#else
int main()
{
    return RunReleasedGame();
}
#endif

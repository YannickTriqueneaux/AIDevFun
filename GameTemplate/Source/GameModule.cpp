#include "Game/Game.h"
#include "Game/GameConfig.h"

#include "Engine/Application/GameModule.h"

#if defined(_WIN32)
    #define GAME_MODULE_API extern "C" __declspec(dllexport)
#else
    #define GAME_MODULE_API extern "C"
#endif

namespace
{
    Engine::GameInterface* CreateGame()
    {
        return new ProceduralGame();
    }

    void DestroyGame(Engine::GameInterface* game)
    {
        delete game;
    }

    Engine::GameApplicationConfig GetApplicationConfig()
    {
        return GameConfig::CreateApplicationConfig();
    }
}

GAME_MODULE_API const Engine::GameModuleApi* GetGameModuleApi()
{
    static const Engine::GameModuleApi api{
        .apiVersion = Engine::GameModuleApiVersion,
        .getApplicationConfig = &GetApplicationConfig,
        .createGame = &CreateGame,
        .destroyGame = &DestroyGame
    };
    return &api;
}

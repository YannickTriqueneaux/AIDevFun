#pragma once

#include "Engine/Application/GameInterface.h"

#include <cstdint>

namespace Engine
{
    inline constexpr std::uint32_t GameModuleApiVersion = 1;
    inline constexpr const char* GetGameModuleApiFunctionName = "GetGameModuleApi";

    struct GameApplicationConfig
    {
        int windowWidth = 1280;
        int windowHeight = 720;
        int targetFramesPerSecond = 144;
        const char* windowTitle = "Game";
        bool verticalSync = true;
    };

    struct GameModuleApi
    {
        std::uint32_t apiVersion = 0;
        GameApplicationConfig (*getApplicationConfig)() = nullptr;
        GameInterface* (*createGame)() = nullptr;
        void (*destroyGame)(GameInterface*) = nullptr;
    };

    using GetGameModuleApiFunction = const GameModuleApi* (*)();
}

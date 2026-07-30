#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig
{
    inline constexpr int PlayAreaWidth = 1280;
    inline constexpr int PlayAreaHeight = 720;
    inline constexpr int PlayAreaDepth = 720;
    inline constexpr int TargetFramesPerSecond = 144;
    inline constexpr const char* WindowTitle = "Lightweight 3D Procedural Game";

    inline Engine::GameApplicationConfig CreateApplicationConfig()
    {
        return {
            .windowWidth = PlayAreaWidth,
            .windowHeight = PlayAreaHeight,
            .targetFramesPerSecond = TargetFramesPerSecond,
            .windowTitle = WindowTitle,
            .verticalSync = true
        };
    }

    inline constexpr Engine::Color BackgroundColor{12, 15, 25, 255};
    inline constexpr Engine::Color GridColor{34, 41, 62, 255};
    inline constexpr int GridSize = 80;
}

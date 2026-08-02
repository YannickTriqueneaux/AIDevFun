#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig {
inline constexpr int PlayAreaWidth = 1280;
inline constexpr int PlayAreaHeight = 720;
inline constexpr int WorldWidth = 4096;
inline constexpr int WorldHeight = 1280;
inline constexpr int TargetFramesPerSecond = 144;
inline constexpr const char *WindowTitle = "Mini RPG Retro";

inline Engine::GameApplicationConfig CreateApplicationConfig() {
  return {.windowWidth = PlayAreaWidth,
          .windowHeight = PlayAreaHeight,
          .targetFramesPerSecond = TargetFramesPerSecond,
          .windowTitle = WindowTitle,
          .verticalSync = true};
}

inline constexpr Engine::Color BackgroundColor{139, 172, 15, 255};
inline constexpr Engine::Color GridColor{122, 154, 15, 255};
inline constexpr int GridSize = 32;
} // namespace GameConfig

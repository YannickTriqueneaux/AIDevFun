#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig {
inline constexpr int PlayAreaWidth = 1280;
inline constexpr int PlayAreaHeight = 720;
inline constexpr int WorldWidth = PlayAreaWidth * 4;
inline constexpr int WorldHeight = PlayAreaHeight;
inline constexpr int TargetFramesPerSecond = 144;
inline constexpr const char *WindowTitle = "Pocket Town";

inline Engine::GameApplicationConfig CreateApplicationConfig() {
  return {.windowWidth = PlayAreaWidth,
          .windowHeight = PlayAreaHeight,
          .targetFramesPerSecond = TargetFramesPerSecond,
          .windowTitle = WindowTitle,
          .verticalSync = true};
}

inline constexpr Engine::Color BackgroundColor{116, 196, 149, 255};
inline constexpr int TileSize = 32;
} // namespace GameConfig

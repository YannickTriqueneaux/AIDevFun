#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig {
inline constexpr int PlayAreaWidth = 1280;
inline constexpr int PlayAreaHeight = 720;
inline constexpr int TargetFramesPerSecond = 144;
inline constexpr const char *WindowTitle = "Little Lake Town";

inline Engine::GameApplicationConfig CreateApplicationConfig() {
  return {.windowWidth = PlayAreaWidth,
          .windowHeight = PlayAreaHeight,
          .targetFramesPerSecond = TargetFramesPerSecond,
          .windowTitle = WindowTitle,
          .verticalSync = true};
}

inline constexpr Engine::Color BackgroundColor{88, 176, 118, 255};
inline constexpr Engine::Color GridColor{104, 190, 128, 255};
inline constexpr int GridSize = 64;
} // namespace GameConfig

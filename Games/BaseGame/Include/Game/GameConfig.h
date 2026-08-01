#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig {
inline constexpr int PlayAreaWidth = 1280;
inline constexpr int PlayAreaHeight = 720;
inline constexpr int TargetFramesPerSecond = 144;
inline constexpr const char *WindowTitle = "Entity Arena";

inline Engine::GameApplicationConfig CreateApplicationConfig() {
  return {.windowWidth = PlayAreaWidth,
          .windowHeight = PlayAreaHeight,
          .targetFramesPerSecond = TargetFramesPerSecond,
          .windowTitle = WindowTitle,
          .verticalSync = true};
}

inline constexpr Engine::Color BackgroundColor{15, 18, 28, 255};
inline constexpr Engine::Color GridColor{31, 36, 51, 255};
inline constexpr int GridSize = 64;
} // namespace GameConfig

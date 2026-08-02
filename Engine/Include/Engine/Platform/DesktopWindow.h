#pragma once

#include "Engine/Core/Export.h"

#include <string>

namespace Engine {

struct DesktopWindowConfig {
  int width = 720;
  int height = 850;
  int targetFramesPerSecond = 60;
  std::string title;
  bool resizable = true;
  bool verticalSync = true;
};

class ENGINE_API DesktopWindow {
public:
  explicit DesktopWindow(DesktopWindowConfig config);
  ~DesktopWindow();

  DesktopWindow(const DesktopWindow &) = delete;
  DesktopWindow &operator=(const DesktopWindow &) = delete;

  [[nodiscard]] bool ShouldClose() const;
  [[nodiscard]] bool IsTabPressed() const;
};

} // namespace Engine

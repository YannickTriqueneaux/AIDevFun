#include "Engine/Platform/DesktopWindow.h"

#include "raylib.h"

#include <utility>

namespace Engine {

DesktopWindow::DesktopWindow(DesktopWindowConfig config) {
  unsigned int flags = 0;
  if (config.resizable)
    flags |= FLAG_WINDOW_RESIZABLE;
  if (config.verticalSync)
    flags |= FLAG_VSYNC_HINT;
  if (flags != 0)
    SetConfigFlags(flags);

  InitWindow(config.width, config.height, config.title.c_str());
  SetTargetFPS(config.targetFramesPerSecond);
}

DesktopWindow::~DesktopWindow() {
  if (IsWindowReady())
    CloseWindow();
}

bool DesktopWindow::ShouldClose() const { return WindowShouldClose(); }

bool DesktopWindow::IsTabPressed() const { return IsKeyPressed(KEY_TAB); }

} // namespace Engine

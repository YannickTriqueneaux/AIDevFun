#include "Engine/Application/Application.h"

#include "Engine/Application/GameInterface.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/WindowFocus.h"
#include "Engine/UI/UiSystem.h"

#include "raylib.h"

#include <utility>

namespace Engine {
Application::Application(ApplicationConfig config)
    : config_(std::move(config)) {
  if (config_.verticalSync) {
    SetConfigFlags(FLAG_VSYNC_HINT);
  }

  InitWindow(config_.windowWidth, config_.windowHeight,
             config_.windowTitle.c_str());
  SetTargetFPS(config_.targetFramesPerSecond);
}

Application::~Application() { CloseWindow(); }

void Application::Run(GameInterface &game) {
  InputSystem input;
  RenderContext renderContext;
  UiSystem ui;

  game.Initialize();

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_TAB)) {
      static_cast<void>(
          WindowFocus::FocusWindowByTitle(config_.focusWindowTitle));
    }

    input.Update();
    game.Update(input, GetFrameTime());

    renderContext.BeginFrame(game.GetClearColor());
    game.Render(renderContext);

    ui.BeginFrame();
    game.RenderUi(ui);
    ui.EndFrame();

    renderContext.EndFrame();
  }

  game.Shutdown();
}
} // namespace Engine

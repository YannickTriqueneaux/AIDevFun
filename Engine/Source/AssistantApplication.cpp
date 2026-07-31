#include "Engine/Application/AssistantApplication.h"

#include "Engine/AI/OpenAISettings.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Platform/WindowFocus.h"
#include "Engine/UI/PromptConsole.h"
#include "Engine/UI/UiSystem.h"

#include "raylib.h"

#include <filesystem>
#include <utility>

namespace {
constexpr int AssistantWindowWidth = 720;
constexpr int AssistantWindowHeight = 850;
} // namespace

namespace Engine {
struct AssistantApplication::Implementation {
  OpenAISettings settings;
  std::string gameWindowTitle;
};

AssistantApplication::AssistantApplication(OpenAISettings settings,
                                           std::string gameName)
    : implementation_(
          new Implementation{std::move(settings), gameName + " - Game"}) {
  const std::string assistantWindowTitle = gameName + " - AI Assistant";
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(AssistantWindowWidth, AssistantWindowHeight,
             assistantWindowTitle.c_str());
  SetTargetFPS(60);
}

AssistantApplication::~AssistantApplication() {
  CloseWindow();
  delete implementation_;
}

void AssistantApplication::Run() {
  RenderContext renderContext;
  UiSystem ui;
  PromptConsole promptConsole(
      implementation_->settings,
      {.collapsible = false,
       .expandedByDefault = true,
       .fillWindow = true,
       .automaticPromptFile =
           std::filesystem::current_path() / "AIRecovery.prompt"});

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_TAB)) {
      static_cast<void>(
          WindowFocus::FocusWindowByTitle(implementation_->gameWindowTitle));
    }

    renderContext.BeginFrame({12, 14, 20, 255});

    ui.BeginFrame();
    promptConsole.Render(ui, renderContext.GetWidth(),
                         renderContext.GetHeight());
    ui.EndFrame();

    renderContext.EndFrame();
  }
}
} // namespace Engine

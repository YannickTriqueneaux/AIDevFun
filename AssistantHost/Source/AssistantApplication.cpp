#include "AssistantHost/AssistantApplication.h"

#include "AssistantHost/AssistantProviderLoader.h"
#include "AssistantHost/PromptConsole.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Platform/DesktopWindow.h"
#include "Engine/Platform/WindowFocus.h"
#include "Engine/UI/UiSystem.h"

#include <filesystem>
#include <utility>

namespace {
constexpr int AssistantWindowWidth = 720;
constexpr int AssistantWindowHeight = 850;
} // namespace

namespace AssistantHost {
struct AssistantApplication::Implementation {
  std::string gameWindowTitle;
  PromptAttachmentProvider *attachmentProvider = nullptr;
  Engine::DesktopWindow window;
  AssistantProviderLoader provider;
};

AssistantApplication::AssistantApplication(
    LauncherSettings settings, std::string gameName,
    std::filesystem::path executableDirectory, std::filesystem::path gameRoot,
    PromptAttachmentProvider *attachmentProvider)
    : implementation_(new Implementation{
          gameName + " - Game", attachmentProvider,
          Engine::DesktopWindow({.width = AssistantWindowWidth,
                                 .height = AssistantWindowHeight,
                                 .targetFramesPerSecond = 60,
                                 .title = gameName + " - AI Assistant",
                                 .resizable = true,
                                 .verticalSync = true}),
          AssistantProviderLoader(
              executableDirectory / settings.providerLibrary,
              executableDirectory / settings.providerSettings, gameRoot)}) {}

AssistantApplication::~AssistantApplication() { delete implementation_; }

void AssistantApplication::Run() {
  Engine::RenderContext renderContext;
  Engine::UiSystem ui;
  PromptConsole promptConsole(
      implementation_->provider.Get(),
      {.collapsible = false,
       .expandedByDefault = true,
       .fillWindow = true,
       .automaticPromptFile =
           std::filesystem::current_path() / "AIRecovery.prompt"},
      implementation_->attachmentProvider);

  while (!implementation_->window.ShouldClose()) {
    if (implementation_->window.IsTabPressed()) {
      static_cast<void>(Engine::WindowFocus::FocusWindowByTitle(
          implementation_->gameWindowTitle));
    }

    renderContext.BeginFrame({12, 14, 20, 255});

    ui.BeginFrame();
    promptConsole.Render(ui, renderContext.GetWidth(),
                         renderContext.GetHeight());
    ui.EndFrame();

    renderContext.EndFrame();
  }
}
} // namespace AssistantHost

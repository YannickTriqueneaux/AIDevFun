#include "Engine/Application/AssistantApplication.h"

#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Platform/WindowFocus.h"
#include "Engine/UI/PromptConsole.h"
#include "Engine/UI/UiSystem.h"

#include "raylib.h"

#include <utility>

namespace
{
    constexpr int AssistantWindowWidth = 720;
    constexpr int AssistantWindowHeight = 850;
}

namespace Engine
{
    AssistantApplication::AssistantApplication(OpenAISettings settings)
        : settings_(std::move(settings))
    {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(
            AssistantWindowWidth,
            AssistantWindowHeight,
            "Game AI Assistant");
        SetTargetFPS(60);
    }

    AssistantApplication::~AssistantApplication()
    {
        CloseWindow();
    }

    void AssistantApplication::Run()
    {
        Renderer2D renderer;
        UiSystem ui;
        PromptConsole promptConsole(
            settings_,
            {
                .collapsible = false,
                .expandedByDefault = true,
                .fillWindow = true
            });

        while (!WindowShouldClose())
        {
            if (IsKeyPressed(KEY_TAB))
            {
                static_cast<void>(
                    WindowFocus::FocusProcessWindow("GameHost.exe"));
            }

            renderer.BeginFrame({12, 14, 20, 255});

            ui.BeginFrame();
            promptConsole.Render(
                ui,
                renderer.GetWidth(),
                renderer.GetHeight());
            ui.EndFrame();

            renderer.EndFrame();
        }
    }
}

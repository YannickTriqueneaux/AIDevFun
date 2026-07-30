#include "Engine/Application/Application.h"

#include "Engine/Application/GameInterface.h"
#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Input/InputSystem.h"

#include "raylib.h"

#include <utility>

namespace Engine
{
    Application::Application(ApplicationConfig config)
        : config_(std::move(config))
    {
        if (config_.verticalSync)
        {
            SetConfigFlags(FLAG_VSYNC_HINT);
        }

        InitWindow(
            config_.windowWidth,
            config_.windowHeight,
            config_.windowTitle.c_str());
        SetTargetFPS(config_.targetFramesPerSecond);
    }

    Application::~Application()
    {
        CloseWindow();
    }

    void Application::Run(GameInterface& game)
    {
        InputSystem input;
        Renderer2D renderer;

        game.Initialize();

        while (!WindowShouldClose())
        {
            input.Update();
            game.Update(input, GetFrameTime());
            game.Render(renderer);
        }

        game.Shutdown();
    }
}


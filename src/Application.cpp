#include "Application.h"

#include "Config.h"
#include "raylib.h"

Application::Application()
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(Config::ScreenWidth, Config::ScreenHeight, Config::WindowTitle);
    SetTargetFPS(Config::TargetFramesPerSecond);
}

Application::~Application()
{
    CloseWindow();
}

void Application::Run()
{
    while (!WindowShouldClose())
    {
        game_.Update(GetFrameTime());

        BeginDrawing();
        game_.Draw();
        EndDrawing();
    }
}


#include "Game.h"

#include "raylib.h"

void Game::Update(float deltaTime)
{
    player_.Update(input_.Poll(), deltaTime);
}

void Game::Draw() const
{
    ClearBackground({15, 18, 28, 255});

    constexpr int GridSize = 64;
    for (int x = 0; x < GetScreenWidth(); x += GridSize)
    {
        DrawLine(x, 0, x, GetScreenHeight(), {31, 36, 51, 255});
    }
    for (int y = 0; y < GetScreenHeight(); y += GridSize)
    {
        DrawLine(0, y, GetScreenWidth(), y, {31, 36, 51, 255});
    }

    player_.Draw();
    player_.DrawHud();
    DrawFPS(GetScreenWidth() - 100, 20);
}


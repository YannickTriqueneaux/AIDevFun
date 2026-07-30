#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Input/InputSystem.h"

void ProceduralGame::Update(
    const Engine::InputSystem& input,
    float deltaTime)
{
    player_.Update(inputBindings_.BuildPlayerCommand(input), deltaTime);
}

void ProceduralGame::Render(Engine::Renderer2D& renderer) const
{
    renderer.BeginFrame(GameConfig::BackgroundColor);

    for (int x = 0; x < renderer.GetWidth(); x += GameConfig::GridSize)
    {
        renderer.DrawLine(
            {static_cast<float>(x), 0.0f},
            {static_cast<float>(x), static_cast<float>(renderer.GetHeight())},
            1.0f,
            GameConfig::GridColor);
    }

    for (int y = 0; y < renderer.GetHeight(); y += GameConfig::GridSize)
    {
        renderer.DrawLine(
            {0.0f, static_cast<float>(y)},
            {static_cast<float>(renderer.GetWidth()), static_cast<float>(y)},
            1.0f,
            GameConfig::GridColor);
    }

    player_.Render(renderer);
    player_.RenderHud(renderer);
    renderer.DrawFramesPerSecond(renderer.GetWidth() - 100, 20);

    renderer.EndFrame();
}


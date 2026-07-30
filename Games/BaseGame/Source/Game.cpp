#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"

void ProceduralGame::Update(
    const Engine::InputSystem& input,
    float deltaTime)
{
    player_.Update(inputBindings_.BuildPlayerCommand(input), deltaTime);
}

Engine::Color ProceduralGame::GetClearColor() const
{
    return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext& context) const
{
    Engine::Renderer2D& renderer = context.Draw2D();
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
#if !defined(GAME_RELEASE_BUILD)
    renderer.DrawFramesPerSecond(renderer.GetWidth() - 100, 20);
#endif
}

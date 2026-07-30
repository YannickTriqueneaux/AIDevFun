#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/RenderContext.h"
#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/Vector2.h"

namespace
{
    Engine::Vector2 ProjectFloorPoint(
        float x,
        float z,
        const Engine::Renderer2D& renderer)
    {
        const float centeredX = x - static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f;
        const float perspective = 480.0f / (z + 480.0f);
        return {
            static_cast<float>(renderer.GetWidth()) * 0.5f + centeredX * perspective,
            static_cast<float>(renderer.GetHeight()) * 0.82f - z * 0.46f * perspective
        };
    }

    void DrawFloorGrid(Engine::Renderer2D& renderer)
    {
        renderer.DrawLine(
            {0.0f, static_cast<float>(renderer.GetHeight()) * 0.34f},
            {static_cast<float>(renderer.GetWidth()), static_cast<float>(renderer.GetHeight()) * 0.34f},
            2.0f,
            {44, 50, 70, 255});

        for (int x = 0; x <= GameConfig::PlayAreaWidth; x += GameConfig::GridSize)
        {
            renderer.DrawLine(
                ProjectFloorPoint(static_cast<float>(x), 0.0f, renderer),
                ProjectFloorPoint(static_cast<float>(x), static_cast<float>(GameConfig::PlayAreaDepth), renderer),
                1.0f,
                GameConfig::GridColor);
        }

        for (int z = 0; z <= GameConfig::PlayAreaDepth; z += GameConfig::GridSize)
        {
            renderer.DrawLine(
                ProjectFloorPoint(0.0f, static_cast<float>(z), renderer),
                ProjectFloorPoint(static_cast<float>(GameConfig::PlayAreaWidth), static_cast<float>(z), renderer),
                1.0f,
                GameConfig::GridColor);
        }
    }
}

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
    DrawFloorGrid(renderer);

    player_.Render(renderer);
    player_.RenderHud(renderer);
#if !defined(GAME_RELEASE_BUILD)
    renderer.DrawFramesPerSecond(renderer.GetWidth() - 100, 20);
#endif
}

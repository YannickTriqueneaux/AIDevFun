#pragma once

#include "Game/GameInput.h"

#include "Engine/Math/Vector2.h"
#include "Engine/Serialization/Serializer.h"

namespace Engine
{
    class Renderer2D;
}

class Player final : public Engine::Serializable
{
public:
    Player();

    void Update(const PlayerCommand& command, float deltaTime);
    void Render(Engine::Renderer2D& renderer) const;
    void RenderHud(Engine::Renderer2D& renderer) const;
    void Serialize(Engine::Serializer& serializer) override;

private:
    void ExecuteAction(PlayerAction action);
    void KeepInsidePlayArea();
    void Reset();

    Engine::Vector2 position_{};
    Engine::Vector2 facing_{1.0f, 0.0f};
    float movementSpeed_ = 260.0f;
    float pulseTimeRemaining_ = 0.0f;
    bool shieldEnabled_ = false;
    const char* lastAction_ = "None";
};


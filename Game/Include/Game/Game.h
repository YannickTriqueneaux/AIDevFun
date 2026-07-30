#pragma once

#include "Engine/Application/GameInterface.h"

#include "Game/GameInput.h"
#include "Game/Player.h"

class ProceduralGame final : public Engine::GameInterface
{
public:
    void Update(const Engine::InputSystem& input, float deltaTime) override;
    void Render(Engine::Renderer2D& renderer) const override;

private:
    GameInput inputBindings_;
    Player player_;
};


#pragma once

#include "Game/GameInput.h"

#include "Engine/Math/Vector2.h"
#include "Engine/Gameplay/Object.h"

namespace Engine
{
    class Renderer2D;
}

class Player final : public Engine::Gameplay::Component
{
public:
    static constexpr Engine::Gameplay::TypeID Type = Engine::Gameplay::StableTypeID("BaseGame.PlayerComponent");
    static constexpr std::uint32_t CurrentVersion = 1;
    static constexpr std::uint32_t MinimumVersion = 1;
    explicit Player(Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> owner);

    [[nodiscard]] Engine::Gameplay::TypeID GetTypeID() const override{return Type;}
    [[nodiscard]] std::uint32_t CurrentStateVersion() const override{return CurrentVersion;}
    [[nodiscard]] std::uint32_t MinimumStateVersion() const override{return MinimumVersion;}
    void SetCommand(PlayerCommand command){command_=command;}
    void Update(Engine::Gameplay::ObjectManager& objects,float deltaTime) override;
    void Render(Engine::Renderer2D& renderer,const Engine::Gameplay::ObjectManager& objects) const;
    void RenderHud(Engine::Renderer2D& renderer) const;
    void SaveState(Engine::Gameplay::StateWriter& writer) const override;
    void LoadState(Engine::Gameplay::StateReader& reader,std::uint32_t version) override;

private:
    void ExecuteAction(PlayerAction action);
    void KeepInsidePlayArea();
    void Reset();

    Engine::Vector2 facing_{1.0f, 0.0f};
    float movementSpeed_ = 260.0f;
    float pulseTimeRemaining_ = 0.0f;
    bool shieldEnabled_ = false;
    const char* lastAction_ = "None";
    PlayerCommand command_{};
};


#include "Game/Player.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Gameplay/ObjectManager.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
    constexpr float PlayerHalfSize = 24.0f;
    constexpr float DashDistance = 100.0f;
    constexpr float PulseDuration = 0.35f;

    void DrawSquare(
        Engine::Renderer2D& renderer,
        Engine::Vector2 center,
        float halfSize,
        Engine::Color color)
    {
        const float left = center.x - halfSize;
        const float right = center.x + halfSize;
        const float top = center.y - halfSize;
        const float bottom = center.y + halfSize;

        for (float y = top; y <= bottom; y += 2.0f)
        {
            renderer.DrawLine({left, y}, {right, y}, 2.0f, color);
        }
    }

    void DrawSquareOutline(
        Engine::Renderer2D& renderer,
        Engine::Vector2 center,
        float halfSize,
        Engine::Color color)
    {
        const float left = center.x - halfSize;
        const float right = center.x + halfSize;
        const float top = center.y - halfSize;
        const float bottom = center.y + halfSize;

        renderer.DrawLine({left, top}, {right, top}, 2.0f, color);
        renderer.DrawLine({right, top}, {right, bottom}, 2.0f, color);
        renderer.DrawLine({right, bottom}, {left, bottom}, 2.0f, color);
        renderer.DrawLine({left, bottom}, {left, top}, 2.0f, color);
    }
}

Player::Player(Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner)
{
    Reset();
}

void Player::Update(Engine::Gameplay::ObjectManager& objects, float deltaTime)
{
    auto* entity = GetOwner().Resolve(objects);
    if (entity == nullptr) return;
    auto& position = entity->transform.position;
    const PlayerCommand command = command_;
    command_ = {};
    if (command.movement.x != 0.0f || command.movement.y != 0.0f)
    {
        facing_ = command.movement;
    }

    position += command.movement * (movementSpeed_ * deltaTime);

    if (command.action == PlayerAction::Dash) { position += facing_ * DashDistance; ExecuteAction(command.action); }
    else if(command.action==PlayerAction::Reset){Reset();position={static_cast<float>(GameConfig::PlayAreaWidth)*0.5f,static_cast<float>(GameConfig::PlayAreaHeight)*0.5f};lastAction_="Reset";}
    else ExecuteAction(command.action);
    pulseTimeRemaining_ = std::max(0.0f, pulseTimeRemaining_ - deltaTime);
    position.x = std::clamp(position.x, PlayerHalfSize, static_cast<float>(GameConfig::PlayAreaWidth)-PlayerHalfSize);
    position.y = std::clamp(position.y, PlayerHalfSize, static_cast<float>(GameConfig::PlayAreaHeight)-PlayerHalfSize);
}

void Player::Render(Engine::Renderer2D& renderer,const Engine::Gameplay::ObjectManager& objects) const
{
    const auto* entity=GetOwner().Resolve(objects); if(entity==nullptr)return; const auto position=entity->transform.position;
    if (pulseTimeRemaining_ > 0.0f)
    {
        const float progress = 1.0f - pulseTimeRemaining_ / PulseDuration;
        const float halfSize = PlayerHalfSize + progress * 90.0f;
        const auto alpha =
            static_cast<std::uint8_t>((1.0f - progress) * 220.0f);
        DrawSquareOutline(
            renderer,
            position,
            halfSize,
            {255, 184, 77, alpha});
    }

    if (shieldEnabled_)
    {
        DrawSquareOutline(
            renderer,
            position,
            PlayerHalfSize + 10.0f,
            {102, 191, 255, 255});
        DrawSquareOutline(
            renderer,
            position,
            PlayerHalfSize + 12.0f,
            {0, 121, 241, 255});
    }

    DrawSquare(renderer, position, PlayerHalfSize, {86, 204, 157, 255});
    renderer.DrawLine(
        position,
        position + facing_ * 36.0f,
        5.0f,
        {245, 245, 245, 255});
}

void Player::RenderHud(Engine::Renderer2D& renderer) const
{
    renderer.DrawText(
        "MOVE: ARROW KEYS",
        {24.0f, 22.0f},
        20,
        {200, 200, 200, 255});
    renderer.DrawText(
        "Q: DASH   W: PULSE   E: SHIELD   R: RESET",
        {24.0f, 50.0f},
        20,
        {200, 200, 200, 255});
    renderer.DrawText(
        std::string("LAST ACTION: ") + lastAction_,
        {24.0f, 84.0f},
        20,
        {255, 203, 0, 255});
}

void Player::SaveState(Engine::Gameplay::StateWriter& writer) const
{
    writer.Value(facing_); writer.Value(movementSpeed_); writer.Value(pulseTimeRemaining_); writer.Value(shieldEnabled_);
}

void Player::LoadState(Engine::Gameplay::StateReader& reader,std::uint32_t version)
{
    if(version!=1) throw std::runtime_error("Unsupported Player state version.");
    facing_=reader.Value<Engine::Vector2>(); movementSpeed_=reader.Value<float>(); pulseTimeRemaining_=reader.Value<float>(); shieldEnabled_=reader.Value<bool>();
}

void Player::ExecuteAction(PlayerAction action)
{
    switch (action)
    {
    case PlayerAction::Dash:
        lastAction_ = "Dash";
        break;
    case PlayerAction::Pulse:
        pulseTimeRemaining_ = PulseDuration;
        lastAction_ = "Pulse";
        break;
    case PlayerAction::ToggleShield:
        shieldEnabled_ = !shieldEnabled_;
        lastAction_ = shieldEnabled_ ? "Shield On" : "Shield Off";
        break;
    case PlayerAction::Reset:
        Reset();
        lastAction_ = "Reset";
        break;
    case PlayerAction::None:
        break;
    }
}

void Player::KeepInsidePlayArea()
{
}

void Player::Reset()
{
    facing_ = {1.0f, 0.0f};
    pulseTimeRemaining_ = 0.0f;
    shieldEnabled_ = false;
}

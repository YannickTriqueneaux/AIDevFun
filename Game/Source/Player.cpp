#include "Game/Player.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/Renderer2D.h"

#include <algorithm>
#include <string>

namespace
{
    constexpr float PlayerRadius = 24.0f;
    constexpr float DashDistance = 100.0f;
    constexpr float PulseDuration = 0.35f;
}

Player::Player()
{
    Reset();
}

void Player::Update(const PlayerCommand& command, float deltaTime)
{
    if (command.movement.x != 0.0f || command.movement.y != 0.0f)
    {
        facing_ = command.movement;
    }

    position_ += command.movement * (movementSpeed_ * deltaTime);

    ExecuteAction(command.action);
    pulseTimeRemaining_ = std::max(0.0f, pulseTimeRemaining_ - deltaTime);
    KeepInsidePlayArea();
}

void Player::Render(Engine::Renderer2D& renderer) const
{
    if (pulseTimeRemaining_ > 0.0f)
    {
        const float progress = 1.0f - pulseTimeRemaining_ / PulseDuration;
        const float radius = PlayerRadius + progress * 90.0f;
        const auto alpha =
            static_cast<std::uint8_t>((1.0f - progress) * 220.0f);
        renderer.DrawCircleOutline(
            position_,
            radius,
            {255, 184, 77, alpha});
    }

    if (shieldEnabled_)
    {
        renderer.DrawCircleOutline(position_, PlayerRadius + 10.0f, {102, 191, 255, 255});
        renderer.DrawCircleOutline(position_, PlayerRadius + 12.0f, {0, 121, 241, 255});
    }

    renderer.DrawCircle(position_, PlayerRadius, {86, 204, 157, 255});
    renderer.DrawLine(
        position_,
        position_ + facing_ * 36.0f,
        5.0f,
        {245, 245, 245, 255});
}

void Player::RenderHud(Engine::Renderer2D& renderer) const
{
    renderer.DrawText("MOVE: ARROW KEYS", {24.0f, 22.0f}, 20, {200, 200, 200, 255});
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

void Player::Serialize(Engine::Serializer& serializer)
{
    serializer.Value("position.x", position_.x);
    serializer.Value("position.y", position_.y);
    serializer.Value("facing.x", facing_.x);
    serializer.Value("facing.y", facing_.y);
    serializer.Value("movementSpeed", movementSpeed_);
    serializer.Value("shieldEnabled", shieldEnabled_);
}

void Player::ExecuteAction(PlayerAction action)
{
    switch (action)
    {
    case PlayerAction::Dash:
        position_ += facing_ * DashDistance;
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
    position_.x = std::clamp(
        position_.x,
        PlayerRadius,
        static_cast<float>(GameConfig::PlayAreaWidth) - PlayerRadius);
    position_.y = std::clamp(
        position_.y,
        PlayerRadius,
        static_cast<float>(GameConfig::PlayAreaHeight) - PlayerRadius);
}

void Player::Reset()
{
    position_ = {
        static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
        static_cast<float>(GameConfig::PlayAreaHeight) * 0.5f
    };
    facing_ = {1.0f, 0.0f};
    pulseTimeRemaining_ = 0.0f;
    shieldEnabled_ = false;
}

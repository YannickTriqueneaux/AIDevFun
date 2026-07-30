#include "Game/Player.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/Renderer2D.h"

#include <algorithm>
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
        const float halfSize = PlayerHalfSize + progress * 90.0f;
        const auto alpha =
            static_cast<std::uint8_t>((1.0f - progress) * 220.0f);
        DrawSquareOutline(
            renderer,
            position_,
            halfSize,
            {255, 184, 77, alpha});
    }

    if (shieldEnabled_)
    {
        DrawSquareOutline(renderer, position_, PlayerHalfSize + 10.0f, {102, 191, 255, 255});
        DrawSquareOutline(renderer, position_, PlayerHalfSize + 12.0f, {0, 121, 241, 255});
    }

    DrawSquare(renderer, position_, PlayerHalfSize, {86, 204, 157, 255});
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
        PlayerHalfSize,
        static_cast<float>(GameConfig::PlayAreaWidth) - PlayerHalfSize);
    position_.y = std::clamp(
        position_.y,
        PlayerHalfSize,
        static_cast<float>(GameConfig::PlayAreaHeight) - PlayerHalfSize);
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

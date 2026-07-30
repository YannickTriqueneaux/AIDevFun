#include "Player.h"

#include "Config.h"

#include <algorithm>

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

void Player::Update(const InputState& input, float deltaTime)
{
    if (input.movement.x != 0.0f || input.movement.y != 0.0f)
    {
        facing_ = input.movement;
    }

    position_.x += input.movement.x * movementSpeed_ * deltaTime;
    position_.y += input.movement.y * movementSpeed_ * deltaTime;

    ExecuteAction(input.action);
    pulseTimeRemaining_ = std::max(0.0f, pulseTimeRemaining_ - deltaTime);
    KeepInsidePlayArea();
}

void Player::Draw() const
{
    if (pulseTimeRemaining_ > 0.0f)
    {
        const float progress = 1.0f - pulseTimeRemaining_ / PulseDuration;
        const float radius = PlayerRadius + progress * 90.0f;
        const unsigned char alpha =
            static_cast<unsigned char>((1.0f - progress) * 220.0f);
        DrawCircleLinesV(position_, radius, {255, 184, 77, alpha});
    }

    if (shieldEnabled_)
    {
        DrawCircleLinesV(position_, PlayerRadius + 10.0f, SKYBLUE);
        DrawCircleLinesV(position_, PlayerRadius + 12.0f, BLUE);
    }

    DrawCircleV(position_, PlayerRadius, {86, 204, 157, 255});

    const Vector2 facingEnd{
        position_.x + facing_.x * 36.0f,
        position_.y + facing_.y * 36.0f
    };
    DrawLineEx(position_, facingEnd, 5.0f, RAYWHITE);
}

void Player::DrawHud() const
{
    DrawText("MOVE: ARROW KEYS", 24, 22, 20, LIGHTGRAY);
    DrawText("Q: DASH   W: PULSE   E: SHIELD   R: RESET", 24, 50, 20, LIGHTGRAY);
    DrawText(TextFormat("LAST ACTION: %s", lastAction_), 24, 84, 20, GOLD);
}

void Player::ExecuteAction(PlayerAction action)
{
    switch (action)
    {
    case PlayerAction::Dash:
        position_.x += facing_.x * DashDistance;
        position_.y += facing_.y * DashDistance;
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
        static_cast<float>(Config::ScreenWidth) - PlayerRadius);
    position_.y = std::clamp(
        position_.y,
        PlayerRadius,
        static_cast<float>(Config::ScreenHeight) - PlayerRadius);
}

void Player::Reset()
{
    position_ = {
        static_cast<float>(Config::ScreenWidth) * 0.5f,
        static_cast<float>(Config::ScreenHeight) * 0.5f
    };
    facing_ = {1.0f, 0.0f};
    pulseTimeRemaining_ = 0.0f;
    shieldEnabled_ = false;
}

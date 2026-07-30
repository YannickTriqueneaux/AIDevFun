#include "Game/Player.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/Renderer2D.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace
{
    constexpr float PlayerHalfSize = 28.0f;
    constexpr float PlayerHeight = 82.0f;
    constexpr float DashDistance = 120.0f;
    constexpr float PulseDuration = 0.35f;

    Engine::Vector2 ProjectPoint(
        GameVector3 point,
        const Engine::Renderer2D& renderer)
    {
        const float centeredX = point.x - static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f;
        const float perspective = 480.0f / (point.z + 480.0f);
        return {
            static_cast<float>(renderer.GetWidth()) * 0.5f + centeredX * perspective,
            static_cast<float>(renderer.GetHeight()) * 0.82f - point.z * 0.46f * perspective - point.y * perspective
        };
    }

    GameVector3 Lerp(GameVector3 a, GameVector3 b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    void DrawProjectedLine(
        Engine::Renderer2D& renderer,
        GameVector3 start,
        GameVector3 end,
        float thickness,
        Engine::Color color)
    {
        renderer.DrawLine(
            ProjectPoint(start, renderer),
            ProjectPoint(end, renderer),
            thickness,
            color);
    }

    void FillQuadWithLines(
        Engine::Renderer2D& renderer,
        GameVector3 topLeft,
        GameVector3 topRight,
        GameVector3 bottomRight,
        GameVector3 bottomLeft,
        Engine::Color color)
    {
        for (float t = 0.0f; t <= 1.0f; t += 0.08f)
        {
            DrawProjectedLine(
                renderer,
                Lerp(topLeft, bottomLeft, t),
                Lerp(topRight, bottomRight, t),
                3.0f,
                color);
        }
    }

    void DrawGroundDiamond(
        Engine::Renderer2D& renderer,
        GameVector3 center,
        float radius,
        Engine::Color color)
    {
        const GameVector3 left{center.x - radius, 0.0f, center.z};
        const GameVector3 far{center.x, 0.0f, center.z + radius};
        const GameVector3 right{center.x + radius, 0.0f, center.z};
        const GameVector3 near{center.x, 0.0f, center.z - radius};

        DrawProjectedLine(renderer, left, far, 2.0f, color);
        DrawProjectedLine(renderer, far, right, 2.0f, color);
        DrawProjectedLine(renderer, right, near, 2.0f, color);
        DrawProjectedLine(renderer, near, left, 2.0f, color);
    }

    void DrawCube(
        Engine::Renderer2D& renderer,
        GameVector3 center,
        float halfSize,
        float height,
        Engine::Color edgeColor)
    {
        const GameVector3 nearBottomLeft{center.x - halfSize, 0.0f, center.z - halfSize};
        const GameVector3 nearBottomRight{center.x + halfSize, 0.0f, center.z - halfSize};
        const GameVector3 farBottomRight{center.x + halfSize, 0.0f, center.z + halfSize};
        const GameVector3 farBottomLeft{center.x - halfSize, 0.0f, center.z + halfSize};
        const GameVector3 nearTopLeft{nearBottomLeft.x, height, nearBottomLeft.z};
        const GameVector3 nearTopRight{nearBottomRight.x, height, nearBottomRight.z};
        const GameVector3 farTopRight{farBottomRight.x, height, farBottomRight.z};
        const GameVector3 farTopLeft{farBottomLeft.x, height, farBottomLeft.z};

        FillQuadWithLines(renderer, farTopLeft, farTopRight, farBottomRight, farBottomLeft, {55, 150, 128, 255});
        FillQuadWithLines(renderer, nearTopRight, farTopRight, farBottomRight, nearBottomRight, {68, 178, 145, 255});
        FillQuadWithLines(renderer, nearTopLeft, nearTopRight, nearBottomRight, nearBottomLeft, {86, 204, 157, 255});

        DrawProjectedLine(renderer, nearBottomLeft, nearBottomRight, 2.0f, edgeColor);
        DrawProjectedLine(renderer, nearBottomRight, farBottomRight, 2.0f, edgeColor);
        DrawProjectedLine(renderer, farBottomRight, farBottomLeft, 2.0f, edgeColor);
        DrawProjectedLine(renderer, farBottomLeft, nearBottomLeft, 2.0f, edgeColor);
        DrawProjectedLine(renderer, nearTopLeft, nearTopRight, 3.0f, edgeColor);
        DrawProjectedLine(renderer, nearTopRight, farTopRight, 3.0f, edgeColor);
        DrawProjectedLine(renderer, farTopRight, farTopLeft, 3.0f, edgeColor);
        DrawProjectedLine(renderer, farTopLeft, nearTopLeft, 3.0f, edgeColor);
        DrawProjectedLine(renderer, nearBottomLeft, nearTopLeft, 2.0f, edgeColor);
        DrawProjectedLine(renderer, nearBottomRight, nearTopRight, 2.0f, edgeColor);
        DrawProjectedLine(renderer, farBottomRight, farTopRight, 2.0f, edgeColor);
        DrawProjectedLine(renderer, farBottomLeft, farTopLeft, 2.0f, edgeColor);
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

    position_.x += command.movement.x * movementSpeed_ * deltaTime;
    position_.z += command.movement.y * movementSpeed_ * deltaTime;

    ExecuteAction(command.action);
    pulseTimeRemaining_ = std::max(0.0f, pulseTimeRemaining_ - deltaTime);
    KeepInsidePlayArea();
}

void Player::Render(Engine::Renderer2D& renderer) const
{
    DrawGroundDiamond(renderer, position_, PlayerHalfSize * 1.4f, {0, 0, 0, 90});

    if (pulseTimeRemaining_ > 0.0f)
    {
        const float progress = 1.0f - pulseTimeRemaining_ / PulseDuration;
        const float halfSize = PlayerHalfSize + progress * 110.0f;
        const auto alpha =
            static_cast<std::uint8_t>((1.0f - progress) * 220.0f);
        DrawCube(
            renderer,
            position_,
            halfSize,
            PlayerHeight + progress * 120.0f,
            {255, 184, 77, alpha});
    }

    if (shieldEnabled_)
    {
        DrawCube(renderer, position_, PlayerHalfSize + 14.0f, PlayerHeight + 18.0f, {102, 191, 255, 255});
        DrawGroundDiamond(renderer, position_, PlayerHalfSize + 34.0f, {0, 121, 241, 255});
    }

    DrawCube(renderer, position_, PlayerHalfSize, PlayerHeight, {235, 255, 245, 255});
    DrawProjectedLine(
        renderer,
        {position_.x, PlayerHeight * 0.62f, position_.z},
        {
            position_.x + facing_.x * 70.0f,
            PlayerHeight * 0.62f,
            position_.z + facing_.y * 70.0f
        },
        5.0f,
        {245, 245, 245, 255});
}

void Player::RenderHud(Engine::Renderer2D& renderer) const
{
    renderer.DrawText("3D MOVE: LEFT/RIGHT STRAFE   UP/DOWN DEPTH", {24.0f, 22.0f}, 20, {200, 200, 200, 255});
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
    serializer.Value("position.z", position_.z);
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
        position_.x += facing_.x * DashDistance;
        position_.z += facing_.y * DashDistance;
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
    position_.y = 0.0f;
    position_.z = std::clamp(
        position_.z,
        PlayerHalfSize,
        static_cast<float>(GameConfig::PlayAreaDepth) - PlayerHalfSize);
}

void Player::Reset()
{
    position_ = {
        static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
        0.0f,
        static_cast<float>(GameConfig::PlayAreaDepth) * 0.5f
    };
    facing_ = {1.0f, 0.0f};
    pulseTimeRemaining_ = 0.0f;
    shieldEnabled_ = false;
}

#pragma once

#include "Input.h"
#include "raylib.h"

class Player
{
public:
    Player();

    void Update(const InputState& input, float deltaTime);
    void Draw() const;
    void DrawHud() const;

private:
    void ExecuteAction(PlayerAction action);
    void KeepInsidePlayArea();
    void Reset();

    Vector2 position_{};
    Vector2 facing_{1.0f, 0.0f};
    float movementSpeed_ = 260.0f;
    float pulseTimeRemaining_ = 0.0f;
    bool shieldEnabled_ = false;
    const char* lastAction_ = "None";
};


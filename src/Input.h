#pragma once

#include "raylib.h"

enum class PlayerAction
{
    None,
    Dash,
    Pulse,
    ToggleShield,
    Reset
};

struct InputState
{
    Vector2 movement{};
    PlayerAction action = PlayerAction::None;
};

class Input
{
public:
    [[nodiscard]] InputState Poll() const;
};


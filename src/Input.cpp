#include "Input.h"

#include <cmath>

namespace
{
    Vector2 NormalizeOrZero(Vector2 value)
    {
        const float lengthSquared = value.x * value.x + value.y * value.y;
        if (lengthSquared <= 0.0f)
        {
            return {};
        }

        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return {value.x * inverseLength, value.y * inverseLength};
    }
}

InputState Input::Poll() const
{
    InputState state;

    state.movement.x =
        static_cast<float>(IsKeyDown(KEY_RIGHT)) -
        static_cast<float>(IsKeyDown(KEY_LEFT));
    state.movement.y =
        static_cast<float>(IsKeyDown(KEY_DOWN)) -
        static_cast<float>(IsKeyDown(KEY_UP));
    state.movement = NormalizeOrZero(state.movement);

    // Actions are edge-triggered: one event for each key press.
    if (IsKeyPressed(KEY_Q))
    {
        state.action = PlayerAction::Dash;
    }
    else if (IsKeyPressed(KEY_W))
    {
        state.action = PlayerAction::Pulse;
    }
    else if (IsKeyPressed(KEY_E))
    {
        state.action = PlayerAction::ToggleShield;
    }
    else if (IsKeyPressed(KEY_R))
    {
        state.action = PlayerAction::Reset;
    }

    return state;
}

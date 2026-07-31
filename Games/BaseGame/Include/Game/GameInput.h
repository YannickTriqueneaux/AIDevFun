#pragma once

#include "Engine/Math/Vector2.h"

namespace Engine {
class InputSystem;
}

enum class PlayerAction { None, Dash, Pulse, ToggleShield, Reset };

struct PlayerCommand {
  Engine::Vector2 movement{};
  PlayerAction action = PlayerAction::None;
};

class GameInput {
public:
  [[nodiscard]] PlayerCommand
  BuildPlayerCommand(const Engine::InputSystem &input) const;
};

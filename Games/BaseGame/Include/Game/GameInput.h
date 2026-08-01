#pragma once

#include "Engine/Math/Vector2.h"

namespace Engine {
class InputSystem;
}

struct PlayerCommand {
  Engine::Vector2 movement{};
  bool firing = false;
};

class GameInput {
public:
  [[nodiscard]] PlayerCommand
  BuildPlayerCommand(const Engine::InputSystem &input) const;
};

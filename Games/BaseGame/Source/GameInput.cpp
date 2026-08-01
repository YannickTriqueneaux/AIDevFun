#include "Game/GameInput.h"

#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"

#include <cmath>

namespace {
Engine::Vector2 NormalizeOrZero(Engine::Vector2 value) {
  const float lengthSquared = value.x * value.x + value.y * value.y;
  if (lengthSquared <= 0.0f) {
    return {};
  }

  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  return {value.x * inverseLength, value.y * inverseLength};
}
} // namespace

PlayerCommand
GameInput::BuildPlayerCommand(const Engine::InputSystem &input) const {
  PlayerCommand command;

  command.movement.x = static_cast<float>(input.IsDown(Engine::Key::Right)) -
                       static_cast<float>(input.IsDown(Engine::Key::Left));
  command.movement.y = static_cast<float>(input.IsDown(Engine::Key::Down)) -
                       static_cast<float>(input.IsDown(Engine::Key::Up));
  command.movement = NormalizeOrZero(command.movement);

  command.firing = input.IsDown(Engine::Key::W);

  return command;
}

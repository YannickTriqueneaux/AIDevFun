#include "Game/GameInput.h"

#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cmath>

namespace {
bool IsKeyboardKeyDown(int virtualKey) {
#if defined(_WIN32)
  return (::GetAsyncKeyState(virtualKey) & 0x8000) != 0;
#else
  (void)virtualKey;
  return false;
#endif
}

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

  (void)input;

  command.movement.x =
      static_cast<float>(IsKeyboardKeyDown('D') ||
                         IsKeyboardKeyDown(VK_RIGHT)) -
      static_cast<float>(IsKeyboardKeyDown('A') || IsKeyboardKeyDown(VK_LEFT));
  command.movement.y =
      static_cast<float>(IsKeyboardKeyDown('S') || IsKeyboardKeyDown(VK_DOWN)) -
      static_cast<float>(IsKeyboardKeyDown('W') || IsKeyboardKeyDown(VK_UP));
  command.movement = NormalizeOrZero(command.movement);

  command.firing = false;

  return command;
}

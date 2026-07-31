#include "Engine/Input/InputSystem.h"

#include "raylib.h"

namespace {
int ToRaylibKey(Engine::Key key) {
  switch (key) {
  case Engine::Key::Up:
    return KEY_UP;
  case Engine::Key::Down:
    return KEY_DOWN;
  case Engine::Key::Left:
    return KEY_LEFT;
  case Engine::Key::Right:
    return KEY_RIGHT;
  case Engine::Key::Q:
    return KEY_Q;
  case Engine::Key::W:
    return KEY_W;
  case Engine::Key::E:
    return KEY_E;
  case Engine::Key::R:
    return KEY_R;
  case Engine::Key::Escape:
    return KEY_ESCAPE;
  }

  return KEY_NULL;
}
} // namespace

namespace Engine {
void InputSystem::Update() {
  // raylib updates its input state while polling window events.
}

bool InputSystem::IsDown(Key key) const {
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_) {
    return autoTestDown_[static_cast<std::size_t>(key)];
  }
#endif
  return IsKeyDown(ToRaylibKey(key));
}

bool InputSystem::WasPressed(Key key) const {
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_) {
    return autoTestPressed_[static_cast<std::size_t>(key)];
  }
#endif
  return IsKeyPressed(ToRaylibKey(key));
}

#if defined(ENGINE_AUTOTESTS)
void InputSystem::EnableAutoTestInput() { autoTestInput_ = true; }

void InputSystem::SetAutoTestKeyDown(Key key, bool down) {
  autoTestInput_ = true;
  autoTestDown_[static_cast<std::size_t>(key)] = down;
}

void InputSystem::SetAutoTestKeyPressed(Key key, bool pressed) {
  autoTestInput_ = true;
  autoTestPressed_[static_cast<std::size_t>(key)] = pressed;
}

void InputSystem::ClearAutoTestPressedKeys() { autoTestPressed_.fill(false); }
#endif
} // namespace Engine

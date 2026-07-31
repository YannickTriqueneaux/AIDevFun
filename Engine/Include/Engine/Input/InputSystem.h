#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Input/Key.h"

#if defined(ENGINE_AUTOTESTS)
#include <array>
#endif

namespace Engine {
class ENGINE_API InputSystem {
public:
  void Update();

  [[nodiscard]] bool IsDown(Key key) const;
  [[nodiscard]] bool WasPressed(Key key) const;
#if defined(ENGINE_AUTOTESTS)
  void EnableAutoTestInput();
  void SetAutoTestKeyDown(Key key, bool down);
  void SetAutoTestKeyPressed(Key key, bool pressed);
  void ClearAutoTestPressedKeys();
#endif

private:
#if defined(ENGINE_AUTOTESTS)
  static constexpr std::size_t KeyCount = 9;
  bool autoTestInput_ = false;
  std::array<bool, KeyCount> autoTestDown_{};
  std::array<bool, KeyCount> autoTestPressed_{};
#endif
};
} // namespace Engine

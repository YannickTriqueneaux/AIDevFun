#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Math/Vector2.h"

#include <string>
#include <string_view>

namespace Engine {
enum class UiCondition { Always, FirstUse };

class ENGINE_API UiSystem {
public:
  UiSystem();
  ~UiSystem();

  UiSystem(const UiSystem &) = delete;
  UiSystem &operator=(const UiSystem &) = delete;

  void BeginFrame();
  void EndFrame();

  void SetNextWindowPosition(Vector2 position, UiCondition condition);
  void SetNextWindowSize(Vector2 size, UiCondition condition);
  [[nodiscard]] bool BeginPanel(std::string_view title, bool movable = true,
                                bool resizable = true, bool decorated = true);
  void EndPanel();

  [[nodiscard]] bool Button(std::string_view label, Vector2 size);

  [[nodiscard]] bool BeginChild(std::string_view id, Vector2 size,
                                bool border = false);
  void EndChild();

  [[nodiscard]] bool InputText(std::string_view label, std::string_view hint,
                               std::string &value, bool submitOnEnter = true);
  [[nodiscard]] bool IsPasteShortcutPressed() const;

  void Text(std::string_view text, Color color);
  void TextWrapped(std::string_view text, float wrapWidth, Color color);
  void Separator();
  void Spacing();
  void SetKeyboardFocusHere();
  void ScrollToBottom();

  [[nodiscard]] float GetAvailableWidth() const;
  [[nodiscard]] float GetTextWidth(std::string_view text) const;
  [[nodiscard]] float GetCursorX() const;
  void SetCursorX(float localX);
  [[nodiscard]] float GetInputHeight() const;
};
} // namespace Engine

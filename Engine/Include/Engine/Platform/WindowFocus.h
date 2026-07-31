#pragma once

#include "Engine/Core/Export.h"

#include <string_view>

namespace Engine {
class ENGINE_API WindowFocus {
public:
  [[nodiscard]] static bool FocusWindowByTitle(std::string_view windowTitle);
};
} // namespace Engine

#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/Core/Export.h"

#include <filesystem>

namespace Engine {
struct LauncherSettings {
  OpenAISettings openAI;
};

class ENGINE_API Settings {
public:
  [[nodiscard]] static LauncherSettings Load(const std::filesystem::path &path);
};
} // namespace Engine

#pragma once

#include "Engine/Core/Export.h"

#include <filesystem>
#include <string_view>

namespace Engine {
class ENGINE_API CrashDiagnostics {
public:
  static void Install(const std::filesystem::path &outputDirectory,
                      std::string_view processName);
};
} // namespace Engine

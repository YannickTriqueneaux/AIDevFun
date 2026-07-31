#pragma once

#include "Engine/Core/Export.h"

#include <filesystem>
#include <string_view>

namespace Engine {
class ENGINE_API Logger {
public:
  static void Initialize(const std::filesystem::path &file);
  static void Info(std::string_view message);
  static void Warning(std::string_view message);
  static void Error(std::string_view message);
  static void Flush();
};
} // namespace Engine

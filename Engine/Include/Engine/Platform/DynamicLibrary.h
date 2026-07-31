#pragma once

#include "Engine/Core/Export.h"

#include <filesystem>

namespace Engine {
class ENGINE_API DynamicLibrary {
public:
  explicit DynamicLibrary(const std::filesystem::path &path);
  ~DynamicLibrary();

  DynamicLibrary(const DynamicLibrary &) = delete;
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;

  DynamicLibrary(DynamicLibrary &&other) noexcept;
  DynamicLibrary &operator=(DynamicLibrary &&other) noexcept;

  [[nodiscard]] void *GetFunction(const char *functionName) const;

private:
  void *handle_ = nullptr;
};
} // namespace Engine

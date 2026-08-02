#pragma once

#include "Development/AssistantProvider.h"
#include "Engine/Platform/DynamicLibrary.h"

#include <filesystem>
#include <memory>

namespace AssistantHost {

class AssistantProviderLoader {
public:
  AssistantProviderLoader(const std::filesystem::path &libraryPath,
                          const std::filesystem::path &settingsPath,
                          const std::filesystem::path &gameRoot);
  ~AssistantProviderLoader();

  [[nodiscard]] Development::AssistantProvider &Get() const;

private:
  Engine::DynamicLibrary library_;
  const Development::AssistantProviderApi *api_ = nullptr;
  Development::AssistantProvider *provider_ = nullptr;
};

} // namespace AssistantHost

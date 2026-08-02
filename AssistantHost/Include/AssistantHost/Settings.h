#pragma once

#include <filesystem>
#include <string>

namespace AssistantHost {
struct LauncherSettings {
  std::string providerLibrary = "AssistantProviderOpenAI.dll";
  std::string providerSettings = "OpenAIProvider.settings.json";
};

class Settings {
public:
  [[nodiscard]] static LauncherSettings Load(const std::filesystem::path &path);
};
} // namespace AssistantHost

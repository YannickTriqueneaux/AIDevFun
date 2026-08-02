#include "AssistantHost/Settings.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace AssistantHost {
LauncherSettings Settings::Load(const std::filesystem::path &path) {
  std::ifstream stream(path);
  if (!stream)
    throw std::runtime_error("Settings file not found: " + path.string());
  nlohmann::json document;
  stream >> document;
  const auto &assistant = document.at("assistant");
  return {.providerLibrary = assistant.value("providerLibrary", "AssistantProviderOpenAI.dll"),
          .providerSettings = assistant.value("providerSettings", "OpenAIProvider.settings.json")};
}
} // namespace AssistantHost

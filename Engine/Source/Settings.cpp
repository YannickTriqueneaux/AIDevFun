#include "Engine/Core/Settings.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace Engine {
LauncherSettings Settings::Load(const std::filesystem::path &path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("Settings file not found: " + path.string());
  }

  nlohmann::json document;
  try {
    stream >> document;
  } catch (const nlohmann::json::exception &exception) {
    throw std::runtime_error("Invalid settings JSON: " +
                             std::string(exception.what()));
  }

  LauncherSettings settings;
  const nlohmann::json &openAI = document.at("openai");
  settings.openAI.apiKey = openAI.value("apiKey", "");
  settings.openAI.model = openAI.value("model", "gpt-5.5");
  if (openAI.contains("pricing")) {
    const nlohmann::json &pricing = openAI.at("pricing");
    settings.openAI.pricing.model = pricing.value("model", "");
    settings.openAI.pricing.inputUsdPerMillion =
        pricing.value("inputUsdPerMillion", 0.0);
    settings.openAI.pricing.cachedInputUsdPerMillion =
        pricing.value("cachedInputUsdPerMillion", 0.0);
    settings.openAI.pricing.outputUsdPerMillion =
        pricing.value("outputUsdPerMillion", 0.0);
    settings.openAI.pricing.longContextThreshold =
        pricing.value("longContextThreshold", 0ULL);
    settings.openAI.pricing.longContextInputMultiplier =
        pricing.value("longContextInputMultiplier", 1.0);
    settings.openAI.pricing.longContextOutputMultiplier =
        pricing.value("longContextOutputMultiplier", 1.0);
  }
  return settings;
}
} // namespace Engine

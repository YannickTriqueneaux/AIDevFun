#include "AssistantHost/OpenAIClient.h"
#include "Development/AssistantProvider.h"

#include <fstream>
#include <nlohmann/json.hpp>

#if defined(_WIN32)
#define ASSISTANT_PROVIDER_API extern "C" __declspec(dllexport)
#else
#define ASSISTANT_PROVIDER_API extern "C"
#endif

namespace {
AssistantHost::OpenAISettings LoadSettings(const char *path) {
  std::ifstream stream(path);
  nlohmann::json document;
  stream >> document;
  AssistantHost::OpenAISettings settings;
  settings.apiKey = document.value("apiKey", "");
  settings.model = document.value("model", "gpt-5.5");
  if (document.contains("pricing")) {
    const auto &pricing = document.at("pricing");
    settings.pricing.model = pricing.value("model", "");
    settings.pricing.inputUsdPerMillion =
        pricing.value("inputUsdPerMillion", 0.0);
    settings.pricing.cachedInputUsdPerMillion =
        pricing.value("cachedInputUsdPerMillion", 0.0);
    settings.pricing.outputUsdPerMillion =
        pricing.value("outputUsdPerMillion", 0.0);
    settings.pricing.longContextThreshold =
        pricing.value("longContextThreshold", 0ULL);
    settings.pricing.longContextInputMultiplier =
        pricing.value("longContextInputMultiplier", 1.0);
    settings.pricing.longContextOutputMultiplier =
        pricing.value("longContextOutputMultiplier", 1.0);
  }
  return settings;
}

Development::AssistantProvider *Create(const char *settingsPath, const char *) {
  try {
    return new AssistantHost::OpenAIClient(LoadSettings(settingsPath));
  } catch (...) {
    return nullptr;
  }
}
void Destroy(Development::AssistantProvider *provider) { delete provider; }
} // namespace

ASSISTANT_PROVIDER_API const Development::AssistantProviderApi *
GetAssistantProviderApi() {
  static const Development::AssistantProviderApi api{
      Development::AssistantProviderApiVersion, &Create, &Destroy};
  return &api;
}

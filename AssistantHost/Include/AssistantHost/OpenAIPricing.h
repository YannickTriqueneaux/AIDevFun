#pragma once

#include "Development/AssistantProvider.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace AssistantHost {
struct OpenAIPricingSettings {
  std::string model;
  double inputUsdPerMillion = 0.0;
  double cachedInputUsdPerMillion = 0.0;
  double outputUsdPerMillion = 0.0;
  std::uint64_t longContextThreshold = 0;
  double longContextInputMultiplier = 1.0;
  double longContextOutputMultiplier = 1.0;

  [[nodiscard]] bool IsConfigured() const {
    return !model.empty() && inputUsdPerMillion >= 0.0 &&
           cachedInputUsdPerMillion >= 0.0 && outputUsdPerMillion >= 0.0;
  }
};

using OpenAITokenUsage = Development::AssistantTokenUsage;

using OpenAICostEstimate = Development::AssistantCostEstimate;

[[nodiscard]] OpenAICostEstimate
EstimateOpenAICost(std::string_view model, const OpenAIPricingSettings &pricing,
                   const OpenAITokenUsage &usage);
} // namespace AssistantHost

#pragma once

#include "Engine/Core/Export.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Engine {
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

struct OpenAITokenUsage {
  std::uint64_t inputTokens = 0;
  std::uint64_t cachedInputTokens = 0;
  std::uint64_t outputTokens = 0;

  [[nodiscard]] std::uint64_t TotalTokens() const {
    return inputTokens + outputTokens;
  }

  OpenAITokenUsage &operator+=(const OpenAITokenUsage &other) {
    inputTokens += other.inputTokens;
    cachedInputTokens += other.cachedInputTokens;
    outputTokens += other.outputTokens;
    return *this;
  }
};

struct OpenAICostEstimate {
  bool available = false;
  double usd = 0.0;
};

[[nodiscard]] ENGINE_API OpenAICostEstimate
EstimateOpenAICost(std::string_view model, const OpenAIPricingSettings &pricing,
                   const OpenAITokenUsage &usage);
} // namespace Engine

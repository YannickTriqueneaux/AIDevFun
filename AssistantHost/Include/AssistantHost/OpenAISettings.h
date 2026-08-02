#pragma once

#include "AssistantHost/OpenAIPricing.h"

#include <string>

namespace AssistantHost {
struct OpenAISettings {
  std::string apiKey;
  std::string model = "gpt-5.5";
  OpenAIPricingSettings pricing;

  [[nodiscard]] bool IsConfigured() const {
    return !apiKey.empty() && !model.empty();
  }
};
} // namespace AssistantHost

#pragma once

#include "Engine/AI/GameToolClient.h"
#include "Engine/AI/OpenAIClient.h"
#include "Engine/UI/PromptMessage.h"

#include <string_view>
#include <vector>

namespace Engine {
struct PromptProcessResult {
  std::vector<PromptMessage> messages;
  OpenAITokenUsage usage;
  bool usageReported = false;
  bool costAvailable = false;
  double estimatedCostUsd = 0.0;
};

class PromptProcessor {
public:
  explicit PromptProcessor(OpenAISettings settings);

  [[nodiscard]] bool IsConfigured() const;
  [[nodiscard]] const std::string &GetModel() const;
  [[nodiscard]] PromptProcessResult
  Process(std::string_view prompt, const std::vector<OpenAIImageInput> &images,
          const OpenAIStreamCallback &onEvent);

private:
  OpenAIClient client_;
  GameToolClient gameTools_;
  std::string previousResponseId_;
};
} // namespace Engine

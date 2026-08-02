#pragma once

#include "AssistantHost/GameToolClient.h"
#include "AssistantHost/PromptMessage.h"
#include "Development/AssistantProvider.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AssistantHost {
using AssistantImageInput = Development::AssistantImageInput;
using AssistantStreamEvent = Development::AssistantStreamEvent;
using AssistantStreamEventType = Development::AssistantStreamEventType;
using AssistantStreamCallback = Development::AssistantStreamCallback;
using AssistantTokenUsage = Development::AssistantTokenUsage;
struct PromptProcessResult {
  std::vector<PromptMessage> messages;
  AssistantTokenUsage usage;
  bool usageReported = false;
  bool costAvailable = false;
  double estimatedCostUsd = 0.0;
};

class PromptProcessor {
public:
  PromptProcessor(Development::AssistantProvider &provider,
                  const std::filesystem::path &promptConfigPath);

  [[nodiscard]] bool IsConfigured() const;
  [[nodiscard]] const std::string &GetModel() const;
  [[nodiscard]] const std::string &GetProviderName() const;
  [[nodiscard]] PromptProcessResult
  Process(std::string_view prompt,
          const std::vector<AssistantImageInput> &images,
          const AssistantStreamCallback &onEvent);

private:
  Development::AssistantProvider &provider_;
  std::filesystem::path promptConfigPath_;
  GameToolClient gameTools_;
  std::string previousResponseId_;
};
} // namespace AssistantHost

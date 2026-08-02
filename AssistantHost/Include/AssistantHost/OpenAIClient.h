#pragma once

#include "AssistantHost/OpenAIPricing.h"
#include "AssistantHost/OpenAISettings.h"
#include "Development/AssistantProvider.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace AssistantHost {
using OpenAIStreamEventType = Development::AssistantStreamEventType;
using OpenAIStreamEvent = Development::AssistantStreamEvent;
using OpenAIStreamCallback = Development::AssistantStreamCallback;
using OpenAIResponse = Development::AssistantResponse;
using OpenAIToolOutput = Development::AssistantToolOutput;
using OpenAIImageInput = Development::AssistantImageInput;

class OpenAIClient final : public Development::AssistantProvider {
public:
  explicit OpenAIClient(OpenAISettings settings);

  [[nodiscard]] bool IsConfigured() const;
  [[nodiscard]] const std::string &GetDisplayName() const override;
  [[nodiscard]] const std::string &GetModel() const;
  [[nodiscard]] OpenAICostEstimate
  EstimateCost(const Development::AssistantTokenUsage &usage) const override;
  [[nodiscard]] OpenAIResponse
  CreateResponse(std::string_view instructions, std::string_view prompt,
                 const std::vector<OpenAIImageInput> &images,
                 std::string_view previousResponseId,
                 const OpenAIStreamCallback &onEvent) const override;
  [[nodiscard]] OpenAIResponse
  ContinueWithToolOutputs(std::string_view instructions,
                          const std::vector<OpenAIToolOutput> &outputs,
                          std::string_view previousResponseId,
                          const OpenAIStreamCallback &onEvent,
                          bool allowTools = true) const override;

private:
  OpenAISettings settings_;
};
} // namespace AssistantHost

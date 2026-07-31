#pragma once

#include "Engine/AI/OpenAIPricing.h"
#include "Engine/AI/OpenAISettings.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine {
enum class OpenAIStreamEventType {
  Status,
  ReasoningSummaryDelta,
  OutputTextDelta
};

struct OpenAIStreamEvent {
  OpenAIStreamEventType type = OpenAIStreamEventType::Status;
  std::string text;
};

using OpenAIStreamCallback = std::function<void(const OpenAIStreamEvent &)>;

struct OpenAIResponse {
  struct ToolCall {
    std::string callId;
    std::string name;
    std::string arguments;
  };

  std::string id;
  std::string text;
  std::vector<ToolCall> toolCalls;
  OpenAITokenUsage usage;
};

struct OpenAIToolOutput {
  std::string callId;
  std::string output;
};

class OpenAIClient {
public:
  explicit OpenAIClient(OpenAISettings settings);

  [[nodiscard]] bool IsConfigured() const;
  [[nodiscard]] const std::string &GetModel() const;
  [[nodiscard]] OpenAICostEstimate
  EstimateCost(const OpenAITokenUsage &usage) const;
  [[nodiscard]] OpenAIResponse
  CreateResponse(std::string_view instructions, std::string_view prompt,
                 std::string_view previousResponseId,
                 const OpenAIStreamCallback &onEvent) const;
  [[nodiscard]] OpenAIResponse
  ContinueWithToolOutputs(std::string_view instructions,
                          const std::vector<OpenAIToolOutput> &outputs,
                          std::string_view previousResponseId,
                          const OpenAIStreamCallback &onEvent,
                          bool allowTools = true) const;

private:
  OpenAISettings settings_;
};
} // namespace Engine

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Development {

inline constexpr std::uint32_t AssistantProviderApiVersion = 1;

enum class AssistantStreamEventType {
  Status,
  ReasoningSummaryDelta,
  OutputTextDelta
};

struct AssistantStreamEvent {
  AssistantStreamEventType type = AssistantStreamEventType::Status;
  std::string text;
};

using AssistantStreamCallback =
    std::function<void(const AssistantStreamEvent &)>;

struct AssistantTokenUsage {
  std::uint64_t inputTokens = 0;
  std::uint64_t cachedInputTokens = 0;
  std::uint64_t outputTokens = 0;
  [[nodiscard]] std::uint64_t TotalTokens() const {
    return inputTokens + outputTokens;
  }
  AssistantTokenUsage &operator+=(const AssistantTokenUsage &other) {
    inputTokens += other.inputTokens;
    cachedInputTokens += other.cachedInputTokens;
    outputTokens += other.outputTokens;
    return *this;
  }
};

struct AssistantCostEstimate {
  bool available = false;
  double usd = 0.0;
};
struct AssistantImageInput {
  std::string mimeType;
  std::string base64Data;
  int width = 0;
  int height = 0;
};
struct AssistantToolOutput {
  std::string callId;
  std::string output;
};

struct AssistantResponse {
  struct ToolCall {
    std::string callId;
    std::string name;
    std::string arguments;
  };
  std::string id;
  std::string text;
  std::vector<ToolCall> toolCalls;
  AssistantTokenUsage usage;
};

class AssistantProvider {
public:
  virtual ~AssistantProvider() = default;
  [[nodiscard]] virtual bool IsConfigured() const = 0;
  [[nodiscard]] virtual const std::string &GetDisplayName() const = 0;
  [[nodiscard]] virtual const std::string &GetModel() const = 0;
  [[nodiscard]] virtual AssistantCostEstimate
  EstimateCost(const AssistantTokenUsage &) const = 0;
  [[nodiscard]] virtual AssistantResponse
  CreateResponse(std::string_view instructions, std::string_view prompt,
                 const std::vector<AssistantImageInput> &images,
                 std::string_view previousResponseId,
                 const AssistantStreamCallback &onEvent) const = 0;
  [[nodiscard]] virtual AssistantResponse
  ContinueWithToolOutputs(std::string_view instructions,
                          const std::vector<AssistantToolOutput> &outputs,
                          std::string_view previousResponseId,
                          const AssistantStreamCallback &onEvent,
                          bool allowTools) const = 0;
};

struct AssistantProviderApi {
  std::uint32_t apiVersion;
  AssistantProvider *(*create)(const char *settingsPath, const char *gameRoot);
  void (*destroy)(AssistantProvider *provider);
};

using GetAssistantProviderApiFunction = const AssistantProviderApi *(*)();

} // namespace Development

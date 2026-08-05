#pragma once

#include "Development/AssistantProvider.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AssistantProviders::Claude {
struct ParsedEventLine {
  std::vector<Development::AssistantStreamEvent> events;
  std::optional<Development::AssistantTokenUsage> usage;
  std::string sessionId;
  std::string resultText;
  bool failed = false;
  std::string error;
};

[[nodiscard]] ParsedEventLine ParseEventLine(std::string_view line);
} // namespace AssistantProviders::Claude

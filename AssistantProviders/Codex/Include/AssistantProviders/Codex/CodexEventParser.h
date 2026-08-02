#pragma once

#include "Development/AssistantProvider.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AssistantProviders::Codex {

struct ParsedEventLine {
  std::vector<Development::AssistantStreamEvent> events;
  std::optional<Development::AssistantTokenUsage> usage;
  std::string threadId;
};

[[nodiscard]] ParsedEventLine ParseEventLine(std::string_view line);

} // namespace AssistantProviders::Codex

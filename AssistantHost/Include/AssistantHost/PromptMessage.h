#pragma once

#include <string>

namespace AssistantHost {
enum class PromptMessageRole { User, Result, Information };

struct PromptMessage {
  PromptMessageRole role = PromptMessageRole::Information;
  std::string text;
};
} // namespace AssistantHost

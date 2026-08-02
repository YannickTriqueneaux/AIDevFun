#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/UI/PromptMessage.h"
#include "Engine/UI/PromptProcessor.h"

#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Engine {
class UiSystem;

struct PromptConsoleOptions {
  bool collapsible = true;
  bool expandedByDefault = false;
  bool fillWindow = false;
  std::filesystem::path automaticPromptFile;
};

class PromptConsole {
public:
  explicit PromptConsole(OpenAISettings settings,
                         PromptConsoleOptions options = {});

  void Render(UiSystem &ui, int screenWidth, int screenHeight);

private:
  void SubmitPrompt();
  void PollPendingRequest();
  void PollStreamEvents();
  void PollAutomaticPrompt();
  void PasteClipboardImage();

  std::vector<PromptMessage> messages_;
  std::vector<std::string> activityLogs_;
  std::string promptInput_;
  std::vector<OpenAIImageInput> promptImages_;
  PromptProcessor processor_;
  PromptConsoleOptions options_;
  std::future<PromptProcessResult> pendingRequest_;
  std::mutex streamEventsMutex_;
  std::vector<OpenAIStreamEvent> pendingStreamEvents_;
  std::optional<std::size_t> activeResponseIndex_;
  std::optional<std::size_t> activeReasoningLogIndex_;
  std::optional<std::filesystem::file_time_type> lastAutomaticPromptTime_;
  OpenAITokenUsage lastPromptUsage_;
  OpenAITokenUsage sessionUsage_;
  double lastPromptCostUsd_ = 0.0;
  double sessionCostUsd_ = 0.0;
  bool lastPromptCostAvailable_ = false;
  bool sessionCostAvailable_ = true;
  bool hasCompletedPrompt_ = false;
  bool expanded_ = false;
  bool scrollToLatest_ = true;
  bool focusInput_ = true;
};
} // namespace Engine

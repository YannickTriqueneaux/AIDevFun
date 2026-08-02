#pragma once

#include "AssistantHost/PromptMessage.h"
#include "AssistantHost/PromptProcessor.h"

#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Engine {
class UiSystem;
}

namespace AssistantHost {

struct PromptTextAttachment {
  std::string displayName;
  std::string promptText;
};

struct PromptAttachmentResult {
  bool handled = false;
  std::optional<PromptTextAttachment> attachment;
  std::string message;
};

class PromptAttachmentProvider {
public:
  virtual ~PromptAttachmentProvider() = default;
  [[nodiscard]] virtual PromptAttachmentResult PasteClipboardFile() = 0;
};

struct PromptConsoleOptions {
  bool collapsible = true;
  bool expandedByDefault = false;
  bool fillWindow = false;
  std::filesystem::path automaticPromptFile;
};

class PromptConsole {
public:
  explicit PromptConsole(
      Development::AssistantProvider &provider,
      PromptConsoleOptions options = {},
      PromptAttachmentProvider *attachmentProvider = nullptr);

  void Render(Engine::UiSystem &ui, int screenWidth, int screenHeight);

private:
  void SubmitPrompt();
  void PollPendingRequest();
  void PollStreamEvents();
  void PollAutomaticPrompt();
  void PasteClipboardImage();
  [[nodiscard]] bool PasteClipboardFile();

  std::vector<PromptMessage> messages_;
  std::vector<std::string> activityLogs_;
  std::string promptInput_;
  std::vector<AssistantImageInput> promptImages_;
  std::vector<PromptTextAttachment> promptFileAttachments_;
  PromptAttachmentProvider *attachmentProvider_ = nullptr;
  PromptProcessor processor_;
  PromptConsoleOptions options_;
  std::future<PromptProcessResult> pendingRequest_;
  std::mutex streamEventsMutex_;
  std::vector<AssistantStreamEvent> pendingStreamEvents_;
  std::optional<std::size_t> activeResponseIndex_;
  std::optional<std::size_t> activeReasoningLogIndex_;
  std::optional<std::filesystem::file_time_type> lastAutomaticPromptTime_;
  AssistantTokenUsage lastPromptUsage_;
  AssistantTokenUsage sessionUsage_;
  double lastPromptCostUsd_ = 0.0;
  double sessionCostUsd_ = 0.0;
  bool lastPromptCostAvailable_ = false;
  bool sessionCostAvailable_ = true;
  bool hasCompletedPrompt_ = false;
  bool expanded_ = false;
  bool scrollToLatest_ = true;
  bool focusInput_ = true;
};
} // namespace AssistantHost

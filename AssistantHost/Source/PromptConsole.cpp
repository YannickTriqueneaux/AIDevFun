#include "AssistantHost/PromptConsole.h"

#include "Engine/UI/UiSystem.h"

#include "Engine/Core/Logger.h"

#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace {
constexpr float PanelWidth = 420.0f;
constexpr float ToggleWidth = 44.0f;
constexpr float ToggleHeight = 48.0f;
constexpr float ToggleButtonSize = 24.0f;
constexpr float ToggleTop = 12.0f;
constexpr float MessageWidthRatio = 0.78f;
constexpr Engine::Color UserColor{110, 190, 255, 255};
constexpr Engine::Color ResultColor{235, 235, 235, 255};
constexpr Engine::Color InformationColor{145, 150, 165, 255};
constexpr std::size_t MaximumPromptImages = 4;
constexpr std::size_t MaximumPromptFileAttachments = 2;
constexpr int MaximumImageDimension = 2048;

std::string EncodeBase64(const unsigned char *data, std::size_t size) {
  static constexpr char Alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((size + 2) / 3) * 4);
  for (std::size_t offset = 0; offset < size; offset += 3) {
    const std::uint32_t first = data[offset];
    const std::uint32_t second = offset + 1 < size ? data[offset + 1] : 0;
    const std::uint32_t third = offset + 2 < size ? data[offset + 2] : 0;
    const std::uint32_t value = (first << 16) | (second << 8) | third;
    result.push_back(Alphabet[(value >> 18) & 63]);
    result.push_back(Alphabet[(value >> 12) & 63]);
    result.push_back(offset + 1 < size ? Alphabet[(value >> 6) & 63] : '=');
    result.push_back(offset + 2 < size ? Alphabet[value & 63] : '=');
  }
  return result;
}

std::string FormatTokens(std::uint64_t tokens) {
  std::string digits = std::to_string(tokens);
  for (std::ptrdiff_t position = static_cast<std::ptrdiff_t>(digits.size()) - 3;
       position > 0; position -= 3) {
    digits.insert(static_cast<std::size_t>(position), ",");
  }
  return digits;
}

std::string FormatCost(std::string_view label,
                       const AssistantHost::AssistantTokenUsage &usage,
                       bool costAvailable, double costUsd) {
  std::ostringstream stream;
  stream << label << ": ";
  if (costAvailable) {
    stream << "~US$" << std::fixed << std::setprecision(costUsd < 0.01 ? 6 : 4)
           << costUsd;
  } else {
    stream << "cost unavailable";
  }
  stream << " (" << FormatTokens(usage.TotalTokens()) << " tokens)";
  return stream.str();
}

} // namespace

namespace AssistantHost {
using Engine::Color;
using Engine::UiCondition;
using Engine::UiSystem;
PromptConsole::PromptConsole(Development::AssistantProvider &provider,
                             PromptConsoleOptions options,
                             PromptAttachmentProvider *attachmentProvider)
    : processor_(provider, options.promptConfigFile), options_(options),
      attachmentProvider_(attachmentProvider),
      expanded_(options.expandedByDefault) {
  messages_.push_back(
      {PromptMessageRole::Information,
       processor_.IsConfigured()
           ? processor_.GetProviderName() + " ready with model " +
                 processor_.GetModel() + "."
           : processor_.GetProviderName() + " is not configured."});
}

void PromptConsole::Render(UiSystem &ui, int screenWidth, int screenHeight) {
  PollStreamEvents();
  PollPendingRequest();
  PollAutomaticPrompt();

  const float effectivePanelWidth =
      options_.fillWindow ? static_cast<float>(screenWidth) : PanelWidth;
  const float panelLeft = static_cast<float>(screenWidth) - effectivePanelWidth;

  if (options_.collapsible) {
    const float toggleLeft =
        expanded_ ? panelLeft - ToggleWidth
                  : static_cast<float>(screenWidth) - ToggleWidth;

    ui.SetNextWindowPosition({toggleLeft, ToggleTop}, UiCondition::Always);
    ui.SetNextWindowSize({ToggleWidth, ToggleHeight}, UiCondition::Always);

    const bool toggleVisible =
        ui.BeginPanel("##EnginePromptToggle", false, false, false);
    if (toggleVisible) {
      const char *toggleLabel = expanded_ ? "<" : ">";
      if (ui.Button(toggleLabel, {ToggleButtonSize, ToggleButtonSize})) {
        expanded_ = !expanded_;
      }
    }
    ui.EndPanel();
  }

  if (!expanded_) {
    return;
  }

  ui.SetNextWindowPosition({panelLeft, 0.0f}, UiCondition::Always);
  ui.SetNextWindowSize({effectivePanelWidth, static_cast<float>(screenHeight)},
                       UiCondition::Always);

  const bool panelVisible =
      ui.BeginPanel("Engine Prompt Console", false, false);
  if (panelVisible) {
    ui.Text("Activity", {210, 215, 225, 255});
    if (hasCompletedPrompt_) {
      ui.TextWrapped(FormatCost("Last prompt", lastPromptUsage_,
                                lastPromptCostAvailable_, lastPromptCostUsd_),
                     ui.GetAvailableWidth(), {255, 203, 90, 255});
      ui.TextWrapped(FormatCost("Session total", sessionUsage_,
                                sessionCostAvailable_, sessionCostUsd_),
                     ui.GetAvailableWidth(), {255, 203, 90, 255});
    }
    if (ui.BeginChild("PromptActivity", {0.0f, 130.0f}, true)) {
      for (const std::string &log : activityLogs_) {
        ui.TextWrapped(log, ui.GetAvailableWidth(), InformationColor);
      }
      if (scrollToLatest_) {
        ui.ScrollToBottom();
      }
    }
    ui.EndChild();

    ui.Text("Conversation", {210, 215, 225, 255});
    ui.Separator();

    const std::size_t attachmentRows =
        promptImages_.size() + promptFileAttachments_.size();
    const float attachmentHeight =
        ui.GetInputHeight() * static_cast<float>(attachmentRows * 2);
    const float inputAreaHeight =
        ui.GetInputHeight() * 2.0f + attachmentHeight + 12.0f;
    if (ui.BeginChild("PromptHistory", {0.0f, -inputAreaHeight}, false)) {
      for (const PromptMessage &message : messages_) {
        const float availableWidth = ui.GetAvailableWidth();
        const float messageWidth =
            std::max(80.0f, availableWidth * MessageWidthRatio);
        const bool userMessage = message.role == PromptMessageRole::User;

        if (userMessage) {
          ui.SetCursorX(ui.GetCursorX() +
                        std::max(0.0f, availableWidth - messageWidth));
        }

        Color color = ResultColor;
        if (userMessage) {
          color = UserColor;
        } else if (message.role == PromptMessageRole::Information) {
          color = InformationColor;
        }

        ui.TextWrapped(message.text, messageWidth, color);
        ui.Spacing();
      }

      if (scrollToLatest_) {
        ui.ScrollToBottom();
        scrollToLatest_ = false;
      }
    }
    ui.EndChild();

    if (focusInput_) {
      ui.SetKeyboardFocusHere();
      focusInput_ = false;
    }

    if (ui.Button("Paste image/MIDI", {145.0f, 0.0f})) {
      if (!PasteClipboardFile())
        PasteClipboardImage();
    }
    for (std::size_t index = 0; index < promptImages_.size();) {
      const AssistantImageInput &image = promptImages_[index];
      ui.Text("Image " + std::to_string(index + 1) + " - " +
                  std::to_string(image.width) + "x" +
                  std::to_string(image.height),
              InformationColor);
      const std::string removeLabel =
          "Remove image " + std::to_string(index + 1) + "##PromptImage";
      if (ui.Button(removeLabel, {130.0f, 0.0f})) {
        promptImages_.erase(promptImages_.begin() +
                            static_cast<std::ptrdiff_t>(index));
      } else {
        ++index;
      }
    }
    for (std::size_t index = 0; index < promptFileAttachments_.size();) {
      ui.Text(promptFileAttachments_[index].displayName, InformationColor);
      const std::string removeLabel =
          "Remove file " + std::to_string(index + 1) + "##PromptFile";
      if (ui.Button(removeLabel, {130.0f, 0.0f})) {
        promptFileAttachments_.erase(promptFileAttachments_.begin() +
                                     static_cast<std::ptrdiff_t>(index));
      } else {
        ++index;
      }
    }

    const bool submitted = ui.InputText(
        "##PromptInput",
        "Type a prompt, paste an image or MIDI file, and press Enter...",
        promptInput_);
    if (ui.IsPasteShortcutPressed()) {
      if (!PasteClipboardFile())
        PasteClipboardImage();
    }
    if (submitted) {
      SubmitPrompt();
    }
  }
  ui.EndPanel();
}

bool PromptConsole::PasteClipboardFile() {
  if (attachmentProvider_ == nullptr)
    return false;
  if (pendingRequest_.valid()) {
    activityLogs_.push_back(
        "Wait for the current response before attaching a file.");
    return true;
  }
  const PromptAttachmentResult result =
      attachmentProvider_->PasteClipboardFile();
  if (!result.handled)
    return false;
  if (promptFileAttachments_.size() >= MaximumPromptFileAttachments) {
    activityLogs_.push_back("A prompt can contain at most two files.");
    return true;
  }
  if (result.attachment)
    promptFileAttachments_.push_back(*result.attachment);
  if (!result.message.empty())
    activityLogs_.push_back(result.message);
  scrollToLatest_ = true;
  return true;
}

void PromptConsole::PollAutomaticPrompt() {
  if (options_.automaticPromptFile.empty() || pendingRequest_.valid() ||
      !std::filesystem::exists(options_.automaticPromptFile)) {
    return;
  }

  std::error_code error;
  const auto writeTime =
      std::filesystem::last_write_time(options_.automaticPromptFile, error);
  if (error ||
      (lastAutomaticPromptTime_ && *lastAutomaticPromptTime_ == writeTime)) {
    return;
  }

  std::ifstream stream(options_.automaticPromptFile, std::ios::binary);
  if (!stream) {
    return;
  }
  std::string prompt{std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>()};
  stream.close();
  lastAutomaticPromptTime_ = writeTime;
  std::filesystem::remove(options_.automaticPromptFile, error);
  if (prompt.empty()) {
    return;
  }

  Engine::Logger::Warning(
      "Starting automatic AI crash recovery investigation.");
  promptInput_ = std::move(prompt);
  SubmitPrompt();
}

void PromptConsole::PasteClipboardImage() {
  if (pendingRequest_.valid()) {
    activityLogs_.push_back(
        "Wait for the current response before attaching an image.");
    return;
  }
  if (promptImages_.size() >= MaximumPromptImages) {
    activityLogs_.push_back("A prompt can contain at most four images.");
    return;
  }

  Image image = GetClipboardImage();
  if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
    activityLogs_.push_back("The clipboard does not contain an image.");
    return;
  }

  const int originalWidth = image.width;
  const int originalHeight = image.height;
  if (image.width > MaximumImageDimension ||
      image.height > MaximumImageDimension) {
    const float scale = static_cast<float>(MaximumImageDimension) /
                        static_cast<float>(std::max(image.width, image.height));
    ImageResize(&image, std::max(1, static_cast<int>(image.width * scale)),
                std::max(1, static_cast<int>(image.height * scale)));
  }

  int pngSize = 0;
  unsigned char *png = ExportImageToMemory(image, ".png", &pngSize);
  UnloadImage(image);
  if (png == nullptr || pngSize <= 0) {
    if (png != nullptr)
      MemFree(png);
    activityLogs_.push_back("Could not encode the clipboard image.");
    return;
  }

  promptImages_.push_back(
      {.mimeType = "image/png",
       .base64Data = EncodeBase64(png, static_cast<std::size_t>(pngSize)),
       .width = image.width,
       .height = image.height});
  MemFree(png);
  activityLogs_.push_back("Attached clipboard image " +
                          std::to_string(originalWidth) + "x" +
                          std::to_string(originalHeight) +
                          (originalWidth != promptImages_.back().width ||
                                   originalHeight != promptImages_.back().height
                               ? " (resized for upload)."
                               : "."));
  scrollToLatest_ = true;
}

void PromptConsole::SubmitPrompt() {
  if (pendingRequest_.valid()) {
    messages_.push_back(
        {PromptMessageRole::Information,
         "Wait for the current response before sending another prompt."});
    scrollToLatest_ = true;
    return;
  }

  const auto firstContent = promptInput_.find_first_not_of(" \t\r\n");
  if (firstContent == std::string::npos && promptImages_.empty() &&
      promptFileAttachments_.empty()) {
    return;
  }

  std::string prompt = promptFileAttachments_.empty()
                           ? "Use the attached image as a visual reference."
                           : "Use the attached MIDI as a musical reference.";
  if (firstContent != std::string::npos) {
    const auto lastContent = promptInput_.find_last_not_of(" \t\r\n");
    prompt = promptInput_.substr(firstContent, lastContent - firstContent + 1);
  }
  auto images = std::move(promptImages_);
  auto fileAttachments = std::move(promptFileAttachments_);
  for (const PromptTextAttachment &attachment : fileAttachments)
    prompt += attachment.promptText;

  Engine::Logger::Info(
      "User prompt submitted:\n" +
      prompt.substr(0, std::min<std::size_t>(prompt.size(), 4'000)));
  const std::string attachmentSummary =
      (images.empty()
           ? ""
           : "\n[" + std::to_string(images.size()) + " image(s) attached]") +
      (fileAttachments.empty()
           ? ""
           : "\n[" + std::to_string(fileAttachments.size()) +
                 " file reference(s) attached]");
  messages_.push_back({PromptMessageRole::User, prompt + attachmentSummary});
  activityLogs_.push_back("Request queued.");
  activeResponseIndex_.reset();
  activeReasoningLogIndex_.reset();

  pendingRequest_ =
      std::async(std::launch::async, [this, prompt = std::move(prompt),
                                      images = std::move(images)]() {
        return processor_.Process(prompt, images,
                                  [this](const AssistantStreamEvent &event) {
                                    std::scoped_lock lock(streamEventsMutex_);
                                    pendingStreamEvents_.push_back(event);
                                  });
      });

  promptInput_.clear();
  promptImages_.clear();
  promptFileAttachments_.clear();
  scrollToLatest_ = true;
  focusInput_ = true;
}

void PromptConsole::PollPendingRequest() {
  if (!pendingRequest_.valid() || pendingRequest_.wait_for(std::chrono::seconds(
                                      0)) != std::future_status::ready) {
    return;
  }

  PromptProcessResult result = pendingRequest_.get();
  lastPromptUsage_ = result.usage;
  sessionUsage_ += result.usage;
  lastPromptCostUsd_ = result.estimatedCostUsd;
  sessionCostUsd_ += result.estimatedCostUsd;
  lastPromptCostAvailable_ = result.costAvailable;
  sessionCostAvailable_ &= result.costAvailable;
  hasCompletedPrompt_ = true;

  messages_.insert(messages_.end(),
                   std::make_move_iterator(result.messages.begin()),
                   std::make_move_iterator(result.messages.end()));
  scrollToLatest_ = true;
  Engine::Logger::Info(FormatCost("Completed prompt estimated cost",
                                  lastPromptUsage_, lastPromptCostAvailable_,
                                  lastPromptCostUsd_) +
                       "; " +
                       FormatCost("session", sessionUsage_,
                                  sessionCostAvailable_, sessionCostUsd_));
  Engine::Logger::Info("Pending assistant request joined by the UI thread.");
}

void PromptConsole::PollStreamEvents() {
  std::vector<AssistantStreamEvent> events;
  {
    std::scoped_lock lock(streamEventsMutex_);
    events.swap(pendingStreamEvents_);
  }

  for (const AssistantStreamEvent &event : events) {
    switch (event.type) {
    case AssistantStreamEventType::Status:
      activityLogs_.push_back(event.text);
      Engine::Logger::Info("Assistant status: " + event.text);
      break;

    case AssistantStreamEventType::ReasoningSummaryDelta:
      if (!activeReasoningLogIndex_) {
        activityLogs_.push_back("Reasoning summary: ");
        activeReasoningLogIndex_ = activityLogs_.size() - 1;
      }
      activityLogs_[*activeReasoningLogIndex_] += event.text;
      break;

    case AssistantStreamEventType::OutputTextDelta:
      if (!activeResponseIndex_) {
        messages_.push_back({PromptMessageRole::Result, ""});
        activeResponseIndex_ = messages_.size() - 1;
      }
      messages_[*activeResponseIndex_].text += event.text;
      break;
    }
  }

  if (!events.empty()) {
    scrollToLatest_ = true;
  }
}
} // namespace AssistantHost

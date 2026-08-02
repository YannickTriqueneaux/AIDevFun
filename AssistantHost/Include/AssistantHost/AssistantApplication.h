#pragma once

#include "AssistantHost/Settings.h"

#include <filesystem>
#include <string>

namespace AssistantHost {
class PromptAttachmentProvider;

class AssistantApplication {
public:
  AssistantApplication(LauncherSettings settings, std::string gameName,
                       std::filesystem::path executableDirectory,
                       std::filesystem::path gameRoot,
                       PromptAttachmentProvider *attachmentProvider = nullptr);
  ~AssistantApplication();

  AssistantApplication(const AssistantApplication &) = delete;
  AssistantApplication &operator=(const AssistantApplication &) = delete;

  void Run();

private:
  struct Implementation;
  Implementation *implementation_ = nullptr;
};
} // namespace AssistantHost

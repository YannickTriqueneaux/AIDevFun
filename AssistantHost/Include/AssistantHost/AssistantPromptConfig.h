#pragma once

#include <filesystem>
#include <string>

namespace AssistantHost {
class AssistantPromptConfig {
public:
  [[nodiscard]] static AssistantPromptConfig
  Load(const std::filesystem::path &path);

  [[nodiscard]] const std::string &GetGameDeveloperInstructions() const;

private:
  std::string gameDeveloperInstructions_;
};
} // namespace AssistantHost
#include "AssistantHost/AssistantPromptConfig.h"

#include <fstream>
#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

namespace AssistantHost {
namespace {
std::string JoinInstructionBlocks(const nlohmann::json &blocks) {
  if (!blocks.is_array() || blocks.empty())
    throw std::runtime_error(
        "gameDeveloperInstructions must be a non-empty JSON array.");

  std::string instructions;
  for (const nlohmann::json &block : blocks) {
    if (!block.is_string() || block.get_ref<const std::string &>().empty())
      throw std::runtime_error(
          "Every gameDeveloperInstructions entry must be a non-empty string.");
    if (!instructions.empty())
      instructions += "\n\n";
    instructions += block.get_ref<const std::string &>();
  }
  return instructions;
}
} // namespace

AssistantPromptConfig
AssistantPromptConfig::Load(const std::filesystem::path &path) {
  std::ifstream stream(path);
  if (!stream)
    throw std::runtime_error("Assistant prompt file not found: " +
                             path.string());

  nlohmann::json document;
  try {
    stream >> document;
    AssistantPromptConfig config;
    config.gameDeveloperInstructions_ =
        JoinInstructionBlocks(document.at("gameDeveloperInstructions"));
    return config;
  } catch (const nlohmann::json::exception &exception) {
    throw std::runtime_error("Invalid assistant prompt file " + path.string() +
                             ": " + exception.what());
  }
}

const std::string &AssistantPromptConfig::GetGameDeveloperInstructions() const {
  return gameDeveloperInstructions_;
}
} // namespace AssistantHost
#include "Development/AssistantProvider.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(_WIN32)
#define ASSISTANT_PROVIDER_API extern "C" __declspec(dllexport)
#else
#define ASSISTANT_PROVIDER_API extern "C"
#endif

namespace {
std::vector<unsigned char> DecodeBase64(std::string_view input) {
  static constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<unsigned char> output;
  unsigned int accumulator = 0;
  int bits = -8;
  for (const char character : input) {
    if (character == '=')
      break;
    const auto value = alphabet.find(character);
    if (value == std::string_view::npos)
      continue;
    accumulator = (accumulator << 6) | static_cast<unsigned int>(value);
    bits += 6;
    if (bits >= 0) {
      output.push_back(
          static_cast<unsigned char>((accumulator >> bits) & 0xff));
      bits -= 8;
    }
  }
  return output;
}

std::string ImageExtension(std::string_view mimeType) {
  if (mimeType == "image/jpeg")
    return ".jpg";
  if (mimeType == "image/webp")
    return ".webp";
  return ".png";
}

std::filesystem::path ResolveExecutable(std::string configured) {
  if (!configured.empty())
    return configured;
#if defined(_WIN32)
  if (const char *appData = std::getenv("APPDATA")) {
    const auto npmCodex = std::filesystem::path(appData) / "npm/codex.cmd";
    if (std::filesystem::exists(npmCodex))
      return npmCodex;
  }
#endif
  return "codex";
}

class CodexProvider final : public Development::AssistantProvider {
public:
  CodexProvider(const char *settingsPath, const char *gameRoot)
      : gameRoot_(gameRoot) {
    std::ifstream stream(settingsPath);
    nlohmann::json settings;
    stream >> settings;
    executable_ = ResolveExecutable(settings.value("executable", ""));
    model_ = settings.value("model", "");
    reasoningEffort_ = settings.value("reasoningEffort", "high");
  }

  bool IsConfigured() const override { return !executable_.empty(); }
  const std::string &GetDisplayName() const override { return displayName_; }
  const std::string &GetModel() const override {
    return model_.empty() ? displayName_ : model_;
  }
  Development::AssistantCostEstimate
  EstimateCost(const Development::AssistantTokenUsage &) const override {
    return {};
  }

  Development::AssistantResponse CreateResponse(
      std::string_view instructions, std::string_view prompt,
      const std::vector<Development::AssistantImageInput> &images,
      std::string_view,
      const Development::AssistantStreamCallback &onEvent) const override {
    const auto nonce = std::to_string(GetCurrentProcessId()) + "_" +
                       std::to_string(GetTickCount64());
    const auto promptPath = std::filesystem::temp_directory_path() /
                            ("aitester_codex_" + nonce + ".txt");
    const auto outputPath = std::filesystem::temp_directory_path() /
                            ("aitester_codex_" + nonce + ".out");
    {
      std::ofstream stream(promptPath, std::ios::binary);
      stream
          << instructions
          << "\n\nWhen using this provider, use Codex's native filesystem and "
             "shell tools inside the active Game directory.\n\n"
          << prompt;
    }
    std::vector<std::filesystem::path> imagePaths;
    imagePaths.reserve(images.size());
    for (std::size_t index = 0; index < images.size(); ++index) {
      const auto imagePath =
          std::filesystem::temp_directory_path() /
          ("aitester_codex_" + nonce + "_image_" + std::to_string(index) +
           ImageExtension(images[index].mimeType));
      const auto bytes = DecodeBase64(images[index].base64Data);
      std::ofstream image(imagePath, std::ios::binary);
      image.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
      imagePaths.push_back(imagePath);
    }
    onEvent({Development::AssistantStreamEventType::Status,
             "Codex is working through the ChatGPT account."});
    std::string command = "cmd /d /s /c \"\"" + executable_.string() +
                          "\" exec --json --color never --sandbox "
                          "workspace-write --skip-git-repo-check -C \"" +
                          gameRoot_.string() + "\"";
    if (!model_.empty())
      command += " --model \"" + model_ + "\"";
    if (!reasoningEffort_.empty())
      command += " -c model_reasoning_effort=\"" + reasoningEffort_ + "\"";
    for (const auto &imagePath : imagePaths)
      command += " --image \"" + imagePath.string() + "\"";
    command += " --output-last-message \"" + outputPath.string() + "\" - < \"" +
               promptPath.string() + "\" 2>&1\"";
    std::array<char, 4096> buffer{};
    FILE *pipe = _popen(command.c_str(), "r");
    if (!pipe)
      throw std::runtime_error("Could not start Codex CLI.");
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    }
    const int exitCode = _pclose(pipe);
    std::ifstream output(outputPath, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(output)), {});
    std::error_code error;
    std::filesystem::remove(promptPath, error);
    std::filesystem::remove(outputPath, error);
    for (const auto &imagePath : imagePaths)
      std::filesystem::remove(imagePath, error);
    if (exitCode != 0)
      throw std::runtime_error("Codex CLI failed with exit code " +
                               std::to_string(exitCode) + ".");
    if (text.empty())
      throw std::runtime_error("Codex returned no final response.");
    onEvent({Development::AssistantStreamEventType::OutputTextDelta, text});
    return {.id = nonce, .text = text};
  }

  Development::AssistantResponse ContinueWithToolOutputs(
      std::string_view, const std::vector<Development::AssistantToolOutput> &,
      std::string_view, const Development::AssistantStreamCallback &,
      bool) const override {
    throw std::runtime_error("Codex provider uses its native agent tools.");
  }

private:
  std::filesystem::path gameRoot_;
  std::filesystem::path executable_;
  std::string model_;
  std::string reasoningEffort_;
  std::string displayName_ = "Codex (ChatGPT account)";
};

Development::AssistantProvider *Create(const char *settingsPath,
                                       const char *gameRoot) {
  try {
    return new CodexProvider(settingsPath, gameRoot);
  } catch (...) {
    return nullptr;
  }
}
void Destroy(Development::AssistantProvider *provider) { delete provider; }
} // namespace

ASSISTANT_PROVIDER_API const Development::AssistantProviderApi *
GetAssistantProviderApi() {
  static const Development::AssistantProviderApi api{
      Development::AssistantProviderApiVersion, &Create, &Destroy};
  return &api;
}

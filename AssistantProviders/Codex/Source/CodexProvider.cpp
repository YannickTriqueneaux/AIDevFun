#include "AssistantProviders/Codex/CodexEventParser.h"
#include "Development/AssistantProvider.h"

#include <array>
#include <cstdio>
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
  std::array<wchar_t, 32768> appData{};
  const DWORD length = GetEnvironmentVariableW(
      L"APPDATA", appData.data(), static_cast<DWORD>(appData.size()));
  if (length > 0 && length < appData.size()) {
    const auto npmCodex =
        std::filesystem::path(std::wstring_view(appData.data(), length)) /
        "npm/codex.cmd";
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
    mcpExecutable_ = settings.value("gameToolsMcpExecutable", "");
    if (mcpExecutable_.empty())
      mcpExecutable_ = std::filesystem::path(settingsPath).parent_path() /
                       "GameToolsMcpServer.exe";
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
      stream << instructions
             << "\n\nUse only the MakeYourOwnGame.GameTools MCP tools for all "
                "Game and Engine inspection, searches, file changes, builds, "
                "reloads, and crash recovery. Do not use a shell or direct "
                "filesystem tools. Use tool discovery to find the required "
                "game_tools MCP operations before calling them. Before any "
                "implementation, call list_agent_skills and read every "
                "applicable skill. If none applies, call "
                "confirm_no_applicable_skills with a concrete reason. Then "
                "call list_agent_documents and read "
                "Architecture.md. Read AIProviders.md for provider work. "
                "Prefer cohesive design over minimizing file or type count. "
                "Create new Game files, EntityTypes, and focused "
                "ComponentTypes whenever a concept has its own lifecycle, "
                "state, behavior, or resume boundary; do not accumulate "
                "unrelated features in existing catch-all files.\n\n"
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
                          "\" exec --json --color never --sandbox read-only "
                          "--disable shell_tool --disable unified_exec "
                          "--enable mcp_2026_07_28 "
                          "--skip-git-repo-check -C \"" +
                          gameRoot_.string() + "\"";
    command += " -c suppress_unstable_features_warning=true";
    std::string mcpPath = mcpExecutable_.generic_string();
    command += " -c \"mcp_servers.game_tools.command='" + mcpPath + "'\"";
    if (!model_.empty())
      command += " --model \"" + model_ + "\"";
    if (!reasoningEffort_.empty())
      command += " -c model_reasoning_effort=\"" + reasoningEffort_ + "\"";
    for (const auto &imagePath : imagePaths)
      command += " --image \"" + imagePath.string() + "\"";
    command += " --output-last-message \"" + outputPath.string() + "\" - < \"" +
               promptPath.string() + "\" 2>&1\"";
    std::array<char, 4096> buffer{};
    std::string pendingLine;
    std::string diagnostics;
    Development::AssistantTokenUsage usage;
    std::string responseId = nonce;
    FILE *pipe = _popen(command.c_str(), "r");
    if (!pipe)
      throw std::runtime_error("Could not start Codex CLI.");
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
      pendingLine += buffer.data();
      std::size_t newline = 0;
      while ((newline = pendingLine.find('\n')) != std::string::npos) {
        std::string line = pendingLine.substr(0, newline);
        pendingLine.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        diagnostics += line + '\n';
        if (diagnostics.size() > 16384)
          diagnostics.erase(0, diagnostics.size() - 16384);
        const auto parsed = AssistantProviders::Codex::ParseEventLine(line);
        for (const auto &event : parsed.events)
          onEvent(event);
        if (parsed.usage)
          usage = *parsed.usage;
        if (!parsed.threadId.empty())
          responseId = parsed.threadId;
      }
    }
    if (!pendingLine.empty()) {
      diagnostics += pendingLine;
      const auto parsed =
          AssistantProviders::Codex::ParseEventLine(pendingLine);
      for (const auto &event : parsed.events)
        onEvent(event);
      if (parsed.usage)
        usage = *parsed.usage;
      if (!parsed.threadId.empty())
        responseId = parsed.threadId;
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
                               std::to_string(exitCode) + ". Recent output:\n" +
                               diagnostics);
    if (text.empty())
      throw std::runtime_error("Codex returned no final response.");
    onEvent({Development::AssistantStreamEventType::OutputTextDelta, text});
    return {.id = responseId, .text = text, .usage = usage};
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
  std::filesystem::path mcpExecutable_;
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

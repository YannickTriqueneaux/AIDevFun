#include "AssistantProviders/Claude/ClaudeEventParser.h"
#include "Development/AssistantProvider.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define ASSISTANT_PROVIDER_API extern "C" __declspec(dllexport)
#else
#define ASSISTANT_PROVIDER_API extern "C"
#endif

namespace {
std::filesystem::path ResolveExecutable(std::string configured) {
  if (!configured.empty())
    return configured;
#if defined(_WIN32)
  std::array<wchar_t, 32768> userProfile{};
  const DWORD profileLength = GetEnvironmentVariableW(
      L"USERPROFILE", userProfile.data(),
      static_cast<DWORD>(userProfile.size()));
  if (profileLength > 0 && profileLength < userProfile.size()) {
    const auto nativeClaude =
        std::filesystem::path(
            std::wstring_view(userProfile.data(), profileLength)) /
        ".local/bin/claude.exe";
    if (std::filesystem::exists(nativeClaude))
      return nativeClaude;
  }
  std::array<wchar_t, 32768> appData{};
  const DWORD appDataLength = GetEnvironmentVariableW(
      L"APPDATA", appData.data(), static_cast<DWORD>(appData.size()));
  if (appDataLength > 0 && appDataLength < appData.size()) {
    const auto npmClaude =
        std::filesystem::path(
            std::wstring_view(appData.data(), appDataLength)) /
        "npm/claude.cmd";
    if (std::filesystem::exists(npmClaude))
      return npmClaude;
  }
#endif
  return "claude";
}

bool IsValidSessionId(std::string_view id) {
  return id.size() <= 128 &&
         std::all_of(id.begin(), id.end(), [](unsigned char character) {
           return std::isalnum(character) || character == '-' ||
                  character == '_';
         });
}

class ClaudeProvider final : public Development::AssistantProvider {
public:
  ClaudeProvider(const char *settingsPath, const char *gameRoot)
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
    effort_ = settings.value("effort", "low");
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
      std::string_view previousResponseId,
      const Development::AssistantStreamCallback &onEvent) const override {
#if !defined(_WIN32)
    throw std::runtime_error("Claude provider currently requires Windows.");
#else
    if (!std::filesystem::exists(mcpExecutable_))
      throw std::runtime_error("Game Tools MCP executable is missing: " +
                               mcpExecutable_.string());
    if (!previousResponseId.empty() && !IsValidSessionId(previousResponseId))
      throw std::runtime_error("Invalid Claude session ID.");

    const std::string nonce = std::to_string(GetCurrentProcessId()) + "_" +
                              std::to_string(GetTickCount64());
    const auto temp = std::filesystem::temp_directory_path();
    const auto inputPath = temp / ("aitester_claude_" + nonce + ".jsonl");
    const auto promptPath = temp / ("aitester_claude_" + nonce + ".prompt");
    const auto mcpPath = temp / ("aitester_claude_" + nonce + ".mcp.json");

    {
      std::ofstream systemPrompt(promptPath, std::ios::binary);
      systemPrompt << instructions;
    }
    {
      nlohmann::json content = nlohmann::json::array(
          {{{"type", "text"}, {"text", std::string(prompt)}}});
      for (const auto &image : images) {
        content.push_back({{"type", "image"},
                           {"source", {{"type", "base64"},
                                       {"media_type", image.mimeType},
                                       {"data", image.base64Data}}}});
      }
      const nlohmann::json input =
          {{"type", "user"},
           {"message", {{"role", "user"}, {"content", content}}}};
      std::ofstream file(inputPath, std::ios::binary);
      file << input.dump() << '\n';
    }
    {
      nlohmann::json args = nlohmann::json::array();
      if (!previousResponseId.empty())
        args.push_back("--guidance-already-read");
      const nlohmann::json config =
          {{"mcpServers",
            {{"game_tools", {{"type", "stdio"},
                              {"command", mcpExecutable_.string()},
                              {"args", args},
                              {"env", {{"MCP_TIMEOUT", "10000"}}}}}}}};
      std::ofstream file(mcpPath, std::ios::binary);
      file << config.dump(2);
    }

    onEvent({Development::AssistantStreamEventType::Status,
             "Claude is working through the configured Claude account."});
    onEvent({Development::AssistantStreamEventType::Status,
             "Starting required Game tools (up to 10 seconds)."});

    std::string command = "cmd /d /s /c \"\"" + executable_.string() +
                          "\" -p --input-format stream-json "
                          "--output-format stream-json --verbose "
                          "--strict-mcp-config --mcp-config \"" +
                          mcpPath.string() +
                          "\" --tools \"WebSearch,WebFetch\" --allowedTools "
                          "\"mcp__game_tools__*,WebSearch,WebFetch\" "
                          "--append-system-prompt-file \"" +
                          promptPath.string() + "\"";
    if (!model_.empty())
      command += " --model \"" + model_ + "\"";
    if (!effort_.empty())
      command += " --effort \"" + effort_ + "\"";
    if (!previousResponseId.empty()) {
      onEvent({Development::AssistantStreamEventType::Status,
               "Resuming the previous Claude conversation."});
      command += " --resume \"" + std::string(previousResponseId) + "\"";
    }
    command += " < \"" + inputPath.string() + "\" 2>&1\"";

    std::array<char, 4096> buffer{};
    std::string pending;
    std::string diagnostics;
    std::string resultText;
    std::string sessionId;
    std::string reportedError;
    Development::AssistantTokenUsage usage;
    FILE *pipe = _popen(command.c_str(), "r");
    if (!pipe)
      throw std::runtime_error("Could not start Claude Code CLI.");
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
      pending += buffer.data();
      std::size_t newline = 0;
      while ((newline = pending.find('\n')) != std::string::npos) {
        std::string line = pending.substr(0, newline);
        pending.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        diagnostics += line + '\n';
        if (diagnostics.size() > 16384)
          diagnostics.erase(0, diagnostics.size() - 16384);
        const auto parsed = AssistantProviders::Claude::ParseEventLine(line);
        for (const auto &event : parsed.events)
          onEvent(event);
        if (parsed.usage)
          usage = *parsed.usage;
        if (!parsed.sessionId.empty())
          sessionId = parsed.sessionId;
        if (!parsed.resultText.empty())
          resultText = parsed.resultText;
        if (parsed.failed)
          reportedError = parsed.error;
      }
    }
    if (!pending.empty()) {
      const auto parsed = AssistantProviders::Claude::ParseEventLine(pending);
      for (const auto &event : parsed.events)
        onEvent(event);
      if (parsed.usage)
        usage = *parsed.usage;
      if (!parsed.sessionId.empty())
        sessionId = parsed.sessionId;
      if (!parsed.resultText.empty())
        resultText = parsed.resultText;
      if (parsed.failed)
        reportedError = parsed.error;
    }
    const int exitCode = _pclose(pipe);
    std::error_code error;
    std::filesystem::remove(inputPath, error);
    std::filesystem::remove(promptPath, error);
    std::filesystem::remove(mcpPath, error);
    if (exitCode != 0 || !reportedError.empty())
      throw std::runtime_error(
          "Claude Code failed" +
          (reportedError.empty() ? std::string{} : ": " + reportedError) +
          ". Recent output:\n" + diagnostics);
    if (resultText.empty())
      throw std::runtime_error("Claude returned no final response.");
    if (sessionId.empty())
      sessionId = nonce;
    onEvent({Development::AssistantStreamEventType::OutputTextDelta,
             resultText});
    return {.id = sessionId, .text = resultText, .usage = usage};
#endif
  }

  Development::AssistantResponse ContinueWithToolOutputs(
      std::string_view, const std::vector<Development::AssistantToolOutput> &,
      std::string_view, const Development::AssistantStreamCallback &,
      bool) const override {
    throw std::runtime_error("Claude provider uses its native agent tools.");
  }

private:
  std::filesystem::path gameRoot_;
  std::filesystem::path executable_;
  std::filesystem::path mcpExecutable_;
  std::string model_;
  std::string effort_;
  std::string displayName_ = "Claude Code (Claude account)";
};

Development::AssistantProvider *Create(const char *settingsPath,
                                       const char *gameRoot) {
  try {
    return new ClaudeProvider(settingsPath, gameRoot);
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

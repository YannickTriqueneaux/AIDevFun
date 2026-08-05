#include "AssistantProviders/Codex/CodexEventParser.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

namespace AssistantProviders::Codex {
namespace {
std::string ReadText(const nlohmann::json &value) {
  if (value.is_string())
    return value.get<std::string>();
  if (value.is_array()) {
    std::string text;
    for (const auto &part : value) {
      const std::string next = ReadText(part);
      if (!next.empty()) {
        if (!text.empty())
          text += '\n';
        text += next;
      }
    }
    return text;
  }
  if (value.is_object()) {
    for (const char *key : {"text", "summary", "message", "content"}) {
      if (value.contains(key)) {
        const std::string text = ReadText(value[key]);
        if (!text.empty())
          return text;
      }
    }
  }
  return {};
}

std::string ReadCommand(const nlohmann::json &item) {
  if (!item.contains("command"))
    return {};
  if (item["command"].is_string())
    return item["command"].get<std::string>();
  if (item["command"].is_array()) {
    std::string command;
    for (const auto &argument : item["command"]) {
      if (!argument.is_string())
        continue;
      if (!command.empty())
        command += ' ';
      command += argument.get<std::string>();
    }
    return command;
  }
  return {};
}

std::string ToolName(const nlohmann::json &item) {
  std::string name = item.value("server", "");
  const std::string tool = item.value("name", item.value("tool", ""));
  if (!name.empty() && !tool.empty())
    name += '.';
  return name + tool;
}

void AddStatus(ParsedEventLine &parsed, std::string text) {
  if (!text.empty())
    parsed.events.push_back(
        {Development::AssistantStreamEventType::Status, std::move(text)});
}
} // namespace

ParsedEventLine ParseEventLine(std::string_view line) {
  ParsedEventLine parsed;
  const nlohmann::json event = nlohmann::json::parse(line, nullptr, false);
  if (event.is_discarded() || !event.is_object()) {
    std::string normalized(line);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                     return static_cast<char>(std::tolower(character));
                   });
    if (normalized.find("mcp") != std::string::npos) {
      constexpr std::size_t MaximumDiagnosticLength = 2048;
      std::string diagnostic(line.substr(0, MaximumDiagnosticLength));
      AddStatus(parsed, "Codex MCP: " + diagnostic);
    }
    return parsed;
  }

  const std::string eventType = event.value("type", "");
  if (eventType == "thread.started") {
    parsed.threadId = event.value("thread_id", "");
    AddStatus(parsed, "Codex session started.");
    return parsed;
  }
  if (eventType == "turn.completed") {
    if (event.contains("usage") && event["usage"].is_object()) {
      const auto &usage = event["usage"];
      parsed.usage = Development::AssistantTokenUsage{
          usage.value("input_tokens", 0ULL),
          usage.value("cached_input_tokens", 0ULL),
          usage.value("output_tokens", 0ULL)};
    }
    AddStatus(parsed, "Codex turn completed.");
    return parsed;
  }
  if (eventType == "turn.failed" || eventType == "error") {
    const std::string message = ReadText(event);
    AddStatus(parsed, message.empty() ? "Codex reported an error."
                                      : "Codex error: " + message);
    return parsed;
  }
  if (eventType != "item.started" && eventType != "item.completed" &&
      eventType != "item.updated")
    return parsed;

  if (!event.contains("item") || !event["item"].is_object())
    return parsed;
  const auto &item = event["item"];
  const std::string itemType = item.value("type", "");
  const bool started = eventType == "item.started";

  if (itemType == "command_execution") {
    if (started) {
      const std::string command = ReadCommand(item);
      AddStatus(parsed, command.empty() ? "Running a command."
                                        : "Running command: " + command);
    } else if (eventType == "item.completed") {
      const int exitCode = item.value("exit_code", 0);
      AddStatus(parsed, exitCode == 0 ? "Command completed."
                                      : "Command failed with exit code " +
                                            std::to_string(exitCode) + ".");
    }
  } else if (itemType == "file_change") {
    AddStatus(parsed,
              started ? "Applying file changes." : "File changes applied.");
  } else if (itemType == "mcp_tool_call" || itemType == "tool_call") {
    const std::string name = ToolName(item);
    AddStatus(parsed, std::string(started ? "Running tool" : "Tool completed") +
                          (name.empty() ? "." : ": " + name));
  } else if (itemType == "web_search") {
    const std::string query = item.value("query", "");
    AddStatus(parsed, query.empty() ? "Searching the web."
                                    : "Searching the web: " + query);
  } else if (itemType == "reasoning" && eventType != "item.started") {
    const std::string text = ReadText(item);
    if (!text.empty())
      parsed.events.push_back(
          {Development::AssistantStreamEventType::ReasoningSummaryDelta, text});
  }
  return parsed;
}

} // namespace AssistantProviders::Codex

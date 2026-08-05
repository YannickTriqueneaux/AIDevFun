#include "AssistantProviders/Claude/ClaudeEventParser.h"

#include <nlohmann/json.hpp>

namespace AssistantProviders::Claude {
namespace {
void AddStatus(ParsedEventLine &parsed, std::string text) {
  if (!text.empty())
    parsed.events.push_back(
        {Development::AssistantStreamEventType::Status, std::move(text)});
}

std::string ToolName(const nlohmann::json &block) {
  std::string name = block.value("name", "");
  constexpr std::string_view prefix = "mcp__";
  if (name.starts_with(prefix)) {
    name.erase(0, prefix.size());
    const auto separator = name.find("__");
    if (separator != std::string::npos)
      name.replace(separator, 2, ".");
  }
  return name;
}
} // namespace

ParsedEventLine ParseEventLine(std::string_view line) {
  ParsedEventLine parsed;
  const auto event = nlohmann::json::parse(line, nullptr, false);
  if (event.is_discarded() || !event.is_object()) {
    if (line.find("MCP") != std::string_view::npos ||
        line.find("mcp") != std::string_view::npos)
      AddStatus(parsed, "Claude MCP: " + std::string(line.substr(0, 2048)));
    return parsed;
  }

  parsed.sessionId = event.value("session_id", "");
  const std::string type = event.value("type", "");
  if (type == "system" && event.value("subtype", "") == "init") {
    AddStatus(parsed, "Claude session started.");
    if (event.contains("mcp_servers") && event["mcp_servers"].is_array()) {
      for (const auto &server : event["mcp_servers"]) {
        const std::string name = server.value("name", "MCP server");
        const std::string status = server.value("status", "unknown");
        AddStatus(parsed, "Claude MCP " + name + ": " + status + ".");
        if (name == "game_tools" && status != "connected") {
          parsed.failed = true;
          parsed.error = "Required Claude MCP server game_tools is " + status;
        }
      }
    }
    return parsed;
  }

  if (type == "assistant" && event.contains("message")) {
    const auto &content = event["message"].value("content", nlohmann::json::array());
    if (content.is_array()) {
      for (const auto &block : content) {
        const std::string blockType = block.value("type", "");
        if (blockType == "tool_use") {
          const std::string name = ToolName(block);
          AddStatus(parsed, name.empty() ? "Running a Claude tool."
                                         : "Running tool: " + name);
        } else if (blockType == "thinking") {
          const std::string thinking = block.value("thinking", "");
          if (!thinking.empty())
            parsed.events.push_back(
                {Development::AssistantStreamEventType::ReasoningSummaryDelta,
                 thinking});
        }
      }
    }
    return parsed;
  }

  if (type == "user" && event.contains("message")) {
    const auto &content = event["message"].value("content", nlohmann::json::array());
    if (content.is_array()) {
      for (const auto &block : content) {
        if (block.value("type", "") == "tool_result")
          AddStatus(parsed, "Claude tool completed.");
      }
    }
    return parsed;
  }

  if (type == "result") {
    parsed.resultText = event.value("result", "");
    parsed.failed = event.value("is_error", false) ||
                    event.value("subtype", "success") != "success";
    parsed.error = parsed.failed ? parsed.resultText : std::string{};
    if (event.contains("usage") && event["usage"].is_object()) {
      const auto &usage = event["usage"];
      parsed.usage = Development::AssistantTokenUsage{
          usage.value("input_tokens", 0ULL),
          usage.value("cache_read_input_tokens", 0ULL),
          usage.value("output_tokens", 0ULL)};
    }
    AddStatus(parsed, parsed.failed ? "Claude turn failed."
                                    : "Claude turn completed.");
  }
  return parsed;
}
} // namespace AssistantProviders::Claude

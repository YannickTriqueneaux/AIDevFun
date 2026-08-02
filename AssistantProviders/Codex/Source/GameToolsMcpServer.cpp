#include "Development/GameToolsProtocol.h"
#include "Engine/IPC/NamedPipe.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace {
using Json = nlohmann::json;

Json Schema(Json properties = Json::object(), Json required = Json::array()) {
  return {{"type", "object"},
          {"properties", std::move(properties)},
          {"required", std::move(required)},
          {"additionalProperties", false}};
}

Json Tool(const char *name, const char *description, Json schema = Schema()) {
  const std::string_view toolName(name);
  const bool readOnly =
      toolName.starts_with("list_") || toolName.starts_with("read_") ||
      toolName.starts_with("search_") || toolName.starts_with("get_") ||
      toolName == "inspect_crash_diagnostics" ||
      toolName == "game_tools_status";
  const bool destructive = toolName == "delete_game_code_file";
  return {{"name", name},
          {"description", description},
          {"inputSchema", std::move(schema)},
          {"annotations",
           {{"readOnlyHint", readOnly},
            {"destructiveHint", destructive},
            {"idempotentHint", readOnly},
            {"openWorldHint", false}}}};
}

Json Tools() {
  const Json path = {{"path", {{"type", "string"}}}};
  const Json paths = {{"paths",
                       {{"type", "array"},
                        {"items", {{"type", "string"}}},
                        {"minItems", 1},
                        {"maxItems", 16}}}};
  return Json::array(
      {Tool("game_tools_status",
            "Verify that the restricted Game Tools MCP bridge is available."),
       Tool("list_game_files", "List editable C++ files in the active Game."),
       Tool("list_agent_skills", "List repository skills before editing."),
       Tool("read_agent_skill", "Read one repository skill completely.",
            Schema({{"name", {{"type", "string"}}}}, {"name"})),
       Tool("read_game_file", "Read one active Game C++ file.",
            Schema(path, {"path"})),
       Tool("read_game_files", "Read up to 16 active Game C++ files.",
            Schema(paths, {"paths"})),
       Tool("search_game_code", "Search exact text in active Game C++ files.",
            Schema({{"query", {{"type", "string"}}}}, {"query"})),
       Tool("list_engine_files", "List C++ files in the read-only Engine."),
       Tool("read_engine_file", "Read one file from the read-only Engine.",
            Schema(path, {"path"})),
       Tool("read_engine_files", "Read up to 16 read-only Engine files.",
            Schema(paths, {"paths"})),
       Tool("search_engine_code", "Search exact text in the read-only Engine.",
            Schema({{"query", {{"type", "string"}}}}, {"query"})),
       Tool("apply_game_patch",
            "Replace one unique exact block in a Game file.",
            Schema({{"path", {{"type", "string"}}},
                    {"oldText", {{"type", "string"}}},
                    {"newText", {{"type", "string"}}}},
                   {"path", "oldText", "newText"})),
       Tool("apply_game_patches",
            "Atomically apply up to 32 exact replacements across Game files.",
            Schema({{"patches",
                     {{"type", "array"},
                      {"minItems", 1},
                      {"maxItems", 32},
                      {"items",
                       {{"type", "object"},
                        {"properties",
                         {{"path", {{"type", "string"}}},
                          {"oldText", {{"type", "string"}}},
                          {"newText", {{"type", "string"}}}}},
                        {"required", {"path", "oldText", "newText"}},
                        {"additionalProperties", false}}}}}},
                   {"patches"})),
       Tool("create_game_code_file",
            "Create a validated new .cpp or .h file in the active Game.",
            Schema({{"path", {{"type", "string"}}},
                    {"content", {{"type", "string"}}}},
                   {"path", "content"})),
       Tool("delete_game_code_file",
            "Delete an obsolete .cpp or .h file from the active Game.",
            Schema(path, {"path"})),
       Tool("build_game", "Compile the controlled Debug Game DLL target."),
       Tool("read_build_output", "Read the latest controlled build output."),
       Tool("reload_game", "Reload the latest successful Game DLL."),
       Tool("get_reload_status", "Read the latest Game reload status."),
       Tool("inspect_crash_diagnostics", "Read current crash diagnostics."),
       Tool("launch_game", "Launch a repaired Game in crash recovery mode.")});
}

Json Result(const Json &id, Json result) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

Json Error(const Json &id, int code, std::string message) {
  return {{"jsonrpc", "2.0"},
          {"id", id},
          {"error", {{"code", code}, {"message", std::move(message)}}}};
}

Json Handle(const Json &request) {
  const Json id = request.value("id", Json(nullptr));
  const std::string method = request.value("method", "");
  if (method == "initialize") {
    return Result(
        id, {{"protocolVersion", "2025-06-18"},
             {"capabilities", {{"tools", Json::object()}}},
             {"serverInfo",
              {{"name", "MakeYourOwnGame.GameTools"}, {"version", "1.0"}}}});
  }
  if (method == "ping")
    return Result(id, Json::object());
  if (method == "tools/list")
    return Result(id, {{"tools", Tools()}});
  if (method == "tools/call") {
    const Json params = request.value("params", Json::object());
    const std::string name = params.value("name", "");
    const Json arguments = params.value("arguments", Json::object());
    if (name == "game_tools_status")
      return Result(
          id, {{"content", Json::array({{{"type", "text"},
                                         {"text", "Game Tools MCP ready."}}})},
               {"isError", false}});
    const Json gameRequest{{"command", name}, {"arguments", arguments}};
    const std::string raw = Engine::NamedPipeClient{}.Request(
        Development::GetGameToolsPipeName(), gameRequest.dump(), 30'000);
    const Json gameResponse = Json::parse(raw);
    const bool ok = gameResponse.value("ok", false);
    return Result(
        id, {{"content", Json::array({{{"type", "text"}, {"text", raw}}})},
             {"isError", !ok}});
  }
  return Error(id, -32601, "Method not found.");
}
} // namespace

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);
  if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
    const Json initialized =
        Handle({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}});
    const Json listed =
        Handle({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}});
    const Json status = Handle(
        {{"jsonrpc", "2.0"},
         {"id", 3},
         {"method", "tools/call"},
         {"params",
          {{"name", "game_tools_status"}, {"arguments", Json::object()}}}});
    const bool passed = initialized.contains("result") &&
                        listed["result"]["tools"].size() >= 20 &&
                        status["result"].value("isError", true) == false;
    return passed ? 0 : 1;
  }
  std::ofstream diagnostics;
  if (argc == 3 && std::string_view(argv[1]) == "--diagnostics")
    diagnostics.open(argv[2], std::ios::app);
  if (diagnostics)
    diagnostics << "started\n" << std::flush;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (diagnostics)
      diagnostics << "request " << line << '\n' << std::flush;
    try {
      const Json request = Json::parse(line);
      if (!request.contains("id"))
        continue;
      const std::string response = Handle(request).dump();
      if (diagnostics)
        diagnostics << "response " << response << '\n' << std::flush;
      std::cout << response << '\n' << std::flush;
    } catch (const std::exception &exception) {
      std::cout << Error(nullptr, -32603, exception.what()).dump() << '\n'
                << std::flush;
    }
  }
}

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
      toolName == "game_tools_status" ||
      toolName == "confirm_no_applicable_skills";
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
       Tool("confirm_no_applicable_skills",
            "Confirm that no listed repository skill applies. Use only after "
            "list_agent_skills and provide a concrete reason.",
            Schema({{"reason", {{"type", "string"}}}}, {"reason"})),
       Tool("list_agent_documents",
            "List top-level read-only documents intended for agents."),
       Tool("read_agent_document", "Read one agent document completely.",
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
       Tool("replace_game_code_file",
            "Atomically replace one complete existing .cpp or .h Game file "
            "with validated C++ source. Use for intentional whole-file "
            "rewrites; use exact patches for localized edits.",
            Schema({{"path", {{"type", "string"}}},
                    {"content", {{"type", "string"}}}},
                   {"path", "content"})),
       Tool("create_game_code_file",
            "Create a validated .cpp or .h file for a cohesive new Game "
            "feature or type instead of growing an unrelated catch-all file.",
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

struct GuidanceState {
  bool inspectedSkills = false;
  bool readApplicableSkill = false;
  bool readArchitecture = false;
};

GuidanceState guidanceState;

void MarkGuidanceAsInherited() {
  guidanceState = {.inspectedSkills = true,
                   .readApplicableSkill = true,
                   .readArchitecture = true};
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
    if (name == "confirm_no_applicable_skills") {
      const std::string reason = arguments.value("reason", "");
      if (!guidanceState.inspectedSkills || reason.size() < 12)
        return Result(
            id, {{"content",
                  Json::array({{{"type", "text"},
                                {"text", "List skills first and provide a "
                                         "concrete reason."}}})},
                 {"isError", true}});
      guidanceState.readApplicableSkill = true;
      return Result(
          id, {{"content",
                Json::array({{{"type", "text"},
                              {"text", "Skill review completed: " + reason}}})},
               {"isError", false}});
    }
    const bool mutatesGame =
        name == "apply_game_patch" || name == "apply_game_patches" ||
        name == "create_game_code_file" || name == "replace_game_code_file" ||
        name == "delete_game_code_file" || name == "build_game" ||
        name == "reload_game" || name == "launch_game";
    if (mutatesGame &&
        (!guidanceState.inspectedSkills || !guidanceState.readApplicableSkill ||
         !guidanceState.readArchitecture)) {
      return Result(
          id,
          {{"content",
            Json::array(
                {{{"type", "text"},
                  {"text",
                   "Required agent guidance has not been inspected. Call "
                   "list_agent_skills, read every applicable skill, and read "
                   "Architecture.md with read_agent_document before changing "
                   "or building the Game."}}})},
           {"isError", true}});
    }
    const Json gameRequest{{"command", name}, {"arguments", arguments}};
    const std::string raw = Engine::NamedPipeClient{}.Request(
        Development::GetGameToolsPipeName(), gameRequest.dump(), 30'000);
    const Json gameResponse = Json::parse(raw);
    const bool ok = gameResponse.value("ok", false);
    if (ok && name == "list_agent_skills")
      guidanceState.inspectedSkills = true;
    if (ok && name == "read_agent_skill")
      guidanceState.readApplicableSkill = true;
    if (ok && name == "read_agent_document" &&
        arguments.value("name", "") == "Architecture.md")
      guidanceState.readArchitecture = true;
    return Result(
        id, {{"content", Json::array({{{"type", "text"}, {"text", raw}}})},
             {"isError", !ok}});
  }
  return Error(id, -32601, "Method not found.");
}
} // namespace

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);
  const bool inheritedGuidance =
      argc == 2 && std::string_view(argv[1]) == "--guidance-already-read";
  if (inheritedGuidance)
    MarkGuidanceAsInherited();
  if (argc == 2 &&
      std::string_view(argv[1]) == "--self-test-inherited-guidance") {
    MarkGuidanceAsInherited();
    return guidanceState.inspectedSkills && guidanceState.readApplicableSkill &&
                   guidanceState.readArchitecture
               ? 0
               : 1;
  }
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
    const Json gatedBuild = Handle(
        {{"jsonrpc", "2.0"},
         {"id", 4},
         {"method", "tools/call"},
         {"params", {{"name", "build_game"}, {"arguments", Json::object()}}}});
    const bool passed = initialized.contains("result") &&
                        listed["result"]["tools"].size() >= 20 &&
                        status["result"].value("isError", true) == false &&
                        gatedBuild["result"].value("isError", false) == true;
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

#include "GameHost/GameToolService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

nlohmann::json Request(GameToolService &service, const char *command,
                       nlohmann::json arguments) {
  return nlohmann::json::parse(service.HandleRequestForAutoTest(
      nlohmann::json({{"command", command}, {"arguments", arguments}}).dump()));
}
} // namespace

int main(int argc, char **argv) {
  std::filesystem::path sandbox;
  std::filesystem::path root;
  try {
    if (argc != 2)
      throw std::runtime_error("Expected a temporary test directory.");
    sandbox = std::filesystem::absolute(argv[1]);
    root = sandbox / "Repository" / "Games" / "TestGame";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(root / "Source");
    std::filesystem::create_directories(sandbox / "Repository" / "Engine");
    std::filesystem::create_directories(sandbox / "Repository" / "docs" /
                                        "skills");
    std::filesystem::create_directories(sandbox / "Repository" / "docs" /
                                        "skills" / "test-skill");
    std::filesystem::create_directories(root / "build");
    std::filesystem::create_directories(root / "runtime");
    {
      std::ofstream(root / "Source" / "Legacy.hpp") << "#pragma once\n";
      std::ofstream(sandbox / "Repository" / "docs" / "Architecture.md")
          << "# Architecture\n";
      std::ofstream(sandbox / "Repository" / "docs" / "skills" / "test-skill" /
                    "SKILL.md")
          << "---\nname: test-skill\ndescription: Test guidance.\n---\n";
    }

    GameToolService service(nullptr, root, root / "build", root / "runtime");
    auto response = Request(service, "list_agent_documents", {});
    Require(response.at("ok") && response["result"]["documents"].size() == 1,
            "Agent documents were not listed.");
    response =
        Request(service, "read_agent_document", {{"name", "Architecture.md"}});
    if (!response.at("ok"))
      throw std::runtime_error("Agent document was not read: " +
                               response.dump());
    Require(response["result"]["content"].get<std::string>().find(
                "# Architecture") != std::string::npos,
            "Agent document content was incorrect.");
    response =
        Request(service, "read_agent_document", {{"name", "../AGENTS.md"}});
    Require(!response.at("ok"), "Agent document traversal was accepted.");
    response = Request(service, "list_agent_skills", {});
    Require(response.at("ok") && response["result"]["skills"].size() == 1,
            "Agent skills were not listed.");
    response = Request(service, "read_agent_skill", {{"name", "test-skill"}});
    Require(response.at("ok"), "Agent skill was not read.");

    const std::string validCode =
        "#pragma once\n\nnamespace Test {\nstruct Widget {};\n}\n";
    response = Request(service, "create_game_code_file",
                       {{"path", "Source/Widget.h"}, {"content", validCode}});
    Require(response.at("ok") &&
                std::filesystem::is_regular_file(root / "Source" / "Widget.h"),
            "Valid Game header was not created.");

    response = Request(service, "create_game_code_file",
                       {{"path", "Source/Widget.h"}, {"content", validCode}});
    Require(!response.at("ok"), "Existing Game file was overwritten.");

    const std::string replacementCode =
        "#pragma once\n\nnamespace Test {\nstruct Replacement {};\n}\n";
    response =
        Request(service, "replace_game_code_file",
                {{"path", "Source/Widget.h"}, {"content", replacementCode}});
    Require(response.at("ok") &&
                std::ifstream(root / "Source" / "Widget.h").good(),
            "Existing Game header was not replaced.");
    {
      std::ifstream stream(root / "Source" / "Widget.h");
      const std::string content{std::istreambuf_iterator<char>(stream),
                                std::istreambuf_iterator<char>()};
      Require(content == replacementCode,
              "Whole-file replacement content was incorrect.");
    }

    response = Request(service, "replace_game_code_file",
                       {{"path", "Source/Widget.h"},
                        {"content", "```cpp\nint Value() { return 1; }\n```"}});
    Require(!response.at("ok"),
            "Invalid whole-file replacement content was accepted.");
    {
      std::ifstream stream(root / "Source" / "Widget.h");
      const std::string content{std::istreambuf_iterator<char>(stream),
                                std::istreambuf_iterator<char>()};
      Require(content == replacementCode,
              "Rejected replacement modified the existing Game file.");
    }

    response = Request(service, "replace_game_code_file",
                       {{"path", "Source/Legacy.hpp"}, {"content", validCode}});
    Require(!response.at("ok"),
            "Whole-file replacement accepted a forbidden extension.");

    response = Request(service, "create_game_code_file",
                       {{"path", "Source/Markdown.cpp"},
                        {"content", "```cpp\nint Value() { return 1; }\n```"}});
    Require(!response.at("ok") &&
                !std::filesystem::exists(root / "Source" / "Markdown.cpp"),
            "Markdown content passed the C++ source validation.");

    response = Request(service, "create_game_code_file",
                       {{"path", "Source/NotCode.txt"},
                        {"content", "int Value() { return 1; }"}});
    Require(!response.at("ok"), "A non-C++ extension was created.");
    response = Request(
        service, "create_game_code_file",
        {{"path", "../Escape.cpp"}, {"content", "int Value() { return 1; }"}});
    Require(!response.at("ok"), "Game path traversal was accepted.");

    response = Request(service, "delete_game_code_file",
                       {{"path", "Source/Widget.h"}});
    Require(response.at("ok") &&
                !std::filesystem::exists(root / "Source" / "Widget.h"),
            "Game header was not deleted.");
    response = Request(service, "delete_game_code_file",
                       {{"path", "Source/Legacy.hpp"}});
    Require(!response.at("ok") &&
                std::filesystem::exists(root / "Source" / "Legacy.hpp"),
            "Deletion accepted a forbidden extension.");

    std::filesystem::remove_all(sandbox);
    std::cout << "Game code file tool tests passed.\n";
    return 0;
  } catch (const std::exception &exception) {
    if (!sandbox.empty())
      std::filesystem::remove_all(sandbox);
    std::cerr << "Game code file tool test failure: " << exception.what()
              << '\n';
    return 1;
  }
}

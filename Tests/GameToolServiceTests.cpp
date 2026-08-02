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
    std::filesystem::create_directories(root / "build");
    std::filesystem::create_directories(root / "runtime");
    {
      std::ofstream(root / "Source" / "Legacy.hpp") << "#pragma once\n";
    }

    GameToolService service(nullptr, root, root / "build", root / "runtime");
    const std::string validCode =
        "#pragma once\n\nnamespace Test {\nstruct Widget {};\n}\n";
    auto response =
        Request(service, "create_game_code_file",
                {{"path", "Source/Widget.h"}, {"content", validCode}});
    Require(response.at("ok") &&
                std::filesystem::is_regular_file(root / "Source" / "Widget.h"),
            "Valid Game header was not created.");

    response = Request(service, "create_game_code_file",
                       {{"path", "Source/Widget.h"}, {"content", validCode}});
    Require(!response.at("ok"), "Existing Game file was overwritten.");

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

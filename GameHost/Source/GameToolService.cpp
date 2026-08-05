#include "GameHost/GameToolService.h"

#include "GameHost/ReloadableGame.h"

#include "Development/GameToolsProtocol.h"
#include "Engine/Core/Logger.h"
#include "Engine/Serialization/Serializer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstring>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {
constexpr std::size_t MaximumFileSize = 512 * 1024;
constexpr std::size_t MaximumSearchResults = 100;
constexpr std::size_t MaximumBatchFiles = 16;
constexpr std::size_t MaximumBatchPatches = 32;
constexpr std::size_t MaximumBatchReadSize = 3 * 1024 * 1024;
constexpr std::size_t MaximumGitOutput = 1024 * 1024;

struct CommandResult {
  int exitCode = -1;
  std::string output;
};

CommandResult RunCommand(const std::string &command,
                         std::size_t outputLimit = MaximumGitOutput) {
  std::array<char, 4096> buffer{};
  CommandResult result;
  FILE *pipe = _popen(command.c_str(), "r");
  if (!pipe)
    throw std::runtime_error("Unable to start command.");
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    const std::size_t available = outputLimit - result.output.size();
    if (available > 0) {
      const std::size_t byteCount = std::strlen(buffer.data()) < available
                                        ? std::strlen(buffer.data())
                                        : available;
      result.output.append(buffer.data(), byteCount);
    }
  }
  result.exitCode = _pclose(pipe);
  return result;
}

std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n' ||
                            value.back() == ' ' || value.back() == '\t'))
    value.pop_back();
  return value;
}

std::string ToWebRepositoryUrl(std::string remote) {
  remote = Trim(std::move(remote));
  if (remote.ends_with(".git"))
    remote.resize(remote.size() - 4);
  if (remote.starts_with("git@")) {
    const auto colon = remote.find(':');
    if (colon != std::string::npos)
      remote = "https://" + remote.substr(4, colon - 4) + "/" +
               remote.substr(colon + 1);
  } else if (remote.starts_with("ssh://git@")) {
    remote = "https://" + remote.substr(10);
  }
  const auto scheme = remote.find("://");
  if (scheme != std::string::npos) {
    const auto authorityEnd = remote.find('/', scheme + 3);
    const auto userInfo = remote.find('@', scheme + 3);
    if (userInfo != std::string::npos &&
        (authorityEnd == std::string::npos || userInfo < authorityEnd))
      remote.erase(scheme + 3, userInfo - (scheme + 3) + 1);
  }
  return remote.starts_with("https://") || remote.starts_with("http://")
             ? remote
             : std::string{};
}

#if defined(ENGINE_AUTOTESTS)
class JsonStateSerializer final : public Engine::Serializer {
public:
  void Value(std::string_view name, bool &value) override {
    values_[std::string(name)] = value;
  }

  void Value(std::string_view name, int &value) override {
    values_[std::string(name)] = value;
  }

  void Value(std::string_view name, float &value) override {
    values_[std::string(name)] = value;
  }

  void Value(std::string_view name, std::string &value) override {
    values_[std::string(name)] = value;
  }

  [[nodiscard]] nlohmann::json TakeValues() { return std::move(values_); }

private:
  nlohmann::json values_ = nlohmann::json::object();
};
#endif

bool IsAllowedSourceExtension(const std::filesystem::path &path) {
  static const std::unordered_set<std::string> extensions{".cpp", ".h", ".hpp",
                                                          ".inl"};
  return extensions.contains(path.extension().string());
}

bool IsCreatableSourceExtension(const std::filesystem::path &path) {
  return path.extension() == ".cpp" || path.extension() == ".h";
}

bool IsValidUtf8(std::string_view content) {
  for (std::size_t index = 0; index < content.size();) {
    const auto first = static_cast<unsigned char>(content[index]);
    if (first < 0x80U) {
      ++index;
      continue;
    }
    std::size_t continuationCount = 0;
    std::uint32_t codePoint = 0;
    if ((first & 0xe0U) == 0xc0U) {
      continuationCount = 1;
      codePoint = first & 0x1fU;
    } else if ((first & 0xf0U) == 0xe0U) {
      continuationCount = 2;
      codePoint = first & 0x0fU;
    } else if ((first & 0xf8U) == 0xf0U) {
      continuationCount = 3;
      codePoint = first & 0x07U;
    } else {
      return false;
    }
    if (index + continuationCount >= content.size())
      return false;
    for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
      const auto byte = static_cast<unsigned char>(content[index + offset]);
      if ((byte & 0xc0U) != 0x80U)
        return false;
      codePoint = (codePoint << 6U) | (byte & 0x3fU);
    }
    const std::uint32_t minimum = continuationCount == 1   ? 0x80U
                                  : continuationCount == 2 ? 0x800U
                                                           : 0x10000U;
    if (codePoint < minimum || codePoint > 0x10ffffU ||
        (codePoint >= 0xd800U && codePoint <= 0xdfffU))
      return false;
    index += continuationCount + 1;
  }
  return true;
}

void ValidateCppContent(std::string_view content) {
  if (content.empty())
    throw std::runtime_error("C++ file content cannot be empty.");
  if (content.size() > MaximumFileSize)
    throw std::runtime_error("C++ file exceeds the 512 KiB limit.");
  if (!IsValidUtf8(content))
    throw std::runtime_error("C++ file must contain valid UTF-8 text.");
  for (const unsigned char character : content) {
    if ((character < 0x20U && character != '\t' && character != '\r' &&
         character != '\n') ||
        character == 0x7fU) {
      throw std::runtime_error(
          "C++ file contains binary or unsupported control bytes.");
    }
  }

  static constexpr std::array ForbiddenFragments{
      "```",     "<!DOCTYPE",  "<!doctype",        "<html", "<HTML", "<script",
      "<SCRIPT", "data:text/", "data:application/"};
  for (const std::string_view fragment : ForbiddenFragments) {
    if (content.find(fragment) != std::string_view::npos)
      throw std::runtime_error(
          "C++ file contains Markdown, HTML, or embedded document data.");
  }

  static constexpr std::array CppMarkers{
      "#include",   "#pragma",    "#define",    "namespace ",    "class ",
      "struct ",    "enum ",      "using ",     "typedef ",      "template<",
      "template <", "constexpr ", "consteval ", "static_assert("};
  const bool hasDeclarationMarker =
      std::ranges::any_of(CppMarkers, [content](std::string_view marker) {
        return content.find(marker) != std::string_view::npos;
      });
  const bool hasFunctionShape = content.find('(') != std::string_view::npos &&
                                content.find(')') != std::string_view::npos &&
                                content.find('{') != std::string_view::npos &&
                                content.find('}') != std::string_view::npos;
  if (!hasDeclarationMarker && !hasFunctionShape)
    throw std::runtime_error(
        "File was rejected because it does not look like C++ code.");
}

std::string ReadTextFile(const std::filesystem::path &path) {
  const std::uintmax_t size = std::filesystem::file_size(path);
  if (size > MaximumFileSize) {
    throw std::runtime_error("Game file exceeds the 512 KiB limit.");
  }

  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to open Game file.");
  }

  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

bool IsPathInside(const std::filesystem::path &child,
                  const std::filesystem::path &parent) {
  auto childPart = child.begin();
  auto parentPart = parent.begin();
  for (; parentPart != parent.end(); ++parentPart, ++childPart) {
    if (childPart == child.end() || *childPart != *parentPart) {
      return false;
    }
  }
  return true;
}

void ApplyExactReplacement(std::string &content, std::string_view oldText,
                           std::string_view newText) {
  if (oldText.empty()) {
    throw std::runtime_error("oldText cannot be empty.");
  }

  const std::size_t position = content.find(oldText);
  if (position == std::string::npos) {
    throw std::runtime_error("oldText was not found.");
  }
  if (content.find(oldText, position + oldText.size()) != std::string::npos) {
    throw std::runtime_error(
        "oldText is ambiguous; provide a larger unique block.");
  }

  content.replace(position, oldText.size(), newText);
  if (content.size() > MaximumFileSize) {
    throw std::runtime_error("Patched file would exceed the 512 KiB limit.");
  }
}

void WriteTextFileAtomically(const std::filesystem::path &file,
                             std::string_view content) {
  const std::filesystem::path temporary = file.string() + ".assistant.tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
      throw std::runtime_error("Unable to create temporary Game file.");
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), file.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary);
    throw std::runtime_error("Atomic replacement of the Game file failed.");
  }
#else
  std::filesystem::rename(temporary, file);
#endif
}

void CreateTextFileAtomically(const std::filesystem::path &file,
                              std::string_view content) {
  const std::filesystem::path temporary = file.string() + ".assistant.tmp";
  if (std::filesystem::exists(temporary))
    throw std::runtime_error("Temporary assistant file already exists.");
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
      throw std::runtime_error("Unable to create temporary Game file.");
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!stream) {
      stream.close();
      std::filesystem::remove(temporary);
      throw std::runtime_error("Writing temporary Game file failed.");
    }
  }

#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary);
    throw std::runtime_error("Atomic creation of the Game file failed.");
  }
#else
  std::filesystem::rename(temporary, file);
#endif
}
} // namespace

GameToolService::GameToolService(ReloadableGame *game,
                                 std::filesystem::path gameRoot,
                                 std::filesystem::path buildDirectory,
                                 std::filesystem::path runtimeDirectory,
                                 bool recoveryMode)
    : game_(game), gameRoot_(std::filesystem::weakly_canonical(gameRoot)),
      engineRoot_(std::filesystem::weakly_canonical(
          gameRoot_.parent_path().parent_path() / "Engine")),
      skillsRoot_(std::filesystem::weakly_canonical(
          gameRoot_.parent_path().parent_path() / "docs" / "skills")),
      documentsRoot_(std::filesystem::weakly_canonical(
          gameRoot_.parent_path().parent_path() / "docs")),
      repositoryRoot_(std::filesystem::weakly_canonical(
          gameRoot_.parent_path().parent_path())),
      buildDirectory_(std::filesystem::weakly_canonical(buildDirectory)),
      runtimeDirectory_(std::filesystem::weakly_canonical(runtimeDirectory)),
      recoveryMode_(recoveryMode), server_(Development::GetGameToolsPipeName(),
                                           [this](std::string_view request) {
                                             return HandleRequest(request);
                                           }) {}

bool GameToolService::IsLaunchRequested() const { return launchRequested_; }

std::string GameToolService::HandleRequest(std::string_view request) {
  try {
    const nlohmann::json input = nlohmann::json::parse(request);
    const std::string command = input.value("command", "");
    const nlohmann::json arguments =
        input.value("arguments", nlohmann::json::object());

    nlohmann::json result;
    Engine::Logger::Info("IPC tool request: " + command);

    if (command == "ping") {
      result = {{"service", "GameHost tools"},
                {"version", 2},
                {"recoveryMode", recoveryMode_},
                {"reloadStatus", game_
                                     ? game_->GetReloadStatus()
                                     : "Game is stopped for crash recovery."}};
    } else if (command == "list_agent_skills") {
      nlohmann::json skills = nlohmann::json::array();
      std::vector<std::filesystem::path> skillFiles;
      for (const auto &entry :
           std::filesystem::directory_iterator(skillsRoot_)) {
        const auto skillFile = entry.path() / "SKILL.md";
        if (!entry.is_directory() ||
            !std::filesystem::is_regular_file(skillFile))
          continue;
        skillFiles.push_back(skillFile);
      }
      std::ranges::sort(skillFiles);
      for (const auto &skillFile : skillFiles) {
        const std::string content = ReadTextFile(skillFile);
        const auto readField = [&content](std::string_view field) {
          const std::string prefix = std::string(field) + ":";
          const std::size_t start = content.find(prefix);
          if (start == std::string::npos)
            return std::string{};
          const std::size_t valueStart =
              content.find_first_not_of(" \t", start + prefix.size());
          const std::size_t end = content.find_first_of("\r\n", valueStart);
          return content.substr(valueStart, end - valueStart);
        };
        skills.push_back({{"name", readField("name")},
                          {"description", readField("description")}});
      }
      result = {{"skills", std::move(skills)}, {"readOnly", true}};
    } else if (command == "read_agent_skill") {
      const std::filesystem::path file =
          ResolveAgentSkill(arguments.at("name").get<std::string>());
      result = {{"name", file.parent_path().filename().string()},
                {"content", ReadTextFile(file)},
                {"readOnly", true}};
    } else if (command == "confirm_no_applicable_skills") {
      const std::string reason = arguments.at("reason").get<std::string>();
      if (reason.size() < 12)
        throw std::runtime_error("A concrete skill-review reason is required.");
      result = {{"reviewed", true}, {"reason", reason}, {"readOnly", true}};
    } else if (command == "list_agent_documents") {
      std::vector<std::string> documentNames;
      for (const auto &entry :
           std::filesystem::directory_iterator(documentsRoot_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md")
          documentNames.push_back(entry.path().filename().string());
      }
      std::ranges::sort(documentNames);
      result = {{"documents", std::move(documentNames)}, {"readOnly", true}};
    } else if (command == "read_agent_document") {
      const std::filesystem::path file =
          ResolveAgentDocument(arguments.at("name").get<std::string>());
      result = {{"name", file.filename().string()},
                {"content", ReadTextFile(file)},
                {"readOnly", true}};
    } else if (command == "read_game_file") {
      const std::filesystem::path file =
          ResolveGameFile(arguments.at("path").get<std::string>());
      result = {
          {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
          {"content", ReadTextFile(file)}};
    } else if (command == "read_game_files") {
      const auto &paths = arguments.at("paths");
      if (!paths.is_array() || paths.empty() ||
          paths.size() > MaximumBatchFiles) {
        throw std::runtime_error("paths must contain between 1 and 16 files.");
      }

      nlohmann::json files = nlohmann::json::array();
      std::size_t totalSize = 0;
      for (const auto &path : paths) {
        const std::filesystem::path file =
            ResolveGameFile(path.get<std::string>());
        std::string content = ReadTextFile(file);
        totalSize += content.size();
        if (totalSize > MaximumBatchReadSize) {
          throw std::runtime_error(
              "Combined batch read exceeds the 3 MiB limit.");
        }
        files.push_back(
            {{"path",
              std::filesystem::relative(file, gameRoot_).generic_string()},
             {"content", std::move(content)}});
      }
      result = {{"files", std::move(files)}};
    } else if (command == "read_engine_file") {
      const std::filesystem::path file =
          ResolveEngineFile(arguments.at("path").get<std::string>());
      result = {{"path",
                 std::filesystem::relative(file, engineRoot_).generic_string()},
                {"content", ReadTextFile(file)},
                {"readOnly", true}};
    } else if (command == "read_engine_files") {
      const auto &paths = arguments.at("paths");
      if (!paths.is_array() || paths.empty() ||
          paths.size() > MaximumBatchFiles) {
        throw std::runtime_error("paths must contain between 1 and 16 files.");
      }

      nlohmann::json files = nlohmann::json::array();
      std::size_t totalSize = 0;
      for (const auto &path : paths) {
        const std::filesystem::path file =
            ResolveEngineFile(path.get<std::string>());
        std::string content = ReadTextFile(file);
        totalSize += content.size();
        if (totalSize > MaximumBatchReadSize) {
          throw std::runtime_error(
              "Combined batch read exceeds the 3 MiB limit.");
        }
        files.push_back(
            {{"path",
              std::filesystem::relative(file, engineRoot_).generic_string()},
             {"content", std::move(content)}});
      }
      result = {{"files", std::move(files)}, {"readOnly", true}};
    } else if (command == "list_game_files") {
      nlohmann::json files = nlohmann::json::array();
      auto iterator = std::filesystem::recursive_directory_iterator(gameRoot_);
      const auto end = std::filesystem::recursive_directory_iterator();
      for (; iterator != end; ++iterator) {
        const auto &entry = *iterator;
        if (entry.is_directory() && iterator.depth() == 0 &&
            entry.path().filename() == "build") {
          iterator.disable_recursion_pending();
          continue;
        }
        if (entry.is_regular_file() && IsAllowedSourceExtension(entry.path())) {
          files.push_back(std::filesystem::relative(entry.path(), gameRoot_)
                              .generic_string());
        }
      }
      result = {{"files", std::move(files)}};
    } else if (command == "list_engine_files") {
      nlohmann::json files = nlohmann::json::array();
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(engineRoot_)) {
        if (entry.is_regular_file() && IsAllowedSourceExtension(entry.path())) {
          files.push_back(std::filesystem::relative(entry.path(), engineRoot_)
                              .generic_string());
        }
      }
      result = {{"files", std::move(files)}, {"readOnly", true}};
    } else if (command == "search_game_code") {
      const std::string query = arguments.at("query").get<std::string>();
      if (query.empty() || query.size() > 256) {
        throw std::runtime_error(
            "Search query must contain 1 to 256 characters.");
      }

      nlohmann::json matches = nlohmann::json::array();
      auto iterator = std::filesystem::recursive_directory_iterator(gameRoot_);
      const auto end = std::filesystem::recursive_directory_iterator();
      for (; iterator != end; ++iterator) {
        const auto &entry = *iterator;
        if (entry.is_directory() && iterator.depth() == 0 &&
            entry.path().filename() == "build") {
          iterator.disable_recursion_pending();
          continue;
        }
        if (!entry.is_regular_file() ||
            !IsAllowedSourceExtension(entry.path())) {
          continue;
        }

        std::ifstream stream(entry.path());
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(stream, line)) {
          ++lineNumber;
          if (line.find(query) != std::string::npos) {
            matches.push_back(
                {{"path", std::filesystem::relative(entry.path(), gameRoot_)
                              .generic_string()},
                 {"line", lineNumber},
                 {"text", line}});
            if (matches.size() >= MaximumSearchResults) {
              break;
            }
          }
        }
        if (matches.size() >= MaximumSearchResults) {
          break;
        }
      }
      result = {{"matches", std::move(matches)}};
    } else if (command == "search_engine_code") {
      const std::string query = arguments.at("query").get<std::string>();
      if (query.empty() || query.size() > 256) {
        throw std::runtime_error(
            "Search query must contain 1 to 256 characters.");
      }

      nlohmann::json matches = nlohmann::json::array();
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(engineRoot_)) {
        if (!entry.is_regular_file() ||
            !IsAllowedSourceExtension(entry.path())) {
          continue;
        }

        std::ifstream stream(entry.path());
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(stream, line)) {
          ++lineNumber;
          if (line.find(query) != std::string::npos) {
            matches.push_back(
                {{"path", std::filesystem::relative(entry.path(), engineRoot_)
                              .generic_string()},
                 {"line", lineNumber},
                 {"text", line}});
            if (matches.size() >= MaximumSearchResults) {
              break;
            }
          }
        }
        if (matches.size() >= MaximumSearchResults) {
          break;
        }
      }
      result = {{"matches", std::move(matches)}, {"readOnly", true}};
    } else if (command == "apply_game_patch") {
      const std::filesystem::path file =
          ResolveGameFile(arguments.at("path").get<std::string>());
      const std::string oldText = arguments.at("oldText").get<std::string>();
      const std::string newText = arguments.at("newText").get<std::string>();

      std::string content = ReadTextFile(file);
      ApplyExactReplacement(content, oldText, newText);
      WriteTextFileAtomically(file, content);
      result = {
          {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
          {"changed", true}};
    } else if (command == "apply_game_patches") {
      const auto &patches = arguments.at("patches");
      if (!patches.is_array() || patches.empty() ||
          patches.size() > MaximumBatchPatches) {
        throw std::runtime_error(
            "patches must contain between 1 and 32 replacements.");
      }

      std::map<std::filesystem::path, std::string> changedFiles;
      for (std::size_t index = 0; index < patches.size(); ++index) {
        const auto &patch = patches.at(index);
        const std::filesystem::path file =
            ResolveGameFile(patch.at("path").get<std::string>());
        auto [entry, inserted] = changedFiles.try_emplace(file);
        if (inserted) {
          entry->second = ReadTextFile(file);
        }

        try {
          ApplyExactReplacement(entry->second,
                                patch.at("oldText").get<std::string>(),
                                patch.at("newText").get<std::string>());
        } catch (const std::exception &exception) {
          throw std::runtime_error(
              "Patch " + std::to_string(index + 1) + " for " +
              std::filesystem::relative(file, gameRoot_).generic_string() +
              " failed: " + exception.what());
        }
      }

      nlohmann::json files = nlohmann::json::array();
      for (const auto &[file, content] : changedFiles) {
        WriteTextFileAtomically(file, content);
        files.push_back(
            std::filesystem::relative(file, gameRoot_).generic_string());
      }
      result = {{"changed", true},
                {"patchCount", patches.size()},
                {"files", std::move(files)}};
    } else if (command == "replace_game_code_file") {
      const std::filesystem::path file =
          ResolveGameFile(arguments.at("path").get<std::string>());
      if (!IsCreatableSourceExtension(file))
        throw std::runtime_error(
            "Only existing .cpp and .h Game files may be replaced.");
      const std::string content = arguments.at("content").get<std::string>();
      ValidateCppContent(content);
      WriteTextFileAtomically(file, content);
      result = {
          {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
          {"replaced", true},
          {"validatedAs", "C++ source text"}};
    } else if (command == "create_game_code_file") {
      const std::filesystem::path file =
          ResolveNewGameFile(arguments.at("path").get<std::string>());
      const std::string content = arguments.at("content").get<std::string>();
      ValidateCppContent(content);
      CreateTextFileAtomically(file, content);
      result = {
          {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
          {"created", true},
          {"validatedAs", "C++ source text"}};
    } else if (command == "delete_game_code_file") {
      const std::filesystem::path file =
          ResolveGameFile(arguments.at("path").get<std::string>());
      if (!IsCreatableSourceExtension(file))
        throw std::runtime_error(
            "Only .cpp and .h Game files may be deleted by this tool.");
      const std::string relative =
          std::filesystem::relative(file, gameRoot_).generic_string();
      if (!std::filesystem::remove(file))
        throw std::runtime_error("Deleting the Game code file failed.");
      result = {{"path", relative}, {"deleted", true}};
    } else if (command == "build_game") {
      result = nlohmann::json::parse(BuildGame());
    } else if (command == "read_build_output") {
      std::scoped_lock lock(buildMutex_);
      result = {{"output", lastBuildOutput_}};
    } else if (command == "get_git_status") {
      result = nlohmann::json::parse(GetGitStatus());
    } else if (command == "read_git_changes") {
      result = nlohmann::json::parse(ReadGitChanges());
    } else if (command == "commit_and_push_changes") {
      result = nlohmann::json::parse(CommitAndPushChanges(
          arguments.at("title").get<std::string>(),
          arguments.at("description").get<std::string>()));
    } else if (command == "reload_game") {
      if (!game_) {
        throw std::runtime_error(
            "Game is not running. Build the repair, then use launch_game.");
      }
      game_->RequestReload();
      result = {{"status", "Reload requested on the game thread."}};
    } else if (command == "get_reload_status") {
      result = {{"status", game_ ? game_->GetReloadStatus()
                                 : "Game is stopped for crash recovery."}};
    }
#if defined(ENGINE_AUTOTESTS)
    else if (command == "get_game_state") {
      if (!game_) {
        throw std::runtime_error("Game is not running.");
      }
      JsonStateSerializer serializer;
      game_->SerializeAutoTestState(serializer);
      result = {{"entities", serializer.TakeValues()}};
    }
#endif
    else if (command == "inspect_crash_diagnostics") {
      result = nlohmann::json::parse(ReadCrashDiagnostics());
    } else if (command == "launch_game") {
      if (!recoveryMode_) {
        throw std::runtime_error(
            "launch_game is only available in crash recovery mode.");
      }
      launchRequested_ = true;
      result = {{"status", "Repair accepted. Launcher will start the Game."}};
    } else {
      throw std::runtime_error("Unknown or disallowed Game tool command.");
    }

    return nlohmann::json({{"ok", true}, {"result", std::move(result)}}).dump();
  } catch (const std::exception &exception) {
    Engine::Logger::Warning(std::string("IPC tool request failed: ") +
                            exception.what());
    return nlohmann::json({{"ok", false}, {"error", exception.what()}}).dump();
  }
}

std::string GameToolService::ReadCrashDiagnostics() const {
  constexpr std::size_t MaximumDiagnosticText = 256 * 1024;
  nlohmann::json reports = nlohmann::json::array();

  const auto appendRecentFiles = [&reports, MaximumDiagnosticText](
                                     const std::filesystem::path &directory,
                                     std::string_view kind,
                                     std::string_view extension) {
    if (!std::filesystem::exists(directory)) {
      return;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
      if (entry.is_regular_file() && entry.path().extension() == extension) {
        entries.push_back(entry);
      }
    }
    std::ranges::sort(entries, std::greater{}, [](const auto &entry) {
      return entry.last_write_time();
    });
    if (entries.size() > 8) {
      entries.resize(8);
    }

    for (const auto &entry : entries) {
      std::ifstream stream(entry.path(), std::ios::binary);
      if (!stream) {
        continue;
      }
      const std::uintmax_t fileSize = std::filesystem::file_size(entry.path());
      const std::size_t readSize = static_cast<std::size_t>(
          std::min<std::uintmax_t>(fileSize, MaximumDiagnosticText));
      if (fileSize > readSize) {
        stream.seekg(static_cast<std::streamoff>(fileSize - readSize));
      }
      std::string content(readSize, '\0');
      stream.read(content.data(), static_cast<std::streamsize>(readSize));
      content.resize(static_cast<std::size_t>(stream.gcount()));
      if (fileSize > readSize) {
        content.insert(0, "[older content truncated]\n");
      }
      reports.push_back({{"kind", kind},
                         {"file", entry.path().filename().string()},
                         {"content", std::move(content)}});
    }
  };

  appendRecentFiles(runtimeDirectory_ / "Crashes", "crash_report", ".txt");
  appendRecentFiles(runtimeDirectory_ / "Logs", "process_log", ".log");
  return nlohmann::json(
             {{"recoveryMode", recoveryMode_},
              {"runtimeDirectory", runtimeDirectory_.generic_string()},
              {"reports", std::move(reports)}})
      .dump();
}

std::filesystem::path
GameToolService::ResolveGameFile(std::string_view relativePath) const {
  const std::filesystem::path relative =
      std::filesystem::path(relativePath).lexically_normal();
  if (relative.empty() || relative.is_absolute()) {
    throw std::runtime_error("Game path must be relative.");
  }
  for (const auto &component : relative) {
    if (component == ".." || component == "build") {
      throw std::runtime_error(
          "Game path traversal and build access are not allowed.");
    }
  }

  const std::filesystem::path resolved =
      std::filesystem::weakly_canonical(gameRoot_ / relative);
  if (!IsPathInside(resolved, gameRoot_) ||
      !IsAllowedSourceExtension(resolved)) {
    throw std::runtime_error(
        "Only C++ source files inside the active Game workspace are allowed.");
  }
  if (!std::filesystem::is_regular_file(resolved)) {
    throw std::runtime_error("Game file does not exist.");
  }
  return resolved;
}

std::filesystem::path
GameToolService::ResolveNewGameFile(std::string_view relativePath) const {
  const std::filesystem::path relative =
      std::filesystem::path(relativePath).lexically_normal();
  if (relative.empty() || relative.is_absolute() || !relative.has_filename())
    throw std::runtime_error("Game path must be a relative file path.");
  for (const auto &component : relative) {
    if (component == ".." || component == "build")
      throw std::runtime_error(
          "Game path traversal and build access are not allowed.");
  }
  if (!IsCreatableSourceExtension(relative))
    throw std::runtime_error("Only new .cpp and .h files may be created.");

  const std::filesystem::path parent =
      std::filesystem::weakly_canonical(gameRoot_ / relative.parent_path());
  if (!IsPathInside(parent, gameRoot_) ||
      !std::filesystem::is_directory(parent)) {
    throw std::runtime_error(
        "New Game files require an existing directory inside the active Game.");
  }
  const std::filesystem::path resolved = parent / relative.filename();
  if (!IsPathInside(resolved, gameRoot_))
    throw std::runtime_error("New Game file resolved outside the active Game.");
  if (std::filesystem::exists(resolved))
    throw std::runtime_error(
        "Game file already exists; use apply_game_patch instead.");
  return resolved;
}

std::filesystem::path
GameToolService::ResolveEngineFile(std::string_view relativePath) const {
  const std::filesystem::path relative =
      std::filesystem::path(relativePath).lexically_normal();
  if (relative.empty() || relative.is_absolute()) {
    throw std::runtime_error("Engine path must be relative.");
  }
  for (const auto &component : relative) {
    if (component == ".." || component == "build") {
      throw std::runtime_error(
          "Engine path traversal and build access are not allowed.");
    }
  }

  const std::filesystem::path resolved =
      std::filesystem::weakly_canonical(engineRoot_ / relative);
  if (!IsPathInside(resolved, engineRoot_) ||
      !IsAllowedSourceExtension(resolved)) {
    throw std::runtime_error("Only Engine C++ source files may be read.");
  }
  if (!std::filesystem::is_regular_file(resolved)) {
    throw std::runtime_error("Engine file does not exist.");
  }
  return resolved;
}

std::filesystem::path
GameToolService::ResolveAgentSkill(std::string_view name) const {
  if (name.empty() || name.size() > 63 ||
      !std::ranges::all_of(name, [](char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '-';
      })) {
    throw std::runtime_error("Invalid skill name.");
  }
  const std::filesystem::path resolved = std::filesystem::weakly_canonical(
      skillsRoot_ / std::string(name) / "SKILL.md");
  if (!IsPathInside(resolved, skillsRoot_) ||
      !std::filesystem::is_regular_file(resolved)) {
    throw std::runtime_error("Agent skill does not exist.");
  }
  return resolved;
}

std::filesystem::path
GameToolService::ResolveAgentDocument(std::string_view name) const {
  const std::filesystem::path relative(name);
  if (relative.empty() || relative.is_absolute() ||
      relative.filename() != relative || relative.extension() != ".md")
    throw std::runtime_error("Invalid agent document name.");
  const std::filesystem::path resolved =
      std::filesystem::weakly_canonical(documentsRoot_ / relative);
  if (!IsPathInside(resolved, documentsRoot_) ||
      !std::filesystem::is_regular_file(resolved))
    throw std::runtime_error("Agent document does not exist.");
  return resolved;
}

std::string GameToolService::BuildGame() {
  std::scoped_lock lock(buildMutex_);
  Engine::Logger::Info("Controlled Game build started.");

  const std::string quotedBuildDirectory =
      "\"" + buildDirectory_.string() + "\"";
  const std::string command =
      "cmake --build " + quotedBuildDirectory +
      " --config Debug --target ZERO_CHECK --parallel 2>&1 && cmake --build " +
      quotedBuildDirectory + " --config Debug --target Game --parallel 2>&1";

  std::array<char, 4096> buffer{};
  std::string output;
  FILE *pipe = _popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("Unable to start the controlled Game build.");
  }

  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    output += buffer.data();
    if (output.size() > 2 * 1024 * 1024) {
      output += "\n[Build output truncated]\n";
      break;
    }
  }

  const int exitCode = _pclose(pipe);
  lastBuildOutput_ = output;
  Engine::Logger::Info("Controlled Game build completed with exit code " +
                       std::to_string(exitCode) + ".");
  return nlohmann::json({{"success", exitCode == 0},
                         {"exitCode", exitCode},
                         {"output", output}})
      .dump();
}

std::string GameToolService::GetGitStatus() const {
  const std::string git = "git -C \"" + repositoryRoot_.string() + "\" ";
  const CommandResult workTree =
      RunCommand(git + "rev-parse --is-inside-work-tree 2>&1");
  if (workTree.exitCode != 0 || Trim(workTree.output) != "true")
    return nlohmann::json({{"canCommitAndPush", false},
                           {"message", "Not a Git worktree."}})
        .dump();

  const CommandResult upstream =
      RunCommand(git + "rev-parse --abbrev-ref --symbolic-full-name "
                       "\"@{upstream}\" 2>&1");
  if (upstream.exitCode != 0)
    return nlohmann::json({{"canCommitAndPush", false},
                           {"message", "The current branch has no upstream."}})
        .dump();
  const std::string upstreamName = Trim(upstream.output);
  const auto slash = upstreamName.find('/');
  if (slash == std::string::npos)
    return nlohmann::json({{"canCommitAndPush", false},
                           {"message", "The upstream remote is invalid."}})
        .dump();
  const std::string remoteName = upstreamName.substr(0, slash);
  if (!std::ranges::all_of(remoteName, [](unsigned char character) {
        return std::isalnum(character) || character == '-' ||
               character == '_' || character == '.';
      }))
    return nlohmann::json({{"canCommitAndPush", false},
                           {"message", "The upstream remote name is invalid."}})
        .dump();
  const CommandResult remote =
      RunCommand(git + "remote get-url --push " + remoteName + " 2>&1");
  if (remote.exitCode != 0)
    return nlohmann::json({{"canCommitAndPush", false},
                           {"message", "No push URL is configured."}})
        .dump();

  const CommandResult changes =
      RunCommand(git + "status --porcelain=v1 --untracked-files=all 2>&1");
  const bool hasChanges = changes.exitCode == 0 && !Trim(changes.output).empty();
  const CommandResult dryRun = RunCommand(
      "set GIT_TERMINAL_PROMPT=0&& " + git + "push --dry-run 2>&1");
  const bool canPush = dryRun.exitCode == 0;
  return nlohmann::json(
             {{"canCommitAndPush", canPush && hasChanges},
              {"pushConfigured", canPush},
              {"hasChanges", hasChanges},
              {"upstream", upstreamName},
              {"repositoryWebUrl", ToWebRepositoryUrl(remote.output)},
              {"message", canPush ? (hasChanges ? "Ready to commit and push."
                                                  : "No uncommitted changes.")
                                   : "A dry-run push failed: " +
                                         Trim(dryRun.output)}})
      .dump();
}

std::string GameToolService::ReadGitChanges() const {
  const std::string git = "git -C \"" + repositoryRoot_.string() + "\" ";
  const CommandResult status =
      RunCommand(git + "status --short --untracked-files=all 2>&1");
  if (status.exitCode != 0)
    throw std::runtime_error("Unable to inspect Git changes: " + status.output);
  const CommandResult diff = RunCommand(
      git + "diff --no-ext-diff --stat HEAD && " + git +
      "diff --no-ext-diff --unified=3 HEAD 2>&1", MaximumGitOutput);
  if (diff.exitCode != 0)
    throw std::runtime_error("Unable to read Git diff: " + diff.output);
  const auto gitStatus = nlohmann::json::parse(GetGitStatus());
  return nlohmann::json({{"status", status.output},
                         {"diff", diff.output},
                         {"repositoryWebUrl",
                          gitStatus.value("repositoryWebUrl", "")},
                         {"truncated", diff.output.size() >= MaximumGitOutput}})
      .dump();
}

std::string GameToolService::CommitAndPushChanges(
    std::string_view title, std::string_view description) {
  std::scoped_lock lock(gitMutex_);
  if (title.empty() || title.size() > 72 || description.empty() ||
      description.size() > 4000 || title.find_first_of("\r\n") !=
                                       std::string_view::npos)
    throw std::runtime_error(
        "Commit title must be 1-72 characters and description 1-4000 characters.");
  for (const unsigned char character : std::string(title) +
                                           std::string(description)) {
    if (character < 0x20U && character != '\r' && character != '\n' &&
        character != '\t')
      throw std::runtime_error("Commit description contains control bytes.");
  }
  const auto status = nlohmann::json::parse(GetGitStatus());
  if (!status.value("canCommitAndPush", false))
    throw std::runtime_error(status.value("message", "Git push is unavailable."));

  const std::filesystem::path messagePath =
      std::filesystem::temp_directory_path() /
      ("aitester_git_message_" + std::to_string(GetCurrentProcessId()) +
       ".txt");
  {
    std::ofstream message(messagePath, std::ios::binary | std::ios::trunc);
    message << title << "\n\n" << description << "\n";
    if (!message)
      throw std::runtime_error("Unable to create the Git commit message.");
  }
  const std::string git = "git -C \"" + repositoryRoot_.string() + "\" ";
  const CommandResult add = RunCommand(git + "add --all 2>&1");
  if (add.exitCode != 0) {
    std::filesystem::remove(messagePath);
    throw std::runtime_error("Git staging failed: " + add.output);
  }
  const CommandResult commit = RunCommand(
      git + "commit --file \"" + messagePath.string() + "\" 2>&1");
  std::filesystem::remove(messagePath);
  if (commit.exitCode != 0)
    throw std::runtime_error("Git commit failed: " + commit.output);
  const CommandResult push = RunCommand(
      "set GIT_TERMINAL_PROMPT=0&& " + git + "push 2>&1");
  if (push.exitCode != 0)
    throw std::runtime_error("Commit created, but Git push failed: " +
                             push.output);
  return nlohmann::json({{"committed", true},
                         {"pushed", true},
                         {"title", title},
                         {"commitOutput", commit.output},
                         {"pushOutput", push.output},
                         {"repositoryWebUrl",
                          status.value("repositoryWebUrl", "")}})
      .dump();
}

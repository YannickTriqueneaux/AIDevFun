#include "GameHost/GameToolService.h"

#include "GameHost/ReloadableGame.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace
{
    constexpr std::size_t MaximumFileSize = 512 * 1024;
    constexpr std::size_t MaximumSearchResults = 100;

    bool IsAllowedSourceExtension(const std::filesystem::path& path)
    {
        static const std::unordered_set<std::string> extensions{
            ".cpp", ".h", ".hpp", ".inl"
        };
        return extensions.contains(path.extension().string());
    }

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        const std::uintmax_t size = std::filesystem::file_size(path);
        if (size > MaximumFileSize)
        {
            throw std::runtime_error("Game file exceeds the 512 KiB limit.");
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Unable to open Game file.");
        }

        return {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };
    }

    bool IsPathInside(
        const std::filesystem::path& child,
        const std::filesystem::path& parent)
    {
        auto childPart = child.begin();
        auto parentPart = parent.begin();
        for (; parentPart != parent.end(); ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
            {
                return false;
            }
        }
        return true;
    }
}

GameToolService::GameToolService(
    ReloadableGame& game,
    std::filesystem::path workspaceRoot,
    std::filesystem::path buildDirectory)
    : game_(game),
      workspaceRoot_(std::filesystem::weakly_canonical(workspaceRoot)),
      gameRoot_(std::filesystem::weakly_canonical(workspaceRoot_ / "Game")),
      buildDirectory_(std::filesystem::weakly_canonical(buildDirectory)),
      server_(
          std::string(Engine::GameToolsPipeName),
          [this](std::string_view request)
          {
              return HandleRequest(request);
          })
{
}

std::string GameToolService::HandleRequest(std::string_view request)
{
    try
    {
        const nlohmann::json input = nlohmann::json::parse(request);
        const std::string command = input.value("command", "");
        const nlohmann::json arguments =
            input.value("arguments", nlohmann::json::object());

        nlohmann::json result;

        if (command == "ping")
        {
            result = {
                {"service", "GameHost tools"},
                {"version", 1},
                {"reloadStatus", game_.GetReloadStatus()}
            };
        }
        else if (command == "read_game_file")
        {
            const std::filesystem::path file = ResolveGameFile(
                arguments.at("path").get<std::string>());
            result = {
                {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
                {"content", ReadTextFile(file)}
            };
        }
        else if (command == "list_game_files")
        {
            nlohmann::json files = nlohmann::json::array();
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(gameRoot_))
            {
                if (entry.is_regular_file() &&
                    IsAllowedSourceExtension(entry.path()))
                {
                    files.push_back(
                        std::filesystem::relative(
                            entry.path(),
                            gameRoot_).generic_string());
                }
            }
            result = {{"files", std::move(files)}};
        }
        else if (command == "search_game_code")
        {
            const std::string query =
                arguments.at("query").get<std::string>();
            if (query.empty() || query.size() > 256)
            {
                throw std::runtime_error(
                    "Search query must contain 1 to 256 characters.");
            }

            nlohmann::json matches = nlohmann::json::array();
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(gameRoot_))
            {
                if (!entry.is_regular_file() ||
                    !IsAllowedSourceExtension(entry.path()))
                {
                    continue;
                }

                std::ifstream stream(entry.path());
                std::string line;
                std::size_t lineNumber = 0;
                while (std::getline(stream, line))
                {
                    ++lineNumber;
                    if (line.find(query) != std::string::npos)
                    {
                        matches.push_back({
                            {"path", std::filesystem::relative(
                                entry.path(),
                                gameRoot_).generic_string()},
                            {"line", lineNumber},
                            {"text", line}
                        });
                        if (matches.size() >= MaximumSearchResults)
                        {
                            break;
                        }
                    }
                }
                if (matches.size() >= MaximumSearchResults)
                {
                    break;
                }
            }
            result = {{"matches", std::move(matches)}};
        }
        else if (command == "apply_game_patch")
        {
            const std::filesystem::path file = ResolveGameFile(
                arguments.at("path").get<std::string>());
            const std::string oldText =
                arguments.at("oldText").get<std::string>();
            const std::string newText =
                arguments.at("newText").get<std::string>();

            if (oldText.empty())
            {
                throw std::runtime_error("oldText cannot be empty.");
            }

            std::string content = ReadTextFile(file);
            const std::size_t position = content.find(oldText);
            if (position == std::string::npos)
            {
                throw std::runtime_error("oldText was not found.");
            }
            if (content.find(oldText, position + oldText.size()) !=
                std::string::npos)
            {
                throw std::runtime_error(
                    "oldText is ambiguous; provide a larger unique block.");
            }

            content.replace(position, oldText.size(), newText);
            if (content.size() > MaximumFileSize)
            {
                throw std::runtime_error(
                    "Patched file would exceed the 512 KiB limit.");
            }

            const std::filesystem::path temporary =
                file.string() + ".assistant.tmp";
            {
                std::ofstream stream(
                    temporary,
                    std::ios::binary | std::ios::trunc);
                if (!stream)
                {
                    throw std::runtime_error(
                        "Unable to create temporary Game file.");
                }
                stream.write(
                    content.data(),
                    static_cast<std::streamsize>(content.size()));
            }

#if defined(_WIN32)
            if (!MoveFileExW(
                    temporary.c_str(),
                    file.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::filesystem::remove(temporary);
                throw std::runtime_error(
                    "Atomic replacement of the Game file failed.");
            }
#else
            std::filesystem::rename(temporary, file);
#endif
            result = {
                {"path", std::filesystem::relative(file, gameRoot_).generic_string()},
                {"changed", true}
            };
        }
        else if (command == "build_game")
        {
            result = nlohmann::json::parse(BuildGame());
        }
        else if (command == "read_build_output")
        {
            std::scoped_lock lock(buildMutex_);
            result = {{"output", lastBuildOutput_}};
        }
        else if (command == "reload_game")
        {
            game_.RequestReload();
            result = {{"status", "Reload requested on the game thread."}};
        }
        else if (command == "get_reload_status")
        {
            result = {{"status", game_.GetReloadStatus()}};
        }
        else
        {
            throw std::runtime_error("Unknown or disallowed Game tool command.");
        }

        return nlohmann::json({
            {"ok", true},
            {"result", std::move(result)}
        }).dump();
    }
    catch (const std::exception& exception)
    {
        return nlohmann::json({
            {"ok", false},
            {"error", exception.what()}
        }).dump();
    }
}

std::filesystem::path GameToolService::ResolveGameFile(
    std::string_view relativePath) const
{
    const std::filesystem::path relative =
        std::filesystem::path(relativePath).lexically_normal();
    if (relative.empty() || relative.is_absolute())
    {
        throw std::runtime_error("Game path must be relative.");
    }
    for (const auto& component : relative)
    {
        if (component == "..")
        {
            throw std::runtime_error("Game path traversal is not allowed.");
        }
    }

    const std::filesystem::path resolved =
        std::filesystem::weakly_canonical(gameRoot_ / relative);
    if (!IsPathInside(resolved, gameRoot_) ||
        !IsAllowedSourceExtension(resolved))
    {
        throw std::runtime_error(
            "Only C++ source files inside Game are allowed.");
    }
    if (!std::filesystem::is_regular_file(resolved))
    {
        throw std::runtime_error("Game file does not exist.");
    }
    return resolved;
}

std::string GameToolService::BuildGame()
{
    std::scoped_lock lock(buildMutex_);

    const std::string command =
        "cmake --build \"" + buildDirectory_.string() +
        "\" --config Debug --target Game 2>&1";

    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe == nullptr)
    {
        throw std::runtime_error("Unable to start the controlled Game build.");
    }

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
    {
        output += buffer.data();
        if (output.size() > 2 * 1024 * 1024)
        {
            output += "\n[Build output truncated]\n";
            break;
        }
    }

    const int exitCode = _pclose(pipe);
    lastBuildOutput_ = output;
    return nlohmann::json({
        {"success", exitCode == 0},
        {"exitCode", exitCode},
        {"output", output}
    }).dump();
}

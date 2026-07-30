#pragma once

#include "Engine/IPC/NamedPipe.h"

#include <filesystem>
#include <mutex>
#include <string>

class ReloadableGame;

class GameToolService
{
public:
    GameToolService(
        ReloadableGame& game,
        std::filesystem::path workspaceRoot,
        std::filesystem::path buildDirectory);

private:
    [[nodiscard]] std::string HandleRequest(std::string_view request);
    [[nodiscard]] std::filesystem::path ResolveGameFile(
        std::string_view relativePath) const;
    [[nodiscard]] std::string BuildGame();

    ReloadableGame& game_;
    std::filesystem::path workspaceRoot_;
    std::filesystem::path gameRoot_;
    std::filesystem::path buildDirectory_;
    std::mutex buildMutex_;
    std::string lastBuildOutput_ = "No Game build has run yet.";
    Engine::NamedPipeServer server_;
};


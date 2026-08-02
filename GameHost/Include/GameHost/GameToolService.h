#pragma once

#include "Engine/IPC/NamedPipe.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

class ReloadableGame;

class GameToolService {
public:
  GameToolService(ReloadableGame *game, std::filesystem::path gameRoot,
                  std::filesystem::path buildDirectory,
                  std::filesystem::path runtimeDirectory,
                  bool recoveryMode = false);

  [[nodiscard]] bool IsLaunchRequested() const;

#if defined(ENGINE_AUTOTESTS)
  [[nodiscard]] std::string HandleRequestForAutoTest(std::string_view request) {
    return HandleRequest(request);
  }
#endif

private:
  [[nodiscard]] std::string HandleRequest(std::string_view request);
  [[nodiscard]] std::filesystem::path
  ResolveGameFile(std::string_view relativePath) const;
  [[nodiscard]] std::filesystem::path
  ResolveNewGameFile(std::string_view relativePath) const;
  [[nodiscard]] std::filesystem::path
  ResolveEngineFile(std::string_view relativePath) const;
  [[nodiscard]] std::filesystem::path
  ResolveAgentSkill(std::string_view name) const;
  [[nodiscard]] std::filesystem::path
  ResolveAgentDocument(std::string_view name) const;
  [[nodiscard]] std::string BuildGame();
  [[nodiscard]] std::string ReadCrashDiagnostics() const;

  ReloadableGame *game_ = nullptr;
  std::filesystem::path gameRoot_;
  std::filesystem::path engineRoot_;
  std::filesystem::path skillsRoot_;
  std::filesystem::path documentsRoot_;
  std::filesystem::path buildDirectory_;
  std::filesystem::path runtimeDirectory_;
  bool recoveryMode_ = false;
  std::atomic_bool launchRequested_ = false;
  std::mutex buildMutex_;
  std::string lastBuildOutput_ = "No Game build has run yet.";
  Engine::NamedPipeServer server_;
};

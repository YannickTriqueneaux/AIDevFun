#include "GameHost/ReloadableGame.h"

#include "Engine/Application/GameInstance.h"
#include "Engine/Core/Logger.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/DynamicLibrary.h"
#include "Engine/UI/UiSystem.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

struct ReloadableGame::LoadedGame {
  std::unique_ptr<Engine::DynamicLibrary> library;
  const Engine::GameModuleApi *api = nullptr;
  Engine::GameInterface *instance = nullptr;

  ~LoadedGame() {
    if (instance != nullptr && api != nullptr && api->destroyGame != nullptr) {
      api->destroyGame(instance);
    }
  }
};

ReloadableGame::ReloadableGame(std::filesystem::path sourceDll,
                               std::filesystem::path shadowDirectory)
    : sourceDll_(std::move(sourceDll)),
      shadowDirectory_(std::move(shadowDirectory)) {
  loaded_ = LoadNextGeneration();
  applicationConfig_ = loaded_->api->getApplicationConfig();
  Engine::Logger::Info("Initial Game DLL generation loaded.");
}

ReloadableGame::~ReloadableGame() = default;

Engine::GameApplicationConfig ReloadableGame::GetApplicationConfig() const {
  return applicationConfig_;
}

void ReloadableGame::RequestReload() {
  reloadRequested_ = true;
  SetReloadStatus("Reload requested.");
  Engine::Logger::Info("Game DLL reload requested through IPC.");
}

std::string ReloadableGame::GetReloadStatus() const {
  std::scoped_lock lock(statusMutex_);
  return reloadStatus_;
}

void ReloadableGame::Unload() { loaded_.reset(); }

void ReloadableGame::Initialize() {
  std::scoped_lock lock(gameMutex_);
  loaded_->instance->Initialize();
  initialized_ = true;
}

void ReloadableGame::Update(const Engine::InputSystem &input, float deltaTime) {
  std::scoped_lock lock(gameMutex_);
  ProcessReloadRequest();
  loaded_->instance->Update(input, deltaTime);
}

Engine::Color ReloadableGame::GetClearColor() const {
  std::scoped_lock lock(gameMutex_);
  return loaded_->instance->GetClearColor();
}

void ReloadableGame::Render(Engine::RenderContext &context) const {
  std::scoped_lock lock(gameMutex_);
  loaded_->instance->Render(context);
}

void ReloadableGame::RenderUi(Engine::UiSystem &ui) {
  std::scoped_lock lock(gameMutex_);
  loaded_->instance->RenderUi(ui);
}

void ReloadableGame::Shutdown() {
  std::scoped_lock lock(gameMutex_);
  if (loaded_) {
    loaded_->instance->Shutdown();
    initialized_ = false;
  }
}

#if defined(ENGINE_AUTOTESTS)
void ReloadableGame::SerializeAutoTestState(Engine::Serializer &serializer) {
  std::scoped_lock lock(gameMutex_);
  loaded_->instance->SerializeAutoTestState(serializer);
}

void ReloadableGame::ProcessAutoTestReload() {
  std::scoped_lock lock(gameMutex_);
  ProcessReloadRequest();
}
#endif

std::unique_ptr<ReloadableGame::LoadedGame>
ReloadableGame::LoadNextGeneration() {
  std::filesystem::create_directories(shadowDirectory_);

  ++generation_;
  std::ostringstream generationName;
  generationName << "Game_" << std::setw(4) << std::setfill('0') << generation_;

  const std::filesystem::path shadowDll =
      shadowDirectory_ / (generationName.str() + ".dll");
  std::filesystem::copy_file(sourceDll_, shadowDll,
                             std::filesystem::copy_options::overwrite_existing);

  const std::filesystem::path sourcePdb =
      sourceDll_.parent_path() / (sourceDll_.stem().string() + ".pdb");
  if (std::filesystem::exists(sourcePdb)) {
    std::filesystem::copy_file(
        sourcePdb, shadowDirectory_ / (generationName.str() + ".pdb"),
        std::filesystem::copy_options::overwrite_existing);
  }

  auto next = std::make_unique<LoadedGame>();
  next->library = std::make_unique<Engine::DynamicLibrary>(shadowDll);

  const auto getApi = reinterpret_cast<Engine::GetGameModuleApiFunction>(
      next->library->GetFunction(Engine::GetGameModuleApiFunctionName));
  next->api = getApi();

  if (next->api == nullptr ||
      next->api->apiVersion != Engine::GameModuleApiVersion ||
      next->api->getApplicationConfig == nullptr ||
      next->api->createGame == nullptr || next->api->destroyGame == nullptr) {
    throw std::runtime_error("Reloaded Game module API is incompatible.");
  }

  next->instance = next->api->createGame();
  if (next->instance == nullptr) {
    throw std::runtime_error("Reloaded Game module returned no instance.");
  }

  return next;
}

void ReloadableGame::ProcessReloadRequest() {
  if (!reloadRequested_.exchange(false)) {
    return;
  }

  Engine::GameInstance *previousInstance = Engine::GameInstance::GetInstance();
  const Engine::Gameplay::ObjectPoolDomain previousDomain =
      previousInstance ? previousInstance->GetObjectPoolDomain() : 0;
  try {
    const std::vector<std::byte> resumeState =
        loaded_->instance->SaveResumeState();
    std::unique_ptr<LoadedGame> next = LoadNextGeneration();
    Engine::GameInstance *nextInstance = Engine::GameInstance::GetInstance();
    if (!resumeState.empty()) {
      next->instance->ResumeFromState(resumeState);
    }
    if (initialized_) {
      next->instance->Initialize();
    }

    if (initialized_) {
      if (previousInstance)
        previousInstance->Activate();
      loaded_->instance->Shutdown();
      if (nextInstance)
        nextInstance->Activate();
    }
    loaded_ = std::move(next);
    if (previousDomain != 0 &&
        Engine::Gameplay::IsObjectPoolDomainAlive(previousDomain)) {
      std::terminate();
    }
    if (nextInstance) {
      const auto domainStats = Engine::Gameplay::GetObjectPoolDomainStats(
          nextInstance->GetObjectPoolDomain());
      const auto liveObjects = nextInstance->GetObjectManager()->LiveCount();
      if (domainStats.liveObjects != liveObjects)
        std::terminate();
      Engine::Logger::Info(
          "Retired Object pool domain " + std::to_string(previousDomain) +
          "; resumed " + std::to_string(liveObjects) + " objects in domain " +
          std::to_string(nextInstance->GetObjectPoolDomain()) + ".");
    }
    SetReloadStatus("Reloaded Game generation " + std::to_string(generation_) +
                    ".");
    Engine::Logger::Info("Reloaded Game DLL generation " +
                         std::to_string(generation_) + ".");
  } catch (const std::exception &exception) {
    if (previousInstance)
      previousInstance->Activate();
    SetReloadStatus(std::string("Reload failed: ") + exception.what());
    Engine::Logger::Error(std::string("Game DLL reload failed: ") +
                          exception.what());
  }
}

void ReloadableGame::SetReloadStatus(std::string status) {
  std::scoped_lock lock(statusMutex_);
  reloadStatus_ = std::move(status);
}

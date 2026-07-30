#include "GameHost/ReloadableGame.h"

#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Core/Logger.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/DynamicLibrary.h"
#include "Engine/UI/UiSystem.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

struct ReloadableGame::LoadedGame
{
    std::unique_ptr<Engine::DynamicLibrary> library;
    const Engine::GameModuleApi* api = nullptr;
    Engine::GameInterface* instance = nullptr;

    ~LoadedGame()
    {
        if (instance != nullptr && api != nullptr && api->destroyGame != nullptr)
        {
            api->destroyGame(instance);
        }
    }
};

ReloadableGame::ReloadableGame(
    std::filesystem::path sourceDll,
    std::filesystem::path shadowDirectory)
    : sourceDll_(std::move(sourceDll)),
      shadowDirectory_(std::move(shadowDirectory))
{
    loaded_ = LoadNextGeneration();
    applicationConfig_ = loaded_->api->getApplicationConfig();
    Engine::Logger::Info("Initial Game DLL generation loaded.");
}

ReloadableGame::~ReloadableGame() = default;

Engine::GameApplicationConfig ReloadableGame::GetApplicationConfig() const
{
    return applicationConfig_;
}

void ReloadableGame::RequestReload()
{
    reloadRequested_ = true;
    SetReloadStatus("Reload requested.");
    Engine::Logger::Info("Game DLL reload requested through IPC.");
}

std::string ReloadableGame::GetReloadStatus() const
{
    std::scoped_lock lock(statusMutex_);
    return reloadStatus_;
}

void ReloadableGame::Initialize()
{
    loaded_->instance->Initialize();
}

void ReloadableGame::Update(
    const Engine::InputSystem& input,
    float deltaTime)
{
    ProcessReloadRequest();
    loaded_->instance->Update(input, deltaTime);
}

Engine::Color ReloadableGame::GetClearColor() const
{
    return loaded_->instance->GetClearColor();
}

void ReloadableGame::Render(Engine::Renderer2D& renderer) const
{
    loaded_->instance->Render(renderer);
}

void ReloadableGame::RenderUi(Engine::UiSystem& ui)
{
    loaded_->instance->RenderUi(ui);
}

void ReloadableGame::Shutdown()
{
    loaded_->instance->Shutdown();
}

std::unique_ptr<ReloadableGame::LoadedGame>
ReloadableGame::LoadNextGeneration()
{
    std::filesystem::create_directories(shadowDirectory_);

    ++generation_;
    std::ostringstream generationName;
    generationName << "Game_" << std::setw(4) << std::setfill('0')
                   << generation_;

    const std::filesystem::path shadowDll =
        shadowDirectory_ / (generationName.str() + ".dll");
    std::filesystem::copy_file(
        sourceDll_,
        shadowDll,
        std::filesystem::copy_options::overwrite_existing);

    const std::filesystem::path sourcePdb =
        sourceDll_.parent_path() / (sourceDll_.stem().string() + ".pdb");
    if (std::filesystem::exists(sourcePdb))
    {
        std::filesystem::copy_file(
            sourcePdb,
            shadowDirectory_ / (generationName.str() + ".pdb"),
            std::filesystem::copy_options::overwrite_existing);
    }

    auto next = std::make_unique<LoadedGame>();
    next->library = std::make_unique<Engine::DynamicLibrary>(shadowDll);

    const auto getApi =
        reinterpret_cast<Engine::GetGameModuleApiFunction>(
            next->library->GetFunction(
                Engine::GetGameModuleApiFunctionName));
    next->api = getApi();

    if (next->api == nullptr ||
        next->api->apiVersion != Engine::GameModuleApiVersion ||
        next->api->getApplicationConfig == nullptr ||
        next->api->createGame == nullptr ||
        next->api->destroyGame == nullptr)
    {
        throw std::runtime_error("Reloaded Game module API is incompatible.");
    }

    next->instance = next->api->createGame();
    if (next->instance == nullptr)
    {
        throw std::runtime_error("Reloaded Game module returned no instance.");
    }

    return next;
}

void ReloadableGame::ProcessReloadRequest()
{
    if (!reloadRequested_.exchange(false))
    {
        return;
    }

    try
    {
        std::unique_ptr<LoadedGame> next = LoadNextGeneration();
        next->instance->Initialize();

        loaded_->instance->Shutdown();
        loaded_ = std::move(next);
        SetReloadStatus(
            "Reloaded Game generation " + std::to_string(generation_) + ".");
        Engine::Logger::Info(
            "Reloaded Game DLL generation " +
            std::to_string(generation_) + ".");
    }
    catch (const std::exception& exception)
    {
        SetReloadStatus(
            std::string("Reload failed: ") + exception.what());
        Engine::Logger::Error(
            std::string("Game DLL reload failed: ") + exception.what());
    }
}

void ReloadableGame::SetReloadStatus(std::string status)
{
    std::scoped_lock lock(statusMutex_);
    reloadStatus_ = std::move(status);
}

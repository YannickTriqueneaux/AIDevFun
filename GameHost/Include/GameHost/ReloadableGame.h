#pragma once

#include "Engine/Application/GameInterface.h"
#include "Engine/Application/GameModule.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace Engine
{
    class DynamicLibrary;
}

class ReloadableGame final : public Engine::GameInterface
{
public:
    ReloadableGame(
        std::filesystem::path sourceDll,
        std::filesystem::path shadowDirectory);
    ~ReloadableGame() override;

    ReloadableGame(const ReloadableGame&) = delete;
    ReloadableGame& operator=(const ReloadableGame&) = delete;

    [[nodiscard]] Engine::GameApplicationConfig GetApplicationConfig() const;
    void RequestReload();
    [[nodiscard]] std::string GetReloadStatus() const;

    void Initialize() override;
    void Update(const Engine::InputSystem& input, float deltaTime) override;
    [[nodiscard]] Engine::Color GetClearColor() const override;
    void Render(Engine::Renderer2D& renderer) const override;
    void RenderUi(Engine::UiSystem& ui) override;
    void Shutdown() override;

private:
    struct LoadedGame;

    [[nodiscard]] std::unique_ptr<LoadedGame> LoadNextGeneration();
    void ProcessReloadRequest();
    void SetReloadStatus(std::string status);

    std::filesystem::path sourceDll_;
    std::filesystem::path shadowDirectory_;
    std::unique_ptr<LoadedGame> loaded_;
    Engine::GameApplicationConfig applicationConfig_{};
    std::atomic_bool reloadRequested_ = false;
    std::uint64_t generation_ = 0;
    mutable std::mutex statusMutex_;
    std::string reloadStatus_ = "Initial Game module loaded.";
};


#pragma once

#include "Engine/Application/GameInstance.h"
#include "Engine/Application/GameInterface.h"

#include "Game/GameInput.h"
#include "Game/GameplayComponents.h"

#include <memory>

struct ArtResources;

class ProceduralGame final : public Engine::GameInterface {
public:
  ProceduralGame();
  ~ProceduralGame() override;
  void Initialize() override;
  void Update(const Engine::InputSystem &input, float deltaTime) override;
  [[nodiscard]] Engine::Color GetClearColor() const override;
  void Render(Engine::RenderContext &context) const override;
  void Shutdown() override;
  [[nodiscard]] std::vector<std::byte> SaveResumeState() const override;
  void ResumeFromState(std::span<const std::byte> state) override;
#if defined(ENGINE_AUTOTESTS)
  void SerializeAutoTestState(Engine::Serializer &serializer) override;
#endif

private:
  void RegisterGameplayTypes();
  void EnsureCoreEntities();
  [[nodiscard]] Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  SpawnEnemy(Engine::Vector2 position);
  [[nodiscard]] Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  SpawnProjectile(Engine::Gameplay::TypeID type, Engine::Vector2 position,
                  Engine::Vector2 direction, BaseGame::Faction faction);
  void ProcessFrameRequests();
  void ProcessCollisionsAndLifetime();

  Engine::GameInstance gameInstance_;
  GameInput inputBindings_;
  Engine::Gameplay::World &world_;
  std::unique_ptr<ArtResources> art_;
  float visualTime_ = 0.0f;
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> playerEntity_;
  Engine::Gameplay::ObjectRef<BaseGame::PlayerMovement> playerMovement_;
  Engine::Gameplay::ObjectRef<BaseGame::PlayerWeapon> playerWeapon_;
  Engine::Gameplay::ObjectRef<BaseGame::Health> playerHealth_;
  Engine::Gameplay::ObjectRef<BaseGame::ArenaDirector> arenaDirector_;
};

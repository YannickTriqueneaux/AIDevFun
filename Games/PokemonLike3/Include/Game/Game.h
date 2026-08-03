#pragma once

#include "Engine/Application/GameInstance.h"
#include "Engine/Application/GameInterface.h"
#include "Engine/Audio/ProceduralAudio.h"

#include "Game/GameInput.h"
#include "Game/GameplayComponents.h"

class ProceduralGame final : public Engine::GameInterface {
public:
  ProceduralGame();
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
  void EnsureTownEntities();
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  SpawnNpc(PocketTown::NpcRole role, Engine::Vector2 position, float radius,
           std::uint32_t seed);
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  SpawnPokemon(PocketTown::PokemonSpecies species, Engine::Vector2 position,
               float radius, std::uint32_t seed);
  void UpdateFootsteps(float deltaTime);
  void UpdateNpcTalk(float deltaTime);
  void UpdatePokemonCries(float deltaTime);
  [[nodiscard]] Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  FindNearestTalker() const;
  [[nodiscard]] Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>
  FindNearestPokemon(float range) const;

  Engine::GameInstance gameInstance_;
  GameInput inputBindings_;
  Engine::Gameplay::World &world_;
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> playerEntity_;
  Engine::Gameplay::ObjectRef<PocketTown::CharacterMotion> playerMotion_;
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> activeTalker_;
  Engine::ProceduralSound footstepSound_;
  Engine::ProceduralSound talkSound_;
  Engine::ProceduralSound pokemonCrySound_;
  float footstepTimer_ = 0.0f;
  float talkBubbleTimer_ = 0.0f;
  float talkSoundTimer_ = 0.0f;
  float pokemonCryTimer_ = 0.0f;
};

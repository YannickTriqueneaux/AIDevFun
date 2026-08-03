#pragma once

#include "Game/GameInput.h"

#include "Engine/Gameplay/Object.h"

#include <cstdint>

namespace TownRpg {
using Engine::Gameplay::ObjectRef;
using Engine::Gameplay::StateReader;
using Engine::Gameplay::StateWriter;
using Engine::Gameplay::TypeID;

inline constexpr TypeID PlayerEntityType =
    Engine::Gameplay::StableTypeID("TownRpg.PlayerEntity.v1");
inline constexpr TypeID NpcEntityType =
    Engine::Gameplay::StableTypeID("TownRpg.NpcEntity.v1");

class PlayerWalk final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("TownRpg.PlayerWalk.v1");
  explicit PlayerWalk(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetCommand(PlayerCommand command) { command_ = command; }
  [[nodiscard]] Engine::Vector2 Facing() const { return facing_; }
  [[nodiscard]] float WalkCycle() const { return walkCycle_; }
  [[nodiscard]] bool IsWalking() const { return walking_; }
  [[nodiscard]] bool ConsumeFootstep();
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Engine::Vector2 facing_{0.0f, 1.0f};
  PlayerCommand command_{};
  float movementSpeed_ = 150.0f;
  float walkCycle_ = 0.0f;
  bool walking_ = false;
  bool footstepPending_ = false;
};

class NpcWander final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("TownRpg.NpcWander.v1");
  explicit NpcWander(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(Engine::Vector2 home, std::uint32_t seed);
  [[nodiscard]] Engine::Vector2 Facing() const { return facing_; }
  [[nodiscard]] float WalkCycle() const { return walkCycle_; }
  [[nodiscard]] bool IsWalking() const { return walking_; }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  [[nodiscard]] float RandomUnit();
  Engine::Vector2 home_{};
  Engine::Vector2 facing_{0.0f, 1.0f};
  Engine::Vector2 direction_{};
  float decisionTimer_ = 0.0f;
  float movementSpeed_ = 48.0f;
  float walkCycle_ = 0.0f;
  bool walking_ = false;
  std::uint32_t randomState_ = 0x1234abcdU;
};
} // namespace TownRpg

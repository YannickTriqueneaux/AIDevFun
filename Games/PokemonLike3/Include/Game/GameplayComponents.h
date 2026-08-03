#pragma once

#include "Game/GameInput.h"

#include "Engine/Gameplay/Object.h"

#include <cstdint>

namespace PocketTown {
using Engine::Gameplay::ObjectRef;
using Engine::Gameplay::StateReader;
using Engine::Gameplay::StateWriter;
using Engine::Gameplay::TypeID;

inline constexpr TypeID PlayerEntityType =
    Engine::Gameplay::StableTypeID("PocketTown.PlayerEntity.v1");
inline constexpr TypeID NpcEntityType =
    Engine::Gameplay::StableTypeID("PocketTown.NpcEntity.v1");
inline constexpr TypeID PokemonEntityType =
    Engine::Gameplay::StableTypeID("PocketTown.PokemonEntity.v1");

enum class Direction : std::uint8_t { Down, Left, Right, Up };
enum class NpcRole : std::uint8_t { Elder, Clerk, Kid, Fisher };
enum class PokemonSpecies : std::uint8_t { Leafling, Aquabbit, Embercub, Sparko, Puffowl };

class CharacterMotion final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("PocketTown.CharacterMotion.v1");
  explicit CharacterMotion(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetCommand(PlayerCommand command) { command_ = command; }
  [[nodiscard]] Engine::Vector2 Velocity() const { return velocity_; }
  [[nodiscard]] Direction Facing() const { return facing_; }
  [[nodiscard]] float WalkCycle() const { return walkCycle_; }
  [[nodiscard]] bool IsMoving() const;
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  PlayerCommand command_{};
  Engine::Vector2 velocity_{};
  Direction facing_ = Direction::Down;
  float walkCycle_ = 0.0f;
  float movementSpeed_ = 150.0f;
};

class PokemonRoam final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("PocketTown.PokemonRoam.v1");
  explicit PokemonRoam(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(PokemonSpecies species, Engine::Vector2 home, float radius,
                 std::uint32_t seed);
  [[nodiscard]] PokemonSpecies Species() const { return species_; }
  [[nodiscard]] Direction Facing() const { return facing_; }
  [[nodiscard]] float BobCycle() const { return bobCycle_; }
  [[nodiscard]] bool IsMoving() const { return moving_; }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  [[nodiscard]] float RandomUnit();
  void ChooseNewStep();

  PokemonSpecies species_ = PokemonSpecies::Leafling;
  Engine::Vector2 home_{};
  Engine::Vector2 target_{};
  Direction facing_ = Direction::Down;
  float radius_ = 80.0f;
  float waitTimer_ = 0.6f;
  float bobCycle_ = 0.0f;
  float movementSpeed_ = 72.0f;
  std::uint32_t randomState_ = 0x8142u;
  bool moving_ = false;
};

class NpcWander final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("PocketTown.NpcWander.v1");
  explicit NpcWander(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(NpcRole role, Engine::Vector2 home, float radius,
                 std::uint32_t seed);
  [[nodiscard]] NpcRole Role() const { return role_; }
  [[nodiscard]] Direction Facing() const { return facing_; }
  [[nodiscard]] float WalkCycle() const { return walkCycle_; }
  [[nodiscard]] bool IsMoving() const { return moving_; }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  [[nodiscard]] float RandomUnit();
  void ChooseNewStep();

  NpcRole role_ = NpcRole::Elder;
  Engine::Vector2 home_{};
  Engine::Vector2 target_{};
  Direction facing_ = Direction::Down;
  float radius_ = 48.0f;
  float waitTimer_ = 0.8f;
  float walkCycle_ = 0.0f;
  float movementSpeed_ = 58.0f;
  std::uint32_t randomState_ = 0x1234567u;
  bool moving_ = false;
};
} // namespace PocketTown

#pragma once

#include "Game/GameInput.h"

#include "Engine/Gameplay/Object.h"

#include <cstdint>

namespace BaseGame {
using Engine::Gameplay::ObjectRef;
using Engine::Gameplay::StateReader;
using Engine::Gameplay::StateWriter;
using Engine::Gameplay::TypeID;

inline constexpr TypeID ArenaDirectorEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.ArenaDirectorEntity.v2");
inline constexpr TypeID PlayerEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.PlayerEntity.v2");
inline constexpr TypeID EnemyEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.EnemyEntity.v2");
inline constexpr TypeID PlayerProjectileEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.PlayerProjectileEntity.v2");
inline constexpr TypeID EnemyProjectileEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.EnemyProjectileEntity.v2");

enum class Faction : std::uint8_t { Player, Enemy };

class ArenaDirector final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.ArenaDirector.v2");
  explicit ArenaDirector(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Update(float deltaTime) override;
  [[nodiscard]] bool ConsumeEnemySpawn(Engine::Vector2 &position);
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  [[nodiscard]] float RandomUnit();
  float spawnCooldown_ = 0.05f;
  std::uint32_t randomState_ = 0x51a7e5u;
  bool spawnPending_ = false;
  Engine::Vector2 pendingPosition_{};
};

class PlayerMovement final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.PlayerMovement.v2");
  explicit PlayerMovement(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetCommand(PlayerCommand command) { command_ = command; }
  void SetFacing(Engine::Vector2 facing) { facing_ = facing; }
  [[nodiscard]] Engine::Vector2 Facing() const { return facing_; }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Engine::Vector2 facing_{1.0f, 0.0f};
  PlayerCommand command_{};
  float movementSpeed_ = 190.0f;
};

class PlayerWeapon final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.PlayerWeapon.v2");
  explicit PlayerWeapon(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetTrigger(bool firing, Engine::Vector2 direction);
  [[nodiscard]] bool ConsumeShot(Engine::Vector2 &direction);
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Engine::Vector2 aim_{1.0f, 0.0f};
  float cooldown_ = 0.0f;
  bool trigger_ = false;
  bool shotPending_ = false;
};

class EnemyMovement final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.EnemyMovement.v2");
  explicit EnemyMovement(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetTarget(ObjectRef<Engine::Gameplay::Entity> target) {
    target_ = target;
  }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  ObjectRef<Engine::Gameplay::Entity> target_;
  float movementSpeed_ = 72.0f;
};

class EnemyWeapon final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.EnemyWeapon.v2");
  explicit EnemyWeapon(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void SetTarget(ObjectRef<Engine::Gameplay::Entity> target) {
    target_ = target;
  }
  [[nodiscard]] bool ConsumeShot(Engine::Vector2 &direction);
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  ObjectRef<Engine::Gameplay::Entity> target_;
  float cooldown_ = 0.8f;
  bool shotPending_ = false;
  Engine::Vector2 pendingDirection_{};
};

class Health final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.Health.v2");
  explicit Health(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(Faction faction, int hitPoints);
  void Damage(int amount) { hitPoints_ -= amount; }
  [[nodiscard]] bool IsDead() const { return hitPoints_ <= 0; }
  [[nodiscard]] int HitPoints() const { return hitPoints_; }
  [[nodiscard]] Faction GetFaction() const { return faction_; }
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Faction faction_ = Faction::Enemy;
  int hitPoints_ = 1;
};

class ProjectileMovement final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.ProjectileMovement.v2");
  explicit ProjectileMovement(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(Engine::Vector2 velocity, float lifetime);
  [[nodiscard]] bool IsExpired() const { return lifetime_ <= 0.0f; }
  void Update(float deltaTime) override;
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Engine::Vector2 velocity_{};
  float lifetime_ = 0.0f;
};

class ProjectileDamage final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type =
      Engine::Gameplay::StableTypeID("BaseGame.ProjectileDamage.v2");
  explicit ProjectileDamage(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }
  void Configure(Faction faction, int damage);
  [[nodiscard]] Faction GetFaction() const { return faction_; }
  [[nodiscard]] int DamageAmount() const { return damage_; }
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  Faction faction_ = Faction::Enemy;
  int damage_ = 1;
};
} // namespace BaseGame

#include "Game/GameplayComponents.h"

#include "Game/GameConfig.h"

#include "Engine/Gameplay/ObjectManager.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace BaseGame {
namespace {
constexpr float PlayerRadius = 12.0f;

struct SolidRect {
  float x;
  float y;
  float width;
  float height;
};

bool CircleOverlapsRect(Engine::Vector2 center, float radius, SolidRect rect) {
  const float closestX = std::clamp(center.x, rect.x, rect.x + rect.width);
  const float closestY = std::clamp(center.y, rect.y, rect.y + rect.height);
  const float dx = center.x - closestX;
  const float dy = center.y - closestY;
  return dx * dx + dy * dy <= radius * radius;
}

bool HitsSolidTile(Engine::Vector2 center) {
  constexpr SolidRect solidRects[]{
      {360.0f, 468.0f, 120.0f, 100.0f},  {760.0f, 528.0f, 120.0f, 100.0f},
      {1420.0f, 428.0f, 120.0f, 100.0f}, {1740.0f, 768.0f, 120.0f, 100.0f},
      {2150.0f, 568.0f, 120.0f, 100.0f}, {2760.0f, 438.0f, 120.0f, 100.0f},
      {3200.0f, 708.0f, 120.0f, 100.0f}, {3660.0f, 508.0f, 120.0f, 100.0f},
      {40.0f, 40.0f, 4016.0f, 28.0f},    {40.0f, 1212.0f, 4016.0f, 28.0f},
      {40.0f, 40.0f, 28.0f, 1200.0f},    {4028.0f, 40.0f, 28.0f, 1200.0f},
  };
  for (const SolidRect rect : solidRects) {
    if (CircleOverlapsRect(center, PlayerRadius, rect))
      return true;
  }
  return false;
}

Engine::Vector2 Direction(Engine::Vector2 from, Engine::Vector2 to) {
  const Engine::Vector2 delta{to.x - from.x, to.y - from.y};
  const float lengthSquared = delta.x * delta.x + delta.y * delta.y;
  if (lengthSquared <= 0.0001f)
    return {};
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  return delta * inverseLength;
}

void RequireVersion(std::uint32_t version, const char *component) {
  if (version != 1)
    throw std::runtime_error(std::string("Unsupported ") + component +
                             " state version.");
}
} // namespace

ArenaDirector::ArenaDirector(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void ArenaDirector::Update(float deltaTime) {
  (void)deltaTime;
  spawnPending_ = false;
}

bool ArenaDirector::ConsumeEnemySpawn(Engine::Vector2 &position) {
  if (!spawnPending_)
    return false;
  position = pendingPosition_;
  spawnPending_ = false;
  return true;
}

void ArenaDirector::SaveState(StateWriter &writer) const {
  writer.Value(spawnCooldown_);
  writer.Value(randomState_);
  writer.Value(spawnPending_);
  writer.Value(pendingPosition_);
}

void ArenaDirector::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "ArenaDirector");
  spawnCooldown_ = reader.Value<float>();
  randomState_ = reader.Value<std::uint32_t>();
  spawnPending_ = reader.Value<bool>();
  pendingPosition_ = reader.Value<Engine::Vector2>();
}

float ArenaDirector::RandomUnit() {
  randomState_ = randomState_ * 1664525u + 1013904223u;
  return static_cast<float>(randomState_ >> 8u) /
         static_cast<float>(0x01000000u);
}

PlayerMovement::PlayerMovement(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void PlayerMovement::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  if (command_.movement.x != 0.0f || command_.movement.y != 0.0f)
    facing_ = command_.movement;

  const float frameMove = movementSpeed_ * deltaTime;
  const Engine::Vector2 previous = entity->transform.position;

  Engine::Vector2 attempt = previous;
  attempt.x += command_.movement.x * frameMove;
  attempt.x =
      std::clamp(attempt.x, PlayerRadius,
                 static_cast<float>(GameConfig::WorldWidth) - PlayerRadius);
  if (!HitsSolidTile(attempt))
    entity->transform.position.x = attempt.x;

  attempt = entity->transform.position;
  attempt.y += command_.movement.y * frameMove;
  attempt.y =
      std::clamp(attempt.y, PlayerRadius,
                 static_cast<float>(GameConfig::WorldHeight) - PlayerRadius);
  if (!HitsSolidTile(attempt))
    entity->transform.position.y = attempt.y;

  command_.movement = {};
}

void PlayerMovement::SaveState(StateWriter &writer) const {
  writer.Value(facing_);
  writer.Value(command_);
  writer.Value(movementSpeed_);
}

void PlayerMovement::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "PlayerMovement");
  facing_ = reader.Value<Engine::Vector2>();
  command_ = reader.Value<PlayerCommand>();
  movementSpeed_ = std::max(reader.Value<float>(), 190.0f);
}

PlayerWeapon::PlayerWeapon(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void PlayerWeapon::SetTrigger(bool firing, Engine::Vector2 direction) {
  trigger_ = firing;
  if (direction.x != 0.0f || direction.y != 0.0f)
    aim_ = direction;
}

bool PlayerWeapon::ConsumeShot(Engine::Vector2 &direction) {
  if (!shotPending_)
    return false;
  direction = aim_;
  shotPending_ = false;
  return true;
}

void PlayerWeapon::Update(float deltaTime) {
  cooldown_ = std::max(0.0f, cooldown_ - deltaTime);
  if (trigger_ && cooldown_ <= 0.0f && !shotPending_) {
    shotPending_ = true;
    cooldown_ = 0.16f;
  }
  trigger_ = false;
}

void PlayerWeapon::SaveState(StateWriter &writer) const {
  writer.Value(aim_);
  writer.Value(cooldown_);
  writer.Value(trigger_);
  writer.Value(shotPending_);
}

void PlayerWeapon::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "PlayerWeapon");
  aim_ = reader.Value<Engine::Vector2>();
  cooldown_ = reader.Value<float>();
  trigger_ = reader.Value<bool>();
  shotPending_ = reader.Value<bool>();
}

EnemyMovement::EnemyMovement(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void EnemyMovement::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  const auto *target = target_.Resolve();
  if (!entity || !target)
    return;
  const Engine::Vector2 direction =
      Direction(entity->transform.position, target->transform.position);
  entity->transform.position += direction * (movementSpeed_ * deltaTime);
}

void EnemyMovement::SaveState(StateWriter &writer) const {
  writer.Value(target_.GetID());
  writer.Value(movementSpeed_);
}

void EnemyMovement::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "EnemyMovement");
  target_ = ObjectRef<Engine::Gameplay::Entity>(
      reader.Value<Engine::Gameplay::ObjectID>());
  movementSpeed_ = reader.Value<float>();
}

EnemyWeapon::EnemyWeapon(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

bool EnemyWeapon::ConsumeShot(Engine::Vector2 &direction) {
  if (!shotPending_)
    return false;
  direction = pendingDirection_;
  shotPending_ = false;
  return true;
}

void EnemyWeapon::Update(float deltaTime) {
  cooldown_ -= deltaTime;
  const auto *owner = GetOwner().Resolve();
  const auto *target = target_.Resolve();
  if (!owner || !target || cooldown_ > 0.0f || shotPending_)
    return;
  pendingDirection_ =
      Direction(owner->transform.position, target->transform.position);
  shotPending_ = true;
  cooldown_ = 0.9f;
}

void EnemyWeapon::SaveState(StateWriter &writer) const {
  writer.Value(target_.GetID());
  writer.Value(cooldown_);
  writer.Value(shotPending_);
  writer.Value(pendingDirection_);
}

void EnemyWeapon::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "EnemyWeapon");
  target_ = ObjectRef<Engine::Gameplay::Entity>(
      reader.Value<Engine::Gameplay::ObjectID>());
  cooldown_ = reader.Value<float>();
  shotPending_ = reader.Value<bool>();
  pendingDirection_ = reader.Value<Engine::Vector2>();
}

Health::Health(ObjectRef<Engine::Gameplay::Entity> owner) : Component(owner) {}

void Health::Configure(Faction faction, int hitPoints) {
  faction_ = faction;
  hitPoints_ = hitPoints;
}

void Health::SaveState(StateWriter &writer) const {
  writer.Value(faction_);
  writer.Value(hitPoints_);
}

void Health::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "Health");
  faction_ = reader.Value<Faction>();
  hitPoints_ = reader.Value<int>();
}

ProjectileMovement::ProjectileMovement(
    ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void ProjectileMovement::Configure(Engine::Vector2 velocity, float lifetime) {
  velocity_ = velocity;
  lifetime_ = lifetime;
}

void ProjectileMovement::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;
  entity->transform.position += velocity_ * deltaTime;
  lifetime_ -= deltaTime;
}

void ProjectileMovement::SaveState(StateWriter &writer) const {
  writer.Value(velocity_);
  writer.Value(lifetime_);
}

void ProjectileMovement::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "ProjectileMovement");
  velocity_ = reader.Value<Engine::Vector2>();
  lifetime_ = reader.Value<float>();
}

ProjectileDamage::ProjectileDamage(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void ProjectileDamage::Configure(Faction faction, int damage) {
  faction_ = faction;
  damage_ = damage;
}

void ProjectileDamage::SaveState(StateWriter &writer) const {
  writer.Value(faction_);
  writer.Value(damage_);
}

void ProjectileDamage::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "ProjectileDamage");
  faction_ = reader.Value<Faction>();
  damage_ = reader.Value<int>();
}
} // namespace BaseGame

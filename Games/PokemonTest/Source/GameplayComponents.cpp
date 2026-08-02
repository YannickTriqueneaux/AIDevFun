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

DragonFollower::DragonFollower(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

float DragonFollower::FireIntensity() const {
  if (!alive_ || breathTime_ <= 0.0f)
    return 0.0f;
  constexpr float BreathDuration = 0.85f;
  const float progress =
      std::clamp(1.0f - breathTime_ / BreathDuration, 0.0f, 1.0f);
  return std::sin(progress * 3.14159265f);
}

float DragonFollower::RespawnProgress() const {
  if (alive_)
    return 1.0f;
  return std::clamp(1.0f - respawnTimer_ / 3.0f, 0.0f, 1.0f);
}

void DragonFollower::Kill() {
  if (!alive_)
    return;
  alive_ = false;
  respawnTimer_ = 3.0f;
  breathTime_ = 0.0f;
  breathCooldown_ = 1.25f;
}

void DragonFollower::Update(float deltaTime) {
  if (!alive_) {
    respawnTimer_ = std::max(0.0f, respawnTimer_ - deltaTime);
    if (respawnTimer_ <= 0.0f)
      alive_ = true;
    else
      return;
  }

  breathCooldown_ -= deltaTime;
  if (breathTime_ > 0.0f) {
    breathTime_ = std::max(0.0f, breathTime_ - deltaTime);
  } else if (breathCooldown_ <= 0.0f) {
    breathTime_ = 0.85f;
    breathCooldown_ = 3.4f;
  }

  auto *entity = GetOwner().Resolve();
  const auto *target = target_.Resolve();
  if (!entity || !target)
    return;

  const Engine::Vector2 desired{target->transform.position.x - followDistance_,
                                target->transform.position.y - 34.0f};
  const Engine::Vector2 toDesired{
      desired.x - entity->transform.position.x,
      desired.y - entity->transform.position.y};
  const float distanceSquared =
      toDesired.x * toDesired.x + toDesired.y * toDesired.y;
  if (distanceSquared <= 0.25f)
    return;
  if (distanceSquared > 280.0f * 280.0f) {
    entity->transform.position = desired;
    return;
  }
  const float distance = std::sqrt(distanceSquared);
  const float step = std::min(distance, movementSpeed_ * deltaTime);
  entity->transform.position += toDesired * (step / distance);
}

void DragonFollower::SaveState(StateWriter &writer) const {
  writer.Value(target_.GetID());
  writer.Value(movementSpeed_);
  writer.Value(followDistance_);
  writer.Value(breathCooldown_);
  writer.Value(breathTime_);
  writer.Value(respawnTimer_);
  writer.Value(alive_);
}

void DragonFollower::LoadState(StateReader &reader, std::uint32_t version) {
  if (version < 1 || version > 3)
    throw std::runtime_error("Unsupported DragonFollower state version.");
  target_ = ObjectRef<Engine::Gameplay::Entity>(
      reader.Value<Engine::Gameplay::ObjectID>());
  movementSpeed_ = reader.Value<float>();
  followDistance_ = reader.Value<float>();
  if (version >= 2) {
    breathCooldown_ = reader.Value<float>();
    breathTime_ = reader.Value<float>();
  } else {
    breathCooldown_ = 1.25f;
    breathTime_ = 0.0f;
  }
  if (version >= 3) {
    respawnTimer_ = reader.Value<float>();
    alive_ = reader.Value<bool>();
  } else {
    respawnTimer_ = 0.0f;
    alive_ = true;
  }
}

KnightVisitor::KnightVisitor(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

bool KnightVisitor::ConsumeDragonAttack() {
  if (!attackPending_)
    return false;
  attackPending_ = false;
  return true;
}

void KnightVisitor::Update(float deltaTime) {
  attackFlash_ = std::max(0.0f, attackFlash_ - deltaTime);
  auto *entity = GetOwner().Resolve();
  auto *target = target_.Resolve();
  if (!entity || !target)
    return;

  const auto dragonFollower = target->GetComponent<DragonFollower>();
  const auto *dragon = dragonFollower.Resolve();
  const bool dragonAlive = dragon && dragon->IsAlive();

  if (!active_) {
    spawnCooldown_ = std::max(0.0f, spawnCooldown_ - deltaTime);
    if (spawnCooldown_ > 0.0f || !dragonAlive)
      return;
    active_ = true;
    attackPending_ = false;
    entity->transform.position = {target->transform.position.x - 360.0f,
                                  target->transform.position.y + 24.0f};
  }

  entity->transform.position.x += movementSpeed_ * deltaTime;
  const float desiredY = target->transform.position.y + 24.0f;
  entity->transform.position.y +=
      (desiredY - entity->transform.position.y) * std::min(1.0f, deltaTime * 4.0f);

  const Engine::Vector2 delta{target->transform.position.x - entity->transform.position.x,
                              target->transform.position.y - entity->transform.position.y};
  if (dragonAlive && !attackPending_ && attackFlash_ <= 0.0f &&
      std::abs(delta.x) <= 30.0f && std::abs(delta.y) <= 44.0f) {
    attackPending_ = true;
    attackFlash_ = 0.42f;
  }

  if (entity->transform.position.x > target->transform.position.x + 430.0f) {
    active_ = false;
    attackPending_ = false;
    spawnCooldown_ = 7.5f;
  }
}

void KnightVisitor::SaveState(StateWriter &writer) const {
  writer.Value(target_.GetID());
  writer.Value(spawnCooldown_);
  writer.Value(attackFlash_);
  writer.Value(movementSpeed_);
  writer.Value(active_);
  writer.Value(attackPending_);
}

void KnightVisitor::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "KnightVisitor");
  target_ = ObjectRef<Engine::Gameplay::Entity>(
      reader.Value<Engine::Gameplay::ObjectID>());
  spawnCooldown_ = reader.Value<float>();
  attackFlash_ = reader.Value<float>();
  movementSpeed_ = reader.Value<float>();
  active_ = reader.Value<bool>();
  attackPending_ = reader.Value<bool>();
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

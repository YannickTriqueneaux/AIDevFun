#include "Game/GameplayComponents.h"

#include "Game/GameConfig.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace PocketTown {
namespace {
constexpr float CharacterHalfWidth = 13.0f;
constexpr float CharacterHalfHeight = 18.0f;
constexpr float TownLeft = 44.0f;
constexpr float TownRight = static_cast<float>(GameConfig::WorldWidth) - 44.0f;
constexpr float TownTop = 58.0f;
constexpr float TownBottom = static_cast<float>(GameConfig::WorldHeight) - 54.0f;

void RequireVersion(std::uint32_t version, const char *component) {
  if (version != 1)
    throw std::runtime_error(std::string("Unsupported ") + component +
                             " state version.");
}

Direction DirectionFromVector(Engine::Vector2 value, Direction fallback) {
  if (std::abs(value.x) > std::abs(value.y))
    return value.x < 0.0f ? Direction::Left : Direction::Right;
  if (std::abs(value.y) > 0.0001f)
    return value.y < 0.0f ? Direction::Up : Direction::Down;
  return fallback;
}

Engine::Vector2 DirectionVector(Direction direction) {
  switch (direction) {
  case Direction::Down:
    return {0.0f, 1.0f};
  case Direction::Left:
    return {-1.0f, 0.0f};
  case Direction::Right:
    return {1.0f, 0.0f};
  case Direction::Up:
    return {0.0f, -1.0f};
  }
  return {0.0f, 1.0f};
}

Engine::Vector2 NormalizeOrZero(Engine::Vector2 value) {
  const float lengthSquared = value.x * value.x + value.y * value.y;
  if (lengthSquared <= 0.0001f)
    return {};
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  return value * inverseLength;
}

float DistanceSquared(Engine::Vector2 left, Engine::Vector2 right) {
  const float x = left.x - right.x;
  const float y = left.y - right.y;
  return x * x + y * y;
}

struct SolidRect {
  Engine::Vector2 center;
  Engine::Vector2 halfSize;
};

constexpr SolidRect HouseSolids[] = {
    {{1010.0f, 204.0f}, {94.0f, 58.0f}},
    {{210.0f, 568.0f}, {94.0f, 58.0f}},
    {{720.0f, 578.0f}, {94.0f, 58.0f}},
};

void ClampToTownBounds(Engine::Vector2 &position) {
  position.x = std::clamp(position.x, TownLeft + CharacterHalfWidth,
                          TownRight - CharacterHalfWidth);
  position.y = std::clamp(position.y, TownTop + CharacterHalfHeight,
                          TownBottom - CharacterHalfHeight);
}

bool IsBlockedByHouses(Engine::Vector2 position) {
  const float segmentWidth = static_cast<float>(GameConfig::PlayAreaWidth);
  const int segment = std::clamp(static_cast<int>(position.x / segmentWidth), 0, 3);
  const float localX = position.x - static_cast<float>(segment) * segmentWidth;
  for (const SolidRect &house : HouseSolids) {
    if (localX + CharacterHalfWidth < house.center.x - house.halfSize.x ||
        localX - CharacterHalfWidth > house.center.x + house.halfSize.x ||
        position.y + CharacterHalfHeight < house.center.y - house.halfSize.y ||
        position.y - CharacterHalfHeight > house.center.y + house.halfSize.y)
      continue;
    return true;
  }
  return false;
}

void MoveWithTownCollision(Engine::Gameplay::Entity &entity, Engine::Vector2 delta) {
  Engine::Vector2 position = entity.transform.position;
  Engine::Vector2 next{position.x + delta.x, position.y};
  ClampToTownBounds(next);
  if (!IsBlockedByHouses(next))
    position.x = next.x;

  next = {position.x, position.y + delta.y};
  ClampToTownBounds(next);
  if (!IsBlockedByHouses(next))
    position.y = next.y;

  entity.transform.position = position;
}
} // namespace

CharacterMotion::CharacterMotion(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

bool CharacterMotion::IsMoving() const {
  return velocity_.x != 0.0f || velocity_.y != 0.0f;
}

void CharacterMotion::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  const Engine::Vector2 movement = NormalizeOrZero(command_.movement);
  velocity_ = movement * movementSpeed_;
  if (movement.x != 0.0f || movement.y != 0.0f) {
    facing_ = DirectionFromVector(movement, facing_);
    walkCycle_ += deltaTime * 8.5f;
  } else {
    walkCycle_ = 0.0f;
  }

  MoveWithTownCollision(*entity, velocity_ * deltaTime);
  command_ = {};
}

void CharacterMotion::SaveState(StateWriter &writer) const {
  writer.Value(command_);
  writer.Value(velocity_);
  writer.Value(facing_);
  writer.Value(walkCycle_);
  writer.Value(movementSpeed_);
}

void CharacterMotion::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "CharacterMotion");
  command_ = reader.Value<PlayerCommand>();
  velocity_ = reader.Value<Engine::Vector2>();
  facing_ = reader.Value<Direction>();
  walkCycle_ = reader.Value<float>();
  movementSpeed_ = reader.Value<float>();
}

PokemonRoam::PokemonRoam(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void PokemonRoam::Configure(PokemonSpecies species, Engine::Vector2 home,
                            float radius, std::uint32_t seed) {
  species_ = species;
  home_ = home;
  target_ = home;
  radius_ = radius;
  randomState_ = seed == 0 ? 0x8142u : seed;
  waitTimer_ = 0.2f + RandomUnit() * 1.2f;
  moving_ = false;
}

void PokemonRoam::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  bobCycle_ += deltaTime * (moving_ ? 7.4f : 2.5f);
  if (!moving_) {
    waitTimer_ -= deltaTime;
    if (waitTimer_ <= 0.0f)
      ChooseNewStep();
    return;
  }

  const Engine::Vector2 toTarget{target_.x - entity->transform.position.x,
                                 target_.y - entity->transform.position.y};
  if (DistanceSquared(entity->transform.position, target_) <= 5.0f * 5.0f) {
    moving_ = false;
    waitTimer_ = 0.4f + RandomUnit() * 1.3f;
    return;
  }

  const Engine::Vector2 direction = NormalizeOrZero(toTarget);
  facing_ = DirectionFromVector(direction, facing_);
  MoveWithTownCollision(*entity, direction * (movementSpeed_ * deltaTime));
}

void PokemonRoam::SaveState(StateWriter &writer) const {
  writer.Value(species_);
  writer.Value(home_);
  writer.Value(target_);
  writer.Value(facing_);
  writer.Value(radius_);
  writer.Value(waitTimer_);
  writer.Value(bobCycle_);
  writer.Value(movementSpeed_);
  writer.Value(randomState_);
  writer.Value(moving_);
}

void PokemonRoam::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "PokemonRoam");
  species_ = reader.Value<PokemonSpecies>();
  home_ = reader.Value<Engine::Vector2>();
  target_ = reader.Value<Engine::Vector2>();
  facing_ = reader.Value<Direction>();
  radius_ = reader.Value<float>();
  waitTimer_ = reader.Value<float>();
  bobCycle_ = reader.Value<float>();
  movementSpeed_ = reader.Value<float>();
  randomState_ = reader.Value<std::uint32_t>();
  moving_ = reader.Value<bool>();
}

float PokemonRoam::RandomUnit() {
  randomState_ = randomState_ * 1103515245u + 12345u;
  return static_cast<float>(randomState_ >> 8u) /
         static_cast<float>(0x01000000u);
}

void PokemonRoam::ChooseNewStep() {
  const Direction direction =
      static_cast<Direction>(static_cast<std::uint8_t>(RandomUnit() * 4.0f) % 4u);
  facing_ = direction;
  if (RandomUnit() < 0.25f) {
    moving_ = false;
    waitTimer_ = 0.35f + RandomUnit() * 1.0f;
    return;
  }

  const Engine::Vector2 offset = DirectionVector(direction) * (22.0f + RandomUnit() * 62.0f);
  target_ = {std::clamp(home_.x + offset.x, home_.x - radius_, home_.x + radius_),
             std::clamp(home_.y + offset.y, home_.y - radius_, home_.y + radius_)};
  target_.x = std::clamp(target_.x, TownLeft + CharacterHalfWidth,
                         TownRight - CharacterHalfWidth);
  target_.y = std::clamp(target_.y, TownTop + CharacterHalfHeight,
                         TownBottom - CharacterHalfHeight);
  moving_ = true;
}

NpcWander::NpcWander(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void NpcWander::Configure(NpcRole role, Engine::Vector2 home, float radius,
                          std::uint32_t seed) {
  role_ = role;
  home_ = home;
  target_ = home;
  radius_ = radius;
  randomState_ = seed == 0 ? 0x1234567u : seed;
  waitTimer_ = 0.4f + RandomUnit() * 1.3f;
  moving_ = false;
}

void NpcWander::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  if (!moving_) {
    waitTimer_ -= deltaTime;
    walkCycle_ = 0.0f;
    if (waitTimer_ <= 0.0f)
      ChooseNewStep();
    return;
  }

  const Engine::Vector2 toTarget{target_.x - entity->transform.position.x,
                                 target_.y - entity->transform.position.y};
  if (DistanceSquared(entity->transform.position, target_) <= 6.0f * 6.0f) {
    moving_ = false;
    waitTimer_ = 0.55f + RandomUnit() * 1.6f;
    return;
  }

  const Engine::Vector2 direction = NormalizeOrZero(toTarget);
  facing_ = DirectionFromVector(direction, facing_);
  MoveWithTownCollision(*entity, direction * (movementSpeed_ * deltaTime));
  walkCycle_ += deltaTime * 6.2f;
}

void NpcWander::SaveState(StateWriter &writer) const {
  writer.Value(role_);
  writer.Value(home_);
  writer.Value(target_);
  writer.Value(facing_);
  writer.Value(radius_);
  writer.Value(waitTimer_);
  writer.Value(walkCycle_);
  writer.Value(movementSpeed_);
  writer.Value(randomState_);
  writer.Value(moving_);
}

void NpcWander::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "NpcWander");
  role_ = reader.Value<NpcRole>();
  home_ = reader.Value<Engine::Vector2>();
  target_ = reader.Value<Engine::Vector2>();
  facing_ = reader.Value<Direction>();
  radius_ = reader.Value<float>();
  waitTimer_ = reader.Value<float>();
  walkCycle_ = reader.Value<float>();
  movementSpeed_ = reader.Value<float>();
  randomState_ = reader.Value<std::uint32_t>();
  moving_ = reader.Value<bool>();
}

float NpcWander::RandomUnit() {
  randomState_ = randomState_ * 1664525u + 1013904223u;
  return static_cast<float>(randomState_ >> 8u) /
         static_cast<float>(0x01000000u);
}

void NpcWander::ChooseNewStep() {
  const float roll = RandomUnit();
  if (roll < 0.35f) {
    facing_ = static_cast<Direction>(static_cast<std::uint8_t>(RandomUnit() * 4.0f) % 4u);
    waitTimer_ = 0.5f + RandomUnit() * 1.2f;
    moving_ = false;
    return;
  }

  const Direction direction =
      static_cast<Direction>(static_cast<std::uint8_t>(RandomUnit() * 4.0f) % 4u);
  facing_ = direction;
  const Engine::Vector2 offset = DirectionVector(direction) * (24.0f + RandomUnit() * 46.0f);
  target_ = {std::clamp(home_.x + offset.x, home_.x - radius_, home_.x + radius_),
             std::clamp(home_.y + offset.y, home_.y - radius_, home_.y + radius_)};
  target_.x = std::clamp(target_.x, TownLeft + CharacterHalfWidth,
                         TownRight - CharacterHalfWidth);
  target_.y = std::clamp(target_.y, TownTop + CharacterHalfHeight,
                         TownBottom - CharacterHalfHeight);
  moving_ = true;
}
} // namespace PocketTown

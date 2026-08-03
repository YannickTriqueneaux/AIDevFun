#include "Game/GameplayComponents.h"

#include "Game/GameConfig.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace TownRpg {
namespace {
constexpr float PlayerRadius = 14.0f;
constexpr float NpcRadius = 13.0f;

void RequireVersion(std::uint32_t version, const char *component) {
  if (version != 1)
    throw std::runtime_error(std::string("Unsupported ") + component +
                             " state version.");
}

bool HasMotion(Engine::Vector2 value) {
  return value.x * value.x + value.y * value.y > 0.0001f;
}
} // namespace

PlayerWalk::PlayerWalk(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

bool PlayerWalk::ConsumeFootstep() {
  if (!footstepPending_)
    return false;
  footstepPending_ = false;
  return true;
}

void PlayerWalk::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  walking_ = HasMotion(command_.movement);
  if (walking_) {
    facing_ = command_.movement;
    const float previousCycle = walkCycle_;
    walkCycle_ += deltaTime * 7.5f;
    if (static_cast<int>(previousCycle * 2.0f) !=
        static_cast<int>(walkCycle_ * 2.0f)) {
      footstepPending_ = true;
    }
    entity->transform.position += command_.movement * (movementSpeed_ * deltaTime);
  } else {
    walkCycle_ = 0.0f;
  }

  entity->transform.position.x = std::clamp(
      entity->transform.position.x, PlayerRadius,
      static_cast<float>(GameConfig::PlayAreaWidth) - PlayerRadius);
  entity->transform.position.y = std::clamp(
      entity->transform.position.y, PlayerRadius,
      static_cast<float>(GameConfig::PlayAreaHeight) - PlayerRadius);
  command_.movement = {};
}

void PlayerWalk::SaveState(StateWriter &writer) const {
  writer.Value(facing_);
  writer.Value(command_);
  writer.Value(movementSpeed_);
  writer.Value(walkCycle_);
  writer.Value(walking_);
  writer.Value(footstepPending_);
}

void PlayerWalk::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "PlayerWalk");
  facing_ = reader.Value<Engine::Vector2>();
  command_ = reader.Value<PlayerCommand>();
  movementSpeed_ = reader.Value<float>();
  walkCycle_ = reader.Value<float>();
  walking_ = reader.Value<bool>();
  footstepPending_ = reader.Value<bool>();
}

NpcWander::NpcWander(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {}

void NpcWander::Configure(Engine::Vector2 home, std::uint32_t seed) {
  home_ = home;
  randomState_ = seed == 0U ? 0x1234abcdU : seed;
  decisionTimer_ = 0.2f + RandomUnit() * 1.2f;
}

void NpcWander::Update(float deltaTime) {
  auto *entity = GetOwner().Resolve();
  if (!entity)
    return;

  decisionTimer_ -= deltaTime;
  if (decisionTimer_ <= 0.0f) {
    const float choice = RandomUnit();
    if (choice < 0.45f) {
      direction_ = {};
      decisionTimer_ = 0.7f + RandomUnit() * 1.4f;
    } else {
      const float angle = RandomUnit() * 6.2831853f;
      direction_ = {std::cos(angle), std::sin(angle)};
      decisionTimer_ = 0.8f + RandomUnit() * 1.6f;
    }
  }

  walking_ = HasMotion(direction_);
  if (walking_) {
    facing_ = direction_;
    walkCycle_ += deltaTime * 5.0f;
    entity->transform.position += direction_ * (movementSpeed_ * deltaTime);
    const Engine::Vector2 offset{entity->transform.position.x - home_.x,
                                 entity->transform.position.y - home_.y};
    if (offset.x * offset.x + offset.y * offset.y > 70.0f * 70.0f) {
      const float length = std::sqrt(offset.x * offset.x + offset.y * offset.y);
      direction_ = {-offset.x / length, -offset.y / length};
    }
  } else {
    walkCycle_ = 0.0f;
  }

  entity->transform.position.x = std::clamp(
      entity->transform.position.x, NpcRadius,
      static_cast<float>(GameConfig::PlayAreaWidth) - NpcRadius);
  entity->transform.position.y = std::clamp(
      entity->transform.position.y, NpcRadius,
      static_cast<float>(GameConfig::PlayAreaHeight) - NpcRadius);
}

void NpcWander::SaveState(StateWriter &writer) const {
  writer.Value(home_);
  writer.Value(facing_);
  writer.Value(direction_);
  writer.Value(decisionTimer_);
  writer.Value(movementSpeed_);
  writer.Value(walkCycle_);
  writer.Value(walking_);
  writer.Value(randomState_);
}

void NpcWander::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version, "NpcWander");
  home_ = reader.Value<Engine::Vector2>();
  facing_ = reader.Value<Engine::Vector2>();
  direction_ = reader.Value<Engine::Vector2>();
  decisionTimer_ = reader.Value<float>();
  movementSpeed_ = reader.Value<float>();
  walkCycle_ = reader.Value<float>();
  walking_ = reader.Value<bool>();
  randomState_ = reader.Value<std::uint32_t>();
}

float NpcWander::RandomUnit() {
  randomState_ = randomState_ * 1664525u + 1013904223u;
  return static_cast<float>(randomState_ >> 8u) /
         static_cast<float>(0x01000000u);
}
} // namespace TownRpg

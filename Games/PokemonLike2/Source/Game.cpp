#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Core/Profile.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Serialization/Serializer.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace {
using Entity = Engine::Gameplay::Entity;
using ObjectID = Engine::Gameplay::ObjectID;
using ObjectRef = Engine::Gameplay::ObjectRef<Entity>;
using TypeID = Engine::Gameplay::TypeID;

constexpr float PlayerRadius = 12.0f;
constexpr float EnemyRadius = 17.0f;
constexpr float ProjectileRadius = 4.0f;
constexpr std::size_t MaximumEnemies = 288;
constexpr int PlayerMaxHealth = 20;

constexpr std::array PlayerShotPitch{Engine::AudioEnvelopePoint{0.0f, 1.0f},
                                     Engine::AudioEnvelopePoint{0.14f, 0.24f}};
constexpr std::array PlayerShotVoices{Engine::ProceduralSoundVoice{
    .waveform = Engine::AudioWaveform::Square,
    .frequencyHz = 980.0f,
    .volume = 0.55f,
    .dutyCycle = 0.3f,
    .envelope = {.attackSeconds = 0.002f,
                 .decaySeconds = 0.025f,
                 .sustainLevel = 0.5f,
                 .releaseSeconds = 0.035f},
    .frequencyMultiplier = {.points = PlayerShotPitch},
    .filter = Engine::AudioFilterType::LowPass,
    .filterCutoffHz = 7'000.0f}};
constexpr Engine::ProceduralSoundDefinition PlayerShotPatch{
    .durationSeconds = 0.14f, .voices = PlayerShotVoices};

constexpr std::array EnemyShotPitch{Engine::AudioEnvelopePoint{0.0f, 1.0f},
                                    Engine::AudioEnvelopePoint{0.2f, 0.55f}};
constexpr std::array EnemyShotVoices{Engine::ProceduralSoundVoice{
    .waveform = Engine::AudioWaveform::Saw,
    .frequencyHz = 310.0f,
    .volume = 0.42f,
    .envelope = {.attackSeconds = 0.006f,
                 .decaySeconds = 0.04f,
                 .sustainLevel = 0.45f,
                 .releaseSeconds = 0.06f},
    .frequencyMultiplier = {.points = EnemyShotPitch},
    .filter = Engine::AudioFilterType::LowPass,
    .filterCutoffHz = 3'200.0f}};
constexpr Engine::ProceduralSoundDefinition EnemyShotPatch{
    .durationSeconds = 0.2f, .voices = EnemyShotVoices};

constexpr std::array ImpactVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 120.0f,
                                 .volume = 0.42f,
                                 .envelope = {.attackSeconds = 0.0f,
                                              .decaySeconds = 0.025f,
                                              .sustainLevel = 0.15f,
                                              .releaseSeconds = 0.07f},
                                 .filter = Engine::AudioFilterType::HighPass,
                                 .filterCutoffHz = 650.0f,
                                 .noiseSeed = 0x51f15eU},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 95.0f,
                                 .volume = 0.3f,
                                 .envelope = {.attackSeconds = 0.0f,
                                              .decaySeconds = 0.02f,
                                              .sustainLevel = 0.2f,
                                              .releaseSeconds = 0.08f}}};
constexpr Engine::ProceduralSoundDefinition ImpactPatch{.durationSeconds = 0.1f,
                                                        .voices = ImpactVoices};

float DistanceSquared(Engine::Vector2 left, Engine::Vector2 right) {
  const float x = left.x - right.x;
  const float y = left.y - right.y;
  return x * x + y * y;
}

void DrawGrid(Engine::Renderer2D &renderer) {
  for (int x = 0; x < renderer.GetWidth(); x += GameConfig::GridSize) {
    renderer.DrawLine(
        {static_cast<float>(x), 0.0f},
        {static_cast<float>(x), static_cast<float>(renderer.GetHeight())}, 1.0f,
        GameConfig::GridColor);
  }
  for (int y = 0; y < renderer.GetHeight(); y += GameConfig::GridSize) {
    renderer.DrawLine(
        {0.0f, static_cast<float>(y)},
        {static_cast<float>(renderer.GetWidth()), static_cast<float>(y)}, 1.0f,
        GameConfig::GridColor);
  }
}

int MaxHealthForFaction(BaseGame::Faction faction) {
  return faction == BaseGame::Faction::Player ? PlayerMaxHealth : 3;
}

Engine::Vector2 NormalizeOrZero(Engine::Vector2 value) {
  const float lengthSquared = value.x * value.x + value.y * value.y;
  if (lengthSquared <= 0.0001f)
    return {};
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  return value * inverseLength;
}

Engine::Vector2 MouseAimFromPlayer(Engine::Vector2 playerPosition,
                                   Engine::Vector2 fallback) {
#if defined(_WIN32)
  POINT mouse{};
  if (::GetCursorPos(&mouse)) {
    HWND window = ::GetForegroundWindow();
    if (window && ::ScreenToClient(window, &mouse)) {
      const Engine::Vector2 direction{
          static_cast<float>(mouse.x) - playerPosition.x,
          static_cast<float>(mouse.y) - playerPosition.y};
      const Engine::Vector2 normalized = NormalizeOrZero(direction);
      if (normalized.x != 0.0f || normalized.y != 0.0f)
        return normalized;
    }
  }
#endif
  return fallback;
}

float HealthRatio(const BaseGame::Health *health) {
  if (!health)
    return 0.0f;
  const int maxHealth = MaxHealthForFaction(health->GetFaction());
  if (maxHealth <= 0)
    return 0.0f;
  return std::clamp(static_cast<float>(health->HitPoints()) /
                        static_cast<float>(maxHealth),
                    0.0f, 1.0f);
}

Engine::Color HealthColor(float ratio) {
  ratio = std::clamp(ratio, 0.0f, 1.0f);
  if (ratio > 0.5f) {
    const float t = (ratio - 0.5f) * 2.0f;
    return {static_cast<std::uint8_t>(255.0f * (1.0f - t) + 74.0f * t),
            static_cast<std::uint8_t>(203.0f * (1.0f - t) + 222.0f * t),
            static_cast<std::uint8_t>(0.0f * (1.0f - t) + 92.0f * t), 255};
  }
  const float t = ratio * 2.0f;
  return {255, static_cast<std::uint8_t>(78.0f * (1.0f - t) + 203.0f * t),
          static_cast<std::uint8_t>(92.0f * (1.0f - t)), 255};
}

void DrawHealthBar(Engine::Renderer2D &renderer, Engine::Vector2 center,
                   float width, float height, float ratio) {
  ratio = std::clamp(ratio, 0.0f, 1.0f);
  const Engine::Vector2 start{center.x - width * 0.5f, center.y};
  const Engine::Vector2 end{center.x + width * 0.5f, center.y};
  renderer.DrawLine(start, end, height + 4.0f, {25, 25, 32, 220});
  renderer.DrawLine(start, end, height, {74, 74, 86, 255});
  if (ratio > 0.0f) {
    const Engine::Vector2 fillEnd{start.x + width * ratio, center.y};
    renderer.DrawLine(start, fillEnd, height, HealthColor(ratio));
  }
}
} // namespace

ProceduralGame::ProceduralGame() : world_(*gameInstance_.GetWorld()) {
  RegisterGameplayTypes();
  EnsureCoreEntities();
}

void ProceduralGame::Initialize() {
  static_cast<void>(playerShotSound_.Build(PlayerShotPatch));
  static_cast<void>(enemyShotSound_.Build(EnemyShotPatch));
  static_cast<void>(impactSound_.Build(ImpactPatch));
  static_cast<void>(playerShotSound_.Upload());
  static_cast<void>(enemyShotSound_.Upload());
  static_cast<void>(impactSound_.Upload());
}

void ProceduralGame::Shutdown() {
  impactSound_.Unload();
  enemyShotSound_.Unload();
  playerShotSound_.Unload();
}

void ProceduralGame::RegisterGameplayTypes() {
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::ArenaDirector>(
          "ArenaDirector"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::PlayerMovement>(
          "PlayerMovement"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::EnemyMovement>(
          "EnemyMovement"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::PlayerWeapon>(
          "PlayerWeapon"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::EnemyWeapon>(
          "EnemyWeapon"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::ProjectileMovement>(
          "ProjectileMovement"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::Health>("Health"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::ProjectileDamage>(
          "ProjectileDamage"));

  world_.RegisterEntity({BaseGame::ArenaDirectorEntityType,
                         "ArenaDirector",
                         {BaseGame::ArenaDirector::Type}});
  world_.RegisterEntity(
      {BaseGame::PlayerEntityType,
       "Player",
       {BaseGame::PlayerMovement::Type, BaseGame::PlayerWeapon::Type,
        BaseGame::Health::Type}});
  world_.RegisterEntity(
      {BaseGame::EnemyEntityType,
       "Enemy",
       {BaseGame::EnemyMovement::Type, BaseGame::EnemyWeapon::Type,
        BaseGame::Health::Type}});
  const std::vector<TypeID> projectileComponents{
      BaseGame::ProjectileMovement::Type, BaseGame::ProjectileDamage::Type};
  world_.RegisterEntity({BaseGame::PlayerProjectileEntityType,
                         "PlayerProjectile", projectileComponents});
  world_.RegisterEntity({BaseGame::EnemyProjectileEntityType, "EnemyProjectile",
                         projectileComponents});
}

void ProceduralGame::EnsureCoreEntities() {
  Entity *player = nullptr;
  for (const ObjectID id : world_.Entities()) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    if (entity->GetTypeID() == BaseGame::PlayerEntityType) {
      player = entity;
      playerEntity_ = ObjectRef(id);
    } else if (entity->GetTypeID() == BaseGame::ArenaDirectorEntityType) {
      arenaDirector_ = entity->GetComponent<BaseGame::ArenaDirector>();
    }
  }

  if (!arenaDirector_.Resolve()) {
    const ObjectRef director =
        world_.Spawn(BaseGame::ArenaDirectorEntityType, "ArenaDirector");
    world_.FlushSpawns();
    Entity *entity = director.Resolve();
    arenaDirector_ = entity->GetComponent<BaseGame::ArenaDirector>();
  }

  if (!player) {
    playerEntity_ = world_.Spawn(BaseGame::PlayerEntityType, "Player");
    world_.FlushSpawns();
    player = playerEntity_.Resolve();
    player->transform.position = {
        static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
        static_cast<float>(GameConfig::PlayAreaHeight) * 0.5f};
    player->GetComponent<BaseGame::Health>().Resolve()->Configure(
        BaseGame::Faction::Player, PlayerMaxHealth);
  }

  playerMovement_ = player->GetComponent<BaseGame::PlayerMovement>();
  playerWeapon_ = player->GetComponent<BaseGame::PlayerWeapon>();
  playerHealth_ = player->GetComponent<BaseGame::Health>();
}

void ProceduralGame::Update(const Engine::InputSystem &input, float deltaTime) {
  ENGINE_PROFILE_SCOPE("BaseGame Update");
  const PlayerCommand command = inputBindings_.BuildPlayerCommand(input);
  auto *movement = playerMovement_.Resolve();
  auto *weapon = playerWeapon_.Resolve();
  if (movement) {
    movement->SetCommand(command);
    if (const auto *player = playerEntity_.Resolve()) {
      movement->SetFacing(
          MouseAimFromPlayer(player->transform.position, movement->Facing()));
    }
  }
  if (weapon && movement) {
    const Engine::Vector2 aim = movement->Facing();
    weapon->SetTrigger(command.firing, aim);
  }

  world_.Update(deltaTime);
  ProcessCollisionsAndLifetime();
  ProcessFrameRequests();
}

ObjectRef ProceduralGame::SpawnEnemy(Engine::Vector2 position) {
  const ObjectRef enemy = world_.Spawn(
      BaseGame::EnemyEntityType,
      "Enemy#" + std::to_string(gameInstance_.GetObjectManager()->LiveCount()));
  world_.FlushSpawns();
  auto *entity = enemy.Resolve();
  entity->transform.position = position;
  entity->GetComponent<BaseGame::EnemyMovement>().Resolve()->SetTarget(
      playerEntity_);
  entity->GetComponent<BaseGame::EnemyWeapon>().Resolve()->SetTarget(
      playerEntity_);
  entity->GetComponent<BaseGame::Health>().Resolve()->Configure(
      BaseGame::Faction::Enemy, 3);
  return enemy;
}

ObjectRef ProceduralGame::SpawnProjectile(TypeID type, Engine::Vector2 position,
                                          Engine::Vector2 direction,
                                          BaseGame::Faction faction) {
  const ObjectRef projectile = world_.Spawn(
      type,
      (faction == BaseGame::Faction::Player ? "PlayerShot#" : "EnemyShot#") +
          std::to_string(gameInstance_.GetObjectManager()->LiveCount()));
  world_.FlushSpawns();
  auto *entity = projectile.Resolve();
  entity->transform.position = position;
  const float speed = faction == BaseGame::Faction::Player ? 560.0f : 310.0f;
  entity->GetComponent<BaseGame::ProjectileMovement>().Resolve()->Configure(
      direction * speed, 3.0f);
  entity->GetComponent<BaseGame::ProjectileDamage>().Resolve()->Configure(
      faction, 1);
  return projectile;
}

void ProceduralGame::ProcessFrameRequests() {
  ENGINE_PROFILE_SCOPE("Process Frame Requests");
  std::size_t enemyCount = 0;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (entity && entity->GetTypeID() == BaseGame::EnemyEntityType)
      ++enemyCount;
  }

  Engine::Vector2 spawnPosition;
  if (auto *director = arenaDirector_.Resolve();
      director && enemyCount < MaximumEnemies &&
      director->ConsumeEnemySpawn(spawnPosition)) {
    (void)SpawnEnemy(spawnPosition);
  }

  if (auto *weapon = playerWeapon_.Resolve()) {
    Engine::Vector2 direction;
    if (weapon->ConsumeShot(direction)) {
      const auto *player = playerEntity_.Resolve();
      (void)SpawnProjectile(BaseGame::PlayerProjectileEntityType,
                            player->transform.position + direction * 17.0f,
                            direction, BaseGame::Faction::Player);
      playerShotSound_.Play(0.75f, 0.96f + direction.x * 0.04f, 0.5f);
    }
  }

  const auto entityIDs = world_.Entities();
  for (const ObjectID id : entityIDs) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity || entity->GetTypeID() != BaseGame::EnemyEntityType)
      continue;
    auto *weapon = entity->GetComponent<BaseGame::EnemyWeapon>().Resolve();
    Engine::Vector2 direction;
    if (weapon && weapon->ConsumeShot(direction)) {
      (void)SpawnProjectile(BaseGame::EnemyProjectileEntityType,
                            entity->transform.position + direction * 22.0f,
                            direction, BaseGame::Faction::Enemy);
      enemyShotSound_.Play(0.45f, 0.92f + direction.y * 0.05f, 0.5f);
    }
  }
}

void ProceduralGame::ProcessCollisionsAndLifetime() {
  ENGINE_PROFILE_SCOPE("Collisions And Lifetime");
  struct Target {
    ObjectRef entity;
    BaseGame::Health *health;
    float radius;
  };
  std::vector<Target> targets;
  std::vector<ObjectRef> projectiles;
  for (const ObjectID id : world_.Entities()) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    if (entity->GetTypeID() == BaseGame::PlayerEntityType ||
        entity->GetTypeID() == BaseGame::EnemyEntityType) {
      targets.push_back(
          {ObjectRef(id), entity->GetComponent<BaseGame::Health>().Resolve(),
           entity->GetTypeID() == BaseGame::PlayerEntityType ? PlayerRadius
                                                             : EnemyRadius});
    } else if (entity->GetTypeID() == BaseGame::PlayerProjectileEntityType ||
               entity->GetTypeID() == BaseGame::EnemyProjectileEntityType) {
      projectiles.emplace_back(id);
    }
  }

  for (const ObjectRef projectileRef : projectiles) {
    auto *projectile = projectileRef.Resolve();
    if (!projectile)
      continue;
    auto *movement =
        projectile->GetComponent<BaseGame::ProjectileMovement>().Resolve();
    auto *damage =
        projectile->GetComponent<BaseGame::ProjectileDamage>().Resolve();
    bool destroyProjectile = movement->IsExpired();
    for (Target &target : targets) {
      auto *targetEntity = target.entity.Resolve();
      if (destroyProjectile || !targetEntity || !target.health ||
          target.health->GetFaction() == damage->GetFaction())
        continue;
      const float hitRadius = target.radius + ProjectileRadius;
      if (DistanceSquared(projectile->transform.position,
                          targetEntity->transform.position) <=
          hitRadius * hitRadius) {
        target.health->Damage(damage->DamageAmount());
        impactSound_.Play(0.55f, 0.95f, 0.5f);
        destroyProjectile = true;
      }
    }
    if (destroyProjectile)
      world_.Destroy(projectileRef);
  }

  for (Target &target : targets) {
    if (!target.health || !target.health->IsDead())
      continue;
    auto *entity = target.entity.Resolve();
    if (entity && entity->GetTypeID() == BaseGame::PlayerEntityType) {
      entity->transform.position = {
          static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
          static_cast<float>(GameConfig::PlayAreaHeight) * 0.5f};
      target.health->Configure(BaseGame::Faction::Player, PlayerMaxHealth);
    } else {
      world_.Destroy(target.entity);
    }
  }
  world_.FlushSpawns();
}

Engine::Color ProceduralGame::GetClearColor() const {
  return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext &context) const {
  ENGINE_PROFILE_SCOPE("BaseGame Render");
  Engine::Renderer2D &renderer = context.Draw2D();
  DrawGrid(renderer);

  std::size_t entities = 0;
  std::size_t enemies = 0;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    ++entities;
    const TypeID type = entity->GetTypeID();
    if (type == BaseGame::PlayerEntityType) {
      renderer.DrawCircle(entity->transform.position, PlayerRadius,
                          {74, 222, 156, 255});
      if (const auto *movement =
              entity->GetComponent<BaseGame::PlayerMovement>().Resolve())
        renderer.DrawLine(entity->transform.position,
                          entity->transform.position +
                              movement->Facing() * 22.0f,
                          3.0f, {245, 245, 245, 255});
      DrawHealthBar(
          renderer,
          {entity->transform.position.x,
           entity->transform.position.y - PlayerRadius - 10.0f},
          42.0f, 5.0f,
          HealthRatio(entity->GetComponent<BaseGame::Health>().Resolve()));
    } else if (type == BaseGame::EnemyEntityType) {
      ++enemies;
      const auto *health = entity->GetComponent<BaseGame::Health>().Resolve();
      const float healthRatio = HealthRatio(health);
      renderer.DrawCircle(entity->transform.position, EnemyRadius,
                          HealthColor(healthRatio));
      renderer.DrawCircleOutline(entity->transform.position, EnemyRadius + 2.0f,
                                 healthRatio > 0.5f
                                     ? Engine::Color{190, 255, 140, 255}
                                     : Engine::Color{255, 145, 85, 255});
    } else if (type == BaseGame::PlayerProjectileEntityType) {
      renderer.DrawCircle(entity->transform.position, ProjectileRadius,
                          {255, 223, 92, 255});
    } else if (type == BaseGame::EnemyProjectileEntityType) {
      renderer.DrawCircle(entity->transform.position, ProjectileRadius,
                          {255, 105, 180, 255});
    }
  }

  const auto *health = playerHealth_.Resolve();
  renderer.DrawText("WASD: MOVE   AUTO FIRE   MOUSE: AIM", {24.0f, 22.0f}, 20,
                    {220, 220, 225, 255});
  renderer.DrawText("HP: " + std::to_string(health ? health->HitPoints() : 0) +
                        "   ENEMIES: " + std::to_string(enemies) +
                        "   ENTITIES: " + std::to_string(entities),
                    {24.0f, 50.0f}, 20, {255, 203, 0, 255});
#if !defined(GAME_RELEASE_BUILD)
  renderer.DrawFramesPerSecond(renderer.GetWidth() - 100, 20);
#endif
}

std::vector<std::byte> ProceduralGame::SaveResumeState() const {
  ENGINE_PROFILE_SCOPE("Save Resume State");
  return world_.Save();
}

void ProceduralGame::ResumeFromState(std::span<const std::byte> state) {
  ENGINE_PROFILE_SCOPE("Resume From State");
  world_.Resume(state);
  playerEntity_ = {};
  playerMovement_ = {};
  playerWeapon_ = {};
  playerHealth_ = {};
  arenaDirector_ = {};
  EnsureCoreEntities();
}

#if defined(ENGINE_AUTOTESTS)
void ProceduralGame::SerializeAutoTestState(Engine::Serializer &serializer) {
  const auto *player = playerEntity_.Resolve();
  int enemies = 0;
  int playerProjectiles = 0;
  int enemyProjectiles = 0;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    enemies += entity->GetTypeID() == BaseGame::EnemyEntityType;
    playerProjectiles +=
        entity->GetTypeID() == BaseGame::PlayerProjectileEntityType;
    enemyProjectiles +=
        entity->GetTypeID() == BaseGame::EnemyProjectileEntityType;
  }
  float x = player ? player->transform.position.x : 0.0f;
  float y = player ? player->transform.position.y : 0.0f;
  int entityIndex = static_cast<int>(playerEntity_.GetID().index);
  int entityVersion = static_cast<int>(playerEntity_.GetID().version);
  int objectCount =
      static_cast<int>(gameInstance_.GetObjectManager()->LiveCount());
  int hitPoints =
      playerHealth_.Resolve() ? playerHealth_.Resolve()->HitPoints() : 0;
  serializer.Value("player.position.x", x);
  serializer.Value("player.position.y", y);
  serializer.Value("player.entity.index", entityIndex);
  serializer.Value("player.entity.version", entityVersion);
  serializer.Value("player.hitPoints", hitPoints);
  serializer.Value("world.enemyCount", enemies);
  serializer.Value("world.playerProjectileCount", playerProjectiles);
  serializer.Value("world.enemyProjectileCount", enemyProjectiles);
  serializer.Value("world.objectCount", objectCount);
}
#endif

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
#include <cmath>
#include <string>
#include <string_view>
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

float DistanceSquared(Engine::Vector2 left, Engine::Vector2 right) {
  const float x = left.x - right.x;
  const float y = left.y - right.y;
  return x * x + y * y;
}

void DrawFilledRect(Engine::Renderer2D &renderer, Engine::Vector2 topLeft,
                    Engine::Vector2 size, Engine::Color color) {
  float left = std::max(0.0f, topLeft.x);
  float top = std::max(0.0f, topLeft.y);
  float right =
      std::min(static_cast<float>(renderer.GetWidth()), topLeft.x + size.x);
  float bottom =
      std::min(static_cast<float>(renderer.GetHeight()), topLeft.y + size.y);
  if (right <= left || bottom <= top)
    return;
  const float height = bottom - top;
  renderer.DrawLine({left, top + height * 0.5f}, {right, top + height * 0.5f},
                    height, color);
}

void DrawRectOutline(Engine::Renderer2D &renderer, Engine::Vector2 topLeft,
                     Engine::Vector2 size, float thickness,
                     Engine::Color color) {
  renderer.DrawLine(topLeft, {topLeft.x + size.x, topLeft.y}, thickness, color);
  renderer.DrawLine({topLeft.x, topLeft.y + size.y},
                    {topLeft.x + size.x, topLeft.y + size.y}, thickness, color);
  renderer.DrawLine(topLeft, {topLeft.x, topLeft.y + size.y}, thickness, color);
  renderer.DrawLine({topLeft.x + size.x, topLeft.y},
                    {topLeft.x + size.x, topLeft.y + size.y}, thickness, color);
}

Engine::Vector2 WorldToScreen(Engine::Vector2 world, Engine::Vector2 camera) {
  return {world.x - camera.x, world.y - camera.y};
}

Engine::Vector2 CameraFor(const Engine::Renderer2D &renderer,
                          Engine::Vector2 playerPosition) {
  const float viewWidth = static_cast<float>(renderer.GetWidth());
  const float viewHeight = static_cast<float>(renderer.GetHeight());
  const float maxX =
      std::max(0.0f, static_cast<float>(GameConfig::WorldWidth) - viewWidth);
  const float maxY =
      std::max(0.0f, static_cast<float>(GameConfig::WorldHeight) - viewHeight);
  return {std::clamp(playerPosition.x - viewWidth * 0.5f, 0.0f, maxX),
          std::clamp(playerPosition.y - viewHeight * 0.5f, 0.0f, maxY)};
}

void DrawRetroWorld(Engine::Renderer2D &renderer, Engine::Vector2 camera) {
  const int firstGridX = (static_cast<int>(camera.x) / GameConfig::GridSize) *
                         GameConfig::GridSize;
  const int firstGridY = (static_cast<int>(camera.y) / GameConfig::GridSize) *
                         GameConfig::GridSize;
  for (int x = firstGridX;
       x < camera.x + renderer.GetWidth() + GameConfig::GridSize;
       x += GameConfig::GridSize * 2) {
    const float screenX = static_cast<float>(x) - camera.x;
    renderer.DrawLine({screenX, 0.0f},
                      {screenX, static_cast<float>(renderer.GetHeight())}, 1.0f,
                      {145, 178, 15, 90});
  }
  for (int y = firstGridY;
       y < camera.y + renderer.GetHeight() + GameConfig::GridSize;
       y += GameConfig::GridSize * 2) {
    const float screenY = static_cast<float>(y) - camera.y;
    renderer.DrawLine({0.0f, screenY},
                      {static_cast<float>(renderer.GetWidth()), screenY}, 1.0f,
                      {145, 178, 15, 90});
  }

  DrawFilledRect(renderer, WorldToScreen({0.0f, 620.0f}, camera),
                 {static_cast<float>(GameConfig::WorldWidth), 104.0f},
                 {200, 202, 96, 255});
  DrawFilledRect(renderer, WorldToScreen({540.0f, 0.0f}, camera),
                 {74.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {200, 202, 96, 255});
  DrawFilledRect(renderer, WorldToScreen({1160.0f, 0.0f}, camera),
                 {74.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {200, 202, 96, 255});
  DrawFilledRect(renderer, WorldToScreen({2040.0f, 0.0f}, camera),
                 {74.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {200, 202, 96, 255});
  DrawFilledRect(renderer, WorldToScreen({3180.0f, 0.0f}, camera),
                 {74.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {200, 202, 96, 255});
  DrawFilledRect(renderer, WorldToScreen({0.0f, 40.0f}, camera),
                 {static_cast<float>(GameConfig::WorldWidth), 28.0f},
                 {48, 98, 48, 255});
  DrawFilledRect(renderer, WorldToScreen({0.0f, 1212.0f}, camera),
                 {static_cast<float>(GameConfig::WorldWidth), 28.0f},
                 {48, 98, 48, 255});
  DrawFilledRect(renderer, WorldToScreen({40.0f, 0.0f}, camera),
                 {28.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {48, 98, 48, 255});
  DrawFilledRect(renderer, WorldToScreen({4028.0f, 0.0f}, camera),
                 {28.0f, static_cast<float>(GameConfig::WorldHeight)},
                 {48, 98, 48, 255});

  for (int i = 0; i < 42; ++i) {
    const float x = static_cast<float>((i * 211) % GameConfig::WorldWidth);
    const float y = static_cast<float>((i * 97 + 53) % GameConfig::WorldHeight);
    const Engine::Vector2 p = WorldToScreen({x, y}, camera);
    if (p.x < -32.0f || p.y < -32.0f || p.x > renderer.GetWidth() + 32.0f ||
        p.y > renderer.GetHeight() + 32.0f)
      continue;
    DrawFilledRect(renderer, {p.x - 8.0f, p.y - 14.0f}, {16.0f, 20.0f},
                   {48, 98, 48, 255});
    DrawFilledRect(renderer, {p.x - 14.0f, p.y - 24.0f}, {28.0f, 18.0f},
                   {15, 56, 15, 255});
  }
}

void DrawHouse(Engine::Renderer2D &renderer, Engine::Vector2 camera,
               Engine::Vector2 origin, Engine::Color roofColor) {
  const Engine::Vector2 p = WorldToScreen(origin, camera);
  if (p.x < -140.0f || p.y < -120.0f || p.x > renderer.GetWidth() + 60.0f ||
      p.y > renderer.GetHeight() + 60.0f)
    return;

  DrawFilledRect(renderer, {p.x + 8.0f, p.y + 32.0f}, {104.0f, 76.0f},
                 {224, 218, 146, 255});
  DrawFilledRect(renderer, {p.x, p.y + 24.0f}, {120.0f, 28.0f}, roofColor);
  DrawFilledRect(renderer, {p.x + 14.0f, p.y + 8.0f}, {92.0f, 26.0f},
                 roofColor);
  DrawRectOutline(renderer, {p.x + 8.0f, p.y + 32.0f}, {104.0f, 76.0f}, 3.0f,
                  {48, 56, 36, 255});
  DrawFilledRect(renderer, {p.x + 52.0f, p.y + 68.0f}, {24.0f, 40.0f},
                 {92, 64, 48, 255});
  DrawFilledRect(renderer, {p.x + 22.0f, p.y + 54.0f}, {22.0f, 18.0f},
                 {123, 172, 205, 255});
  DrawFilledRect(renderer, {p.x + 84.0f, p.y + 54.0f}, {18.0f, 18.0f},
                 {123, 172, 205, 255});
}

void DrawSign(Engine::Renderer2D &renderer, Engine::Vector2 camera,
              Engine::Vector2 origin, std::string_view label) {
  const Engine::Vector2 p = WorldToScreen(origin, camera);
  if (p.x < -80.0f || p.y < -60.0f || p.x > renderer.GetWidth() + 80.0f ||
      p.y > renderer.GetHeight() + 60.0f)
    return;
  DrawFilledRect(renderer, {p.x + 18.0f, p.y + 24.0f}, {8.0f, 28.0f},
                 {92, 64, 48, 255});
  DrawFilledRect(renderer, {p.x, p.y}, {64.0f, 26.0f}, {224, 218, 146, 255});
  DrawRectOutline(renderer, {p.x, p.y}, {64.0f, 26.0f}, 3.0f,
                  {48, 56, 36, 255});
  renderer.DrawText(label, {p.x + 6.0f, p.y + 5.0f}, 14, {48, 56, 36, 255});
}

void DrawFlowerPatch(Engine::Renderer2D &renderer, Engine::Vector2 camera,
                     Engine::Vector2 origin) {
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 5; ++x) {
      const Engine::Vector2 p =
          WorldToScreen({origin.x + static_cast<float>(x * 14),
                         origin.y + static_cast<float>(y * 12)},
                        camera);
      if (p.x < -20.0f || p.y < -20.0f || p.x > renderer.GetWidth() + 20.0f ||
          p.y > renderer.GetHeight() + 20.0f)
        continue;
      renderer.DrawCircle(p, 4.0f,
                          ((x + y) & 1) == 0
                              ? Engine::Color{190, 48, 80, 255}
                              : Engine::Color{245, 205, 92, 255});
      renderer.DrawLine({p.x, p.y + 4.0f}, {p.x, p.y + 10.0f}, 2.0f,
                        {48, 98, 48, 255});
    }
  }
}

void DrawWater(Engine::Renderer2D &renderer, Engine::Vector2 camera,
               Engine::Vector2 origin, Engine::Vector2 size) {
  const Engine::Vector2 p = WorldToScreen(origin, camera);
  DrawFilledRect(renderer, p, size, {80, 140, 190, 255});
  DrawRectOutline(renderer, p, size, 4.0f, {48, 56, 110, 255});
  for (int i = 0; i < 8; ++i) {
    const float y = p.y + 18.0f + static_cast<float>(i) * 18.0f;
    renderer.DrawLine({p.x + 18.0f, y}, {p.x + size.x - 18.0f, y}, 2.0f,
                      {123, 172, 205, 255});
  }
}

struct NpcData {
  Engine::Vector2 position;
  Engine::Color clothes;
  const char *dialog;
};

void DrawDialogBubble(Engine::Renderer2D &renderer, Engine::Vector2 anchor,
                      std::string_view text) {
  const Engine::Vector2 bubble{anchor.x - 142.0f, anchor.y - 92.0f};
  DrawFilledRect(renderer, bubble, {284.0f, 54.0f}, {245, 245, 210, 245});
  DrawRectOutline(renderer, bubble, {284.0f, 54.0f}, 3.0f, {48, 56, 36, 255});
  DrawFilledRect(renderer, {anchor.x - 7.0f, anchor.y - 40.0f}, {14.0f, 14.0f},
                 {245, 245, 210, 245});
  renderer.DrawText(text, {bubble.x + 12.0f, bubble.y + 13.0f}, 18,
                    {48, 56, 36, 255});
}

void DrawNpcSprite(Engine::Renderer2D &renderer, Engine::Vector2 center,
                   Engine::Color clothes) {
  renderer.DrawCircle({center.x, center.y + 14.0f}, 12.0f, {48, 56, 36, 80});
  DrawFilledRect(renderer, {center.x - 8.0f, center.y + 7.0f}, {6.0f, 12.0f},
                 {48, 56, 36, 255});
  DrawFilledRect(renderer, {center.x + 2.0f, center.y + 7.0f}, {6.0f, 12.0f},
                 {48, 56, 36, 255});
  DrawFilledRect(renderer, {center.x - 11.0f, center.y - 8.0f}, {22.0f, 22.0f},
                 clothes);
  DrawFilledRect(renderer, {center.x - 10.0f, center.y - 27.0f}, {20.0f, 20.0f},
                 {245, 205, 160, 255});
  DrawFilledRect(renderer, {center.x - 12.0f, center.y - 32.0f}, {24.0f, 8.0f},
                 {92, 64, 48, 255});
  DrawFilledRect(renderer, {center.x - 5.0f, center.y - 20.0f}, {4.0f, 4.0f},
                 {15, 18, 28, 255});
  DrawFilledRect(renderer, {center.x + 3.0f, center.y - 20.0f}, {4.0f, 4.0f},
                 {15, 18, 28, 255});
}

void DrawPlayerSprite(Engine::Renderer2D &renderer, Engine::Vector2 center,
                      Engine::Vector2 facing) {
  const int walkFrame =
      (static_cast<int>(std::floor((center.x + center.y) / 18.0f)) & 1);
  const float legStep = walkFrame == 0 ? -3.0f : 3.0f;

  renderer.DrawCircle({center.x, center.y + 15.0f}, 13.0f, {48, 56, 36, 90});
  DrawFilledRect(renderer, {center.x - 8.0f + legStep, center.y + 8.0f},
                 {6.0f, 12.0f}, {48, 56, 36, 255});
  DrawFilledRect(renderer, {center.x + 2.0f - legStep, center.y + 8.0f},
                 {6.0f, 12.0f}, {48, 56, 36, 255});
  DrawFilledRect(renderer, {center.x - 12.0f, center.y - 8.0f}, {24.0f, 22.0f},
                 {48, 98, 190, 255});
  DrawFilledRect(renderer, {center.x - 10.0f, center.y - 27.0f}, {20.0f, 20.0f},
                 {245, 205, 160, 255});
  DrawFilledRect(renderer, {center.x - 13.0f, center.y - 33.0f}, {26.0f, 10.0f},
                 {190, 48, 48, 255});
  DrawFilledRect(renderer, {center.x - 5.0f, center.y - 39.0f}, {10.0f, 8.0f},
                 {245, 245, 245, 255});

  const float eyeX = facing.x > 0.2f ? 4.0f : (facing.x < -0.2f ? -4.0f : 0.0f);
  const float eyeY = facing.y > 0.2f ? 5.0f : 0.0f;
  DrawFilledRect(renderer, {center.x - 5.0f + eyeX, center.y - 20.0f + eyeY},
                 {4.0f, 4.0f}, {15, 18, 28, 255});
  DrawFilledRect(renderer, {center.x + 3.0f + eyeX, center.y - 20.0f + eyeY},
                 {4.0f, 4.0f}, {15, 18, 28, 255});
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
    if (entity->GetTypeID() == BaseGame::EnemyEntityType ||
        entity->GetTypeID() == BaseGame::PlayerProjectileEntityType ||
        entity->GetTypeID() == BaseGame::EnemyProjectileEntityType) {
      world_.Destroy(ObjectRef(id));
      continue;
    }
    if (entity->GetTypeID() == BaseGame::PlayerEntityType) {
      player = entity;
      playerEntity_ = ObjectRef(id);
    } else if (entity->GetTypeID() == BaseGame::ArenaDirectorEntityType) {
      arenaDirector_ = entity->GetComponent<BaseGame::ArenaDirector>();
    }
  }
  world_.FlushSpawns();

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
        static_cast<float>(GameConfig::WorldWidth) * 0.5f,
        static_cast<float>(GameConfig::WorldHeight) * 0.5f};
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
  // Mini RPG mode: no arena spawns, no weapons, no combat projectiles.
}

void ProceduralGame::ProcessCollisionsAndLifetime() {
  const auto entityIDs = world_.Entities();
  for (const ObjectID id : entityIDs) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    if (entity->GetTypeID() == BaseGame::EnemyEntityType ||
        entity->GetTypeID() == BaseGame::PlayerProjectileEntityType ||
        entity->GetTypeID() == BaseGame::EnemyProjectileEntityType) {
      world_.Destroy(ObjectRef(id));
    }
  }
  world_.FlushSpawns();
}

Engine::Color ProceduralGame::GetClearColor() const {
  return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext &context) const {
  ENGINE_PROFILE_SCOPE("Mini RPG Render");
  Engine::Renderer2D &renderer = context.Draw2D();

  const auto *player = playerEntity_.Resolve();
  const Engine::Vector2 playerPosition =
      player
          ? player->transform.position
          : Engine::Vector2{static_cast<float>(GameConfig::WorldWidth) * 0.5f,
                            static_cast<float>(GameConfig::WorldHeight) * 0.5f};
  const Engine::Vector2 camera = CameraFor(renderer, playerPosition);

  DrawRetroWorld(renderer, camera);
  DrawWater(renderer, camera, {2360.0f, 900.0f}, {380.0f, 170.0f});
  DrawFlowerPatch(renderer, camera, {250.0f, 620.0f});
  DrawFlowerPatch(renderer, camera, {920.0f, 720.0f});
  DrawFlowerPatch(renderer, camera, {1850.0f, 600.0f});
  DrawFlowerPatch(renderer, camera, {3020.0f, 610.0f});
  DrawHouse(renderer, camera, {360.0f, 460.0f}, {146, 60, 48, 255});
  DrawHouse(renderer, camera, {760.0f, 520.0f}, {80, 90, 160, 255});
  DrawHouse(renderer, camera, {1420.0f, 420.0f}, {146, 60, 48, 255});
  DrawHouse(renderer, camera, {1740.0f, 760.0f}, {80, 90, 160, 255});
  DrawHouse(renderer, camera, {2150.0f, 560.0f}, {146, 60, 48, 255});
  DrawHouse(renderer, camera, {2760.0f, 430.0f}, {80, 90, 160, 255});
  DrawHouse(renderer, camera, {3200.0f, 700.0f}, {146, 60, 48, 255});
  DrawHouse(renderer, camera, {3660.0f, 500.0f}, {80, 90, 160, 255});
  DrawSign(renderer, camera, {1980.0f, 725.0f}, "TOWN");

  const NpcData npcs[]{
      {{1960.0f, 655.0f}, {190, 48, 80, 255}, "Salut! Bienvenue au village."},
      {{1510.0f, 575.0f},
       {80, 90, 160, 255},
       "Les maisons bloquent le chemin."},
      {{2360.0f, 790.0f}, {48, 98, 48, 255}, "Le lac est calme aujourd'hui."},
      {{3140.0f, 775.0f}, {245, 205, 92, 255}, "Continue vers l'est!"},
  };

  const NpcData *nearbyNpc = nullptr;
  for (const NpcData &npc : npcs) {
    const Engine::Vector2 screen = WorldToScreen(npc.position, camera);
    if (screen.x >= -48.0f && screen.y >= -80.0f &&
        screen.x <= renderer.GetWidth() + 48.0f &&
        screen.y <= renderer.GetHeight() + 48.0f) {
      DrawNpcSprite(renderer, screen, npc.clothes);
    }
    if (!nearbyNpc &&
        DistanceSquared(playerPosition, npc.position) <= 72.0f * 72.0f)
      nearbyNpc = &npc;
  }

  if (nearbyNpc) {
    DrawDialogBubble(renderer, WorldToScreen(nearbyNpc->position, camera),
                     nearbyNpc->dialog);
  }

  if (player) {
    Engine::Vector2 facing{0.0f, 1.0f};
    if (const auto *movement =
            player->GetComponent<BaseGame::PlayerMovement>().Resolve())
      facing = movement->Facing();
    DrawPlayerSprite(renderer, WorldToScreen(playerPosition, camera), facing);
  }

  std::size_t entityCount = 0;
  for (const ObjectID id : world_.Entities()) {
    if (ObjectRef(id).Resolve())
      ++entityCount;
  }

  DrawFilledRect(renderer, {14.0f, 14.0f}, {500.0f, 78.0f}, {15, 56, 15, 210});
  renderer.DrawText("MINI RPG RETRO v2 - WASD / FLECHES POUR MARCHER",
                    {24.0f, 24.0f}, 20, {235, 245, 210, 255});
  renderer.DrawText(
      "Grande carte side-scroll, maisons solides, routes et village",
      {24.0f, 48.0f}, 18, {200, 220, 160, 255});
  renderer.DrawText("Entities: " + std::to_string(entityCount) + "   PNJ: 4",
                    {24.0f, 70.0f}, 18, {245, 245, 210, 255});
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

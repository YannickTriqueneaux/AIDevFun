#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Serialization/Serializer.h"

namespace {
constexpr auto PlayerEntityType =
    Engine::Gameplay::StableTypeID("BaseGame.PlayerEntity");
}

ProceduralGame::ProceduralGame() {
  world_.RegisterComponent(
      {Player::Type, "Player",
       [](Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> owner) {
         return NEW_OBJECT(Player, owner);
       }});
  world_.RegisterEntity({PlayerEntityType, "PlayerEntity", {Player::Type}});
  playerEntity_ = world_.Spawn(PlayerEntityType, "Player");
  world_.FlushSpawns();
  auto *entity = playerEntity_.Resolve(world_.Objects());
  entity->transform.position = {
      static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
      static_cast<float>(GameConfig::PlayAreaHeight) * 0.5f};
  player_ = Engine::Gameplay::ObjectRef<Player>(entity->components.front());
}

void ProceduralGame::Update(const Engine::InputSystem &input, float deltaTime) {
  if (auto *player = player_.Resolve(world_.Objects()))
    player->SetCommand(inputBindings_.BuildPlayerCommand(input));
  world_.Update(deltaTime);
}

Engine::Color ProceduralGame::GetClearColor() const {
  return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext &context) const {
  Engine::Renderer2D &renderer = context.Draw2D();
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

  if (const auto *player = player_.Resolve(world_.Objects())) {
    player->Render(renderer, world_.Objects());
    player->RenderHud(renderer);
  }
#if !defined(GAME_RELEASE_BUILD)
  renderer.DrawFramesPerSecond(renderer.GetWidth() - 100, 20);
#endif
}

std::vector<std::byte> ProceduralGame::SaveResumeState() const {
  return world_.Save();
}
void ProceduralGame::ResumeFromState(std::span<const std::byte> state) {
  world_.Resume(state);
  Engine::Gameplay::Entity *playerEntity = nullptr;
  for (const auto id : world_.Entities()) {
    auto *candidate = world_.Objects().GetAs<Engine::Gameplay::Entity>(id);
    if (candidate != nullptr && candidate->GetTypeID() == PlayerEntityType) {
      playerEntity = candidate;
      playerEntity_ = Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity>(id);
      break;
    }
  }
  if (playerEntity == nullptr) {
    playerEntity_ = world_.Spawn(PlayerEntityType, "Player");
    world_.FlushSpawns();
    playerEntity = playerEntity_.Resolve(world_.Objects());
    playerEntity->transform.position = {
        static_cast<float>(GameConfig::PlayAreaWidth) * 0.5f,
        static_cast<float>(GameConfig::PlayAreaHeight) * 0.5f};
  }
  player_ =
      Engine::Gameplay::ObjectRef<Player>(playerEntity->components.front());
}

#if defined(ENGINE_AUTOTESTS)
void ProceduralGame::SerializeAutoTestState(Engine::Serializer &serializer) {
  const auto *entity = playerEntity_.Resolve(world_.Objects());
  const auto *player = player_.Resolve(world_.Objects());
  float x = entity ? entity->transform.position.x : 0.0f;
  float y = entity ? entity->transform.position.y : 0.0f;
  int entityIndex = static_cast<int>(playerEntity_.GetID().index);
  int entityVersion = static_cast<int>(playerEntity_.GetID().version);
  int objectCount = static_cast<int>(world_.Objects().LiveCount());
  serializer.Value("player.position.x", x);
  serializer.Value("player.position.y", y);
  serializer.Value("player.entity.index", entityIndex);
  serializer.Value("player.entity.version", entityVersion);
  serializer.Value("world.objectCount", objectCount);
  (void)player;
}
#endif

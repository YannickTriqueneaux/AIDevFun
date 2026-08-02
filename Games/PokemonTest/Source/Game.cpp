#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Core/Profile.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Graphics/VectorShape.h"
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
#include <memory>
#include <stdexcept>
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
constexpr Engine::Color White{255, 255, 255, 255};

static constexpr std::string_view GroundSvg = R"svg(
<svg viewBox="0 0 100 100">
  <g id="base"><rect x="0" y="0" width="100" height="100" fill="#74CFA0"/><rect x="0" y="0" width="100" height="100" fill="#8BDBA8" opacity="0.38"/></g>
  <g id="moss"><rect x="12" y="18" width="3" height="7" fill="#4EB87D" opacity="0.60"/><rect x="16" y="21" width="3" height="5" fill="#A9E8B8" opacity="0.55"/><rect x="42" y="72" width="4" height="8" fill="#4EB87D" opacity="0.48"/><rect x="78" y="38" width="3" height="7" fill="#A9E8B8" opacity="0.50"/><rect x="66" y="83" width="6" height="3" fill="#5FC98C" opacity="0.45"/></g>
  <g id="veins"><line x1="8" y1="52" x2="22" y2="52" stroke="#5FC98C" stroke-width="1" opacity="0.22"/><line x1="54" y1="18" x2="66" y2="18" stroke="#BDEFC6" stroke-width="1" opacity="0.18"/><line x1="30" y1="38" x2="40" y2="38" stroke="#5FC98C" stroke-width="1" opacity="0.18"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> GroundGroups{"base", "moss", "veins"};

static constexpr std::string_view RoadSvg = R"svg(
<svg viewBox="0 0 100 24">
  <g id="pavement"><rect x="0" y="0" width="100" height="24" fill="#8B6F3E"/><rect x="0" y="3" width="100" height="18" fill="#C8A85D"/><rect x="0" y="11" width="100" height="2" fill="#9A7C45" opacity="0.70"/></g>
  <g id="glow"><line x1="0" y1="3" x2="100" y2="3" stroke="#F1D88A" stroke-width="2" opacity="0.58"/><line x1="0" y1="21" x2="100" y2="21" stroke="#5C4A32" stroke-width="2" opacity="0.72"/></g>
  <g id="dash"><line x1="16" y1="4" x2="16" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="36" y1="4" x2="36" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="56" y1="4" x2="56" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="76" y1="4" x2="76" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><rect x="8" y="6" width="2" height="2" fill="#FFE9A5"/><rect x="27" y="17" width="2" height="2" fill="#FFE9A5"/><rect x="69" y="7" width="2" height="2" fill="#FFE9A5"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> RoadGroups{"pavement", "glow", "dash"};

static constexpr std::string_view HouseSvg = R"svg(
<svg viewBox="0 0 120 120">
  <g id="shadow"><ellipse cx="60" cy="108" rx="50" ry="9" fill="#050816" opacity="0.35"/></g>
  <g id="body"><rect x="14" y="44" width="92" height="64" fill="#F8F1C9"/><rect x="20" y="50" width="80" height="52" fill="#DDBB86" opacity="0.35"/></g>
  <g id="roof"><polygon points="8,48 60,10 112,48" fill="#FF4FD8"/><rect x="20" y="36" width="80" height="18" fill="#8A5CFF"/></g>
  <g id="windows"><rect x="27" y="60" width="19" height="18" fill="#00E5FF"/><rect x="74" y="60" width="19" height="18" fill="#00E5FF"/><line x1="36" y1="60" x2="36" y2="78" stroke="#F8F871" stroke-width="2"/><line x1="83" y1="60" x2="83" y2="78" stroke="#F8F871" stroke-width="2"/></g>
  <g id="door"><rect x="51" y="74" width="20" height="34" fill="#51344D"/><circle cx="66" cy="91" r="2" fill="#F8F871"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 5> HouseGroups{"shadow", "body", "roof", "windows", "door"};

static constexpr std::string_view SignSvg = R"svg(
<svg viewBox="0 0 80 60">
  <g id="post"><rect x="37" y="24" width="6" height="34" fill="#51344D"/></g>
  <g id="board"><rect x="6" y="4" width="68" height="28" fill="#F8F1C9"/><line x1="6" y1="4" x2="74" y2="4" stroke="#00E5FF" stroke-width="3"/><line x1="6" y1="32" x2="74" y2="32" stroke="#FF4FD8" stroke-width="3"/></g>
  <g id="spark"><circle cx="67" cy="8" r="3" fill="#F8F871"/><circle cx="13" cy="28" r="2" fill="#F8F871"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> SignGroups{"post", "board", "spark"};

static constexpr std::string_view FlowerSvg = R"svg(
<svg viewBox="0 0 32 40">
  <g id="stem"><line x1="16" y1="18" x2="16" y2="38" stroke="#2DD36F" stroke-width="3"/><ellipse cx="10" cy="29" rx="6" ry="3" fill="#39FFB6"/><ellipse cx="22" cy="32" rx="6" ry="3" fill="#39FFB6"/></g>
  <g id="petals"><circle cx="16" cy="10" r="5" fill="#F8F871"/><circle cx="8" cy="14" r="5" fill="#FF4FD8"/><circle cx="24" cy="14" r="5" fill="#8A5CFF"/><circle cx="16" cy="20" r="5" fill="#00E5FF"/></g>
  <g id="heart"><circle cx="16" cy="15" r="4" fill="#F8F1C9"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> FlowerGroups{"stem", "petals", "heart"};

static constexpr std::string_view WaterSvg = R"svg(
<svg viewBox="0 0 100 60">
  <g id="pool"><rect x="0" y="0" width="100" height="60" fill="#3F79D1"/><rect x="0" y="0" width="100" height="60" fill="#5B98E1" opacity="0.35"/></g>
  <g id="waves"><polyline points="2,12 10,7 18,12 26,7 34,12 42,7 50,12 58,7 66,12 74,7 82,12 90,7 98,12" stroke="#73B7E8" stroke-width="2" fill="none" opacity="0.65"/><polyline points="0,30 8,25 16,30 24,25 32,30 40,25 48,30 56,25 64,30 72,25 80,30 88,25 96,30" stroke="#2F6FC5" stroke-width="2" fill="none" opacity="0.38"/><polyline points="2,48 10,43 18,48 26,43 34,48 42,43 50,48 58,43 66,48 74,43 82,48 90,43 98,48" stroke="#73B7E8" stroke-width="2" fill="none" opacity="0.55"/></g>
  <g id="shine"><rect x="74" y="10" width="4" height="4" fill="#A8DFFF" opacity="0.75"/><rect x="30" y="42" width="3" height="3" fill="#A8DFFF" opacity="0.55"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> WaterGroups{"pool", "waves", "shine"};

static constexpr std::string_view TreeSvg = R"svg(
<svg viewBox="0 0 56 76">
  <g id="shadow"><ellipse cx="28" cy="70" rx="17" ry="5" fill="#31583D" opacity="0.30"/></g>
  <g id="trunk"><rect x="22" y="45" width="12" height="21" fill="#5F4631"/><rect x="25" y="45" width="6" height="21" fill="#8B6A42"/></g>
  <g id="crown"><circle cx="28" cy="21" r="20" fill="#2F7F48"/><circle cx="17" cy="33" r="15" fill="#3E9959"/><circle cx="39" cy="33" r="15" fill="#2D7442"/><circle cx="28" cy="39" r="16" fill="#34894E"/><rect x="17" y="12" width="8" height="5" fill="#BDEFC6" opacity="0.70"/><rect x="8" y="30" width="6" height="4" fill="#8EDC95" opacity="0.62"/><rect x="34" y="22" width="8" height="5" fill="#8EDC95" opacity="0.55"/></g>
  <g id="fruit"><rect x="18" y="24" width="4" height="4" fill="#D94F45"/><rect x="38" y="19" width="4" height="4" fill="#F4D35E"/><rect x="30" y="42" width="4" height="4" fill="#D94F45"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 4> TreeGroups{"shadow", "trunk", "crown", "fruit"};

static constexpr std::string_view PlayerSvg = R"svg(
<svg viewBox="0 0 64 80">
  <g id="shadow"><ellipse cx="32" cy="73" rx="15" ry="5" fill="#31583D" opacity="0.32"/></g>
  <g id="left_leg">
    <rect x="24" y="55" width="7" height="13" fill="#26334F"/>
    <rect x="22" y="67" width="10" height="4" fill="#2D2D34"/>
  </g>
  <g id="right_leg">
    <rect x="34" y="55" width="7" height="13" fill="#26334F"/>
    <rect x="34" y="67" width="10" height="4" fill="#2D2D34"/>
  </g>
  <g id="body">
    <rect x="20" y="36" width="24" height="20" fill="#2D2D34"/>
    <rect x="22" y="38" width="20" height="16" fill="#5C6FCB"/>
    <rect x="24" y="38" width="16" height="6" fill="#F4F1E8"/>
    <rect x="15" y="39" width="7" height="16" fill="#2D2D34"/>
    <rect x="16" y="41" width="4" height="12" fill="#FFD2A1"/>
    <rect x="42" y="39" width="7" height="16" fill="#2D2D34"/>
    <rect x="44" y="41" width="4" height="12" fill="#FFD2A1"/>
    <rect x="18" y="34" width="7" height="18" fill="#8B4E8E"/>
  </g>
  <g id="head">
    <rect x="22" y="18" width="20" height="18" fill="#2D2D34"/>
    <rect x="24" y="20" width="16" height="14" fill="#FFD2A1"/>
    <rect x="27" y="33" width="10" height="3" fill="#E8A06D"/>
  </g>
  <g id="hat">
    <rect x="20" y="12" width="24" height="8" fill="#C63D32"/>
    <rect x="24" y="8" width="14" height="5" fill="#E65645"/>
    <rect x="38" y="15" width="7" height="4" fill="#F4F1E8"/>
    <rect x="18" y="19" width="14" height="4" fill="#C63D32"/>
  </g>
  <g id="eyes">
    <rect x="27" y="25" width="3" height="3" fill="#2D2D34"/>
    <rect x="35" y="25" width="3" height="3" fill="#2D2D34"/>
    <rect x="30" y="31" width="5" height="2" fill="#B54A42"/>
  </g>
</svg>
)svg";
static constexpr std::array<std::string_view, 7> PlayerGroups{"shadow", "left_leg", "right_leg", "body", "head", "hat", "eyes"};

enum class PlayerGroup : std::size_t { Shadow, LeftLeg, RightLeg, Body, Head, Hat, Eyes, Count };

static constexpr std::string_view NpcSvg = R"svg(
<svg viewBox="0 0 64 80">
  <g id="shadow"><ellipse cx="32" cy="73" rx="16" ry="5" fill="#050816" opacity="0.26"/></g>
  <g id="legs">
    <rect x="23" y="55" width="7" height="13" fill="#050816"/>
    <rect x="24" y="55" width="5" height="11" fill="#FFD2A1"/>
    <rect x="34" y="55" width="7" height="13" fill="#050816"/>
    <rect x="35" y="55" width="5" height="11" fill="#FFD2A1"/>
    <rect x="20" y="67" width="11" height="4" fill="#050816"/>
    <rect x="33" y="67" width="11" height="4" fill="#050816"/>
  </g>
  <g id="body">
    <rect x="18" y="33" width="28" height="24" fill="#050816"/>
    <rect x="20" y="35" width="24" height="20" fill="#FF6BCB"/>
    <rect x="24" y="39" width="16" height="4" fill="#F8F8F0"/>
    <rect x="15" y="36" width="6" height="17" fill="#050816"/>
    <rect x="16" y="38" width="4" height="13" fill="#FFD2A1"/>
    <rect x="43" y="36" width="6" height="17" fill="#050816"/>
    <rect x="44" y="38" width="4" height="13" fill="#FFD2A1"/>
  </g>
  <g id="head">
    <rect x="21" y="13" width="22" height="21" fill="#050816"/>
    <rect x="24" y="17" width="16" height="15" fill="#FFD2A1"/>
    <rect x="27" y="31" width="10" height="3" fill="#E8A06D"/>
  </g>
  <g id="hair">
    <rect x="19" y="10" width="24" height="9" fill="#050816"/>
    <rect x="17" y="18" width="8" height="12" fill="#050816"/>
    <rect x="39" y="18" width="8" height="12" fill="#050816"/>
    <rect x="28" y="8" width="8" height="4" fill="#050816"/>
  </g>
  <g id="eyes">
    <rect x="27" y="22" width="3" height="3" fill="#050816"/>
    <rect x="35" y="22" width="3" height="3" fill="#050816"/>
    <rect x="30" y="28" width="5" height="2" fill="#D94F45"/>
  </g>
</svg>
)svg";
static constexpr std::array<std::string_view, 6> NpcGroups{"shadow", "legs", "body", "head", "hair", "eyes"};

enum class NpcGroup : std::size_t { Shadow, Legs, Body, Head, Hair, Eyes, Count };

template <std::size_t Count>
std::array<Engine::VectorShapeGroupID, Count>
LoadShape(Engine::VectorShape &shape, std::string_view svg,
          const std::array<std::string_view, Count> &groups) {
  if (!shape.LoadFromSvg(svg) || !shape.Upload())
    throw std::runtime_error(shape.GetLastError());
  auto ids = shape.ResolveGroups(groups);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (!ids[index].IsValid())
      throw std::runtime_error("Missing SVG group: " + std::string(groups[index]));
  }
  return ids;
}

} // namespace

struct ArtResources {
  Engine::VectorShape ground;
  Engine::VectorShape road;
  Engine::VectorShape house;
  Engine::VectorShape sign;
  Engine::VectorShape flower;
  Engine::VectorShape water;
  Engine::VectorShape tree;
  Engine::VectorShape player;
  Engine::VectorShape npc;

  std::array<Engine::VectorShapeGroupID, GroundGroups.size()> groundGroups{};
  std::array<Engine::VectorShapeGroupID, RoadGroups.size()> roadGroups{};
  std::array<Engine::VectorShapeGroupID, HouseGroups.size()> houseGroups{};
  std::array<Engine::VectorShapeGroupID, SignGroups.size()> signGroups{};
  std::array<Engine::VectorShapeGroupID, FlowerGroups.size()> flowerGroups{};
  std::array<Engine::VectorShapeGroupID, WaterGroups.size()> waterGroups{};
  std::array<Engine::VectorShapeGroupID, TreeGroups.size()> treeGroups{};
  std::array<Engine::VectorShapeGroupID, PlayerGroups.size()> playerGroups{};
  std::array<Engine::VectorShapeGroupID, NpcGroups.size()> npcGroups{};

  std::unique_ptr<Engine::VectorShapePose> roadPose;
  std::unique_ptr<Engine::VectorShapePose> housePose;
  std::unique_ptr<Engine::VectorShapePose> signPose;
  std::unique_ptr<Engine::VectorShapePose> flowerPose;
  std::unique_ptr<Engine::VectorShapePose> waterPose;
  std::unique_ptr<Engine::VectorShapePose> treePose;
  std::unique_ptr<Engine::VectorShapePose> playerPose;
  std::unique_ptr<Engine::VectorShapePose> npcPose;
  bool ready = false;

  void Initialize() {
    Shutdown();
    groundGroups = LoadShape(ground, GroundSvg, GroundGroups);
    roadGroups = LoadShape(road, RoadSvg, RoadGroups);
    houseGroups = LoadShape(house, HouseSvg, HouseGroups);
    signGroups = LoadShape(sign, SignSvg, SignGroups);
    flowerGroups = LoadShape(flower, FlowerSvg, FlowerGroups);
    waterGroups = LoadShape(water, WaterSvg, WaterGroups);
    treeGroups = LoadShape(tree, TreeSvg, TreeGroups);
    playerGroups = LoadShape(player, PlayerSvg, PlayerGroups);
    npcGroups = LoadShape(npc, NpcSvg, NpcGroups);
    roadPose = std::make_unique<Engine::VectorShapePose>(road);
    housePose = std::make_unique<Engine::VectorShapePose>(house);
    signPose = std::make_unique<Engine::VectorShapePose>(sign);
    flowerPose = std::make_unique<Engine::VectorShapePose>(flower);
    waterPose = std::make_unique<Engine::VectorShapePose>(water);
    treePose = std::make_unique<Engine::VectorShapePose>(tree);
    playerPose = std::make_unique<Engine::VectorShapePose>(player);
    npcPose = std::make_unique<Engine::VectorShapePose>(npc);
    ready = true;
  }

  void Shutdown() {
    ready = false;
    npcPose.reset();
    playerPose.reset();
    treePose.reset();
    waterPose.reset();
    flowerPose.reset();
    signPose.reset();
    housePose.reset();
    roadPose.reset();
    npc.Unload();
    player.Unload();
    tree.Unload();
    water.Unload();
    flower.Unload();
    sign.Unload();
    house.Unload();
    road.Unload();
    ground.Unload();
  }
};

namespace {
void DrawVectorArt(Engine::Renderer2D &renderer, const Engine::VectorShape &shape,
                   Engine::Vector2 center, Engine::Vector2 size,
                   const Engine::VectorShapePose *pose = nullptr,
                   float rotationDegrees = 0.0f,
                   Engine::Color tint = White) {
  if (center.x < -size.x || center.y < -size.y ||
      center.x > static_cast<float>(renderer.GetWidth()) + size.x ||
      center.y > static_cast<float>(renderer.GetHeight()) + size.y)
    return;
  Engine::VectorShapeDrawParameters draw;
  draw.position = center;
  draw.size = size;
  draw.rotationDegrees = rotationDegrees;
  draw.tint = tint;
  draw.pose = pose;
  renderer.DrawVectorShape(shape, draw);
}

Engine::VectorShapeGroupID PlayerGroupId(const ArtResources &art,
                                         PlayerGroup group) {
  return art.playerGroups[static_cast<std::size_t>(group)];
}

Engine::VectorShapeGroupID NpcGroupId(const ArtResources &art, NpcGroup group) {
  return art.npcGroups[static_cast<std::size_t>(group)];
}

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

ProceduralGame::ProceduralGame()
    : world_(*gameInstance_.GetWorld()), art_(std::make_unique<ArtResources>()) {
  RegisterGameplayTypes();
  EnsureCoreEntities();
}

ProceduralGame::~ProceduralGame() = default;

void ProceduralGame::Initialize() {
  if (art_)
    art_->Initialize();
}

void ProceduralGame::Shutdown() {
  if (art_)
    art_->Shutdown();
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
  visualTime_ += deltaTime;
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
  if (!art_ || !art_->ready) {
    renderer.DrawText("Chargement des SVG...", {24.0f, 24.0f}, 22,
                      {235, 245, 210, 255});
    return;
  }
  const ArtResources &art = *art_;

  const auto *player = playerEntity_.Resolve();
  const Engine::Vector2 playerPosition =
      player
          ? player->transform.position
          : Engine::Vector2{static_cast<float>(GameConfig::WorldWidth) * 0.5f,
                            static_cast<float>(GameConfig::WorldHeight) * 0.5f};
  const Engine::Vector2 camera = CameraFor(renderer, playerPosition);

  constexpr float GroundTileSize = 320.0f;
  const float firstGroundX =
      std::floor(camera.x / GroundTileSize) * GroundTileSize;
  const float firstGroundY =
      std::floor(camera.y / GroundTileSize) * GroundTileSize;
  for (float y = firstGroundY;
       y < camera.y + static_cast<float>(renderer.GetHeight()) + GroundTileSize;
       y += GroundTileSize) {
    for (float x = firstGroundX;
         x < camera.x + static_cast<float>(renderer.GetWidth()) + GroundTileSize;
         x += GroundTileSize) {
      DrawVectorArt(renderer, art.ground,
                    WorldToScreen({x + GroundTileSize * 0.5f,
                                   y + GroundTileSize * 0.5f},
                                  camera),
                    {GroundTileSize, GroundTileSize});
    }
  }

  auto drawRoad = [&](Engine::Vector2 origin, Engine::Vector2 size,
                      bool vertical) {
    constexpr float RoadTileLength = 256.0f;
    const float length = vertical ? size.y : size.x;
    const float width = vertical ? size.x : size.y;
    for (float offset = 0.0f; offset < length; offset += RoadTileLength) {
      const float segmentLength = std::min(RoadTileLength, length - offset);
      const Engine::Vector2 worldCenter =
          vertical ? Engine::Vector2{origin.x + width * 0.5f,
                                     origin.y + offset + segmentLength * 0.5f}
                   : Engine::Vector2{origin.x + offset + segmentLength * 0.5f,
                                     origin.y + width * 0.5f};
      DrawVectorArt(renderer, art.road, WorldToScreen(worldCenter, camera),
                    vertical ? Engine::Vector2{segmentLength, width}
                             : Engine::Vector2{segmentLength, width},
                    nullptr, vertical ? 90.0f : 0.0f);
    }
  };
  drawRoad({0.0f, 620.0f}, {static_cast<float>(GameConfig::WorldWidth), 104.0f},
           false);
  drawRoad({540.0f, 0.0f}, {74.0f, static_cast<float>(GameConfig::WorldHeight)},
           true);
  drawRoad({1160.0f, 0.0f}, {74.0f, static_cast<float>(GameConfig::WorldHeight)},
           true);
  drawRoad({2040.0f, 0.0f}, {74.0f, static_cast<float>(GameConfig::WorldHeight)},
           true);
  drawRoad({3180.0f, 0.0f}, {74.0f, static_cast<float>(GameConfig::WorldHeight)},
           true);

  auto drawHouse = [&](Engine::Vector2 origin, float hueShift) {
    art.housePose->Reset();
    Engine::VectorShapeTransform windows;
    windows.opacity = 0.68f + 0.32f * (0.5f + 0.5f * std::sin(visualTime_ * 3.0f + hueShift));
    art.housePose->SetGroupTransform(art.houseGroups[3], windows);
    DrawVectorArt(renderer, art.house,
                  WorldToScreen({origin.x + 60.0f, origin.y + 60.0f}, camera),
                  {128.0f, 128.0f}, art.housePose.get());
  };
  auto drawTree = [&](Engine::Vector2 origin, float phase) {
    art.treePose->Reset();
    Engine::VectorShapeTransform crown;
    crown.rotationDegrees = std::sin(visualTime_ * 1.7f + phase) * 4.0f;
    crown.translation = {std::sin(visualTime_ * 1.3f + phase) * 1.6f, 0.0f};
    art.treePose->SetGroupTransform(art.treeGroups[2], crown);
    DrawVectorArt(renderer, art.tree, WorldToScreen(origin, camera), {56.0f, 76.0f},
                  art.treePose.get());
  };
  auto drawFlowerPatch = [&](Engine::Vector2 origin) {
    for (int y = 0; y < 3; ++y) {
      for (int x = 0; x < 5; ++x) {
        art.flowerPose->Reset();
        Engine::VectorShapeTransform petals;
        const float phase = static_cast<float>(x * 3 + y) * 0.47f;
        const float pulse = 1.0f + 0.12f * std::sin(visualTime_ * 4.0f + phase);
        petals.scale = {pulse, pulse};
        petals.rotationDegrees = std::sin(visualTime_ * 2.0f + phase) * 9.0f;
        art.flowerPose->SetGroupTransform(art.flowerGroups[1], petals);
        DrawVectorArt(renderer, art.flower,
                      WorldToScreen({origin.x + static_cast<float>(x * 14),
                                     origin.y + static_cast<float>(y * 12)},
                                    camera),
                      {28.0f, 34.0f}, art.flowerPose.get());
      }
    }
  };
  auto drawWater = [&](Engine::Vector2 origin, Engine::Vector2 size) {
    art.waterPose->Reset();
    Engine::VectorShapeTransform waves;
    waves.translation = {std::sin(visualTime_ * 2.7f) * 4.0f, 0.0f};
    art.waterPose->SetGroupTransform(art.waterGroups[1], waves);
    DrawVectorArt(renderer, art.water,
                  WorldToScreen({origin.x + size.x * 0.5f,
                                 origin.y + size.y * 0.5f},
                                camera),
                  size, art.waterPose.get());
  };

  for (int i = 0; i < 42; ++i) {
    const float x = static_cast<float>((i * 211) % GameConfig::WorldWidth);
    const float y = static_cast<float>((i * 97 + 53) % GameConfig::WorldHeight);
    drawTree({x, y}, static_cast<float>(i) * 0.33f);
  }

  drawWater({2360.0f, 900.0f}, {380.0f, 170.0f});
  drawFlowerPatch({250.0f, 620.0f});
  drawFlowerPatch({920.0f, 720.0f});
  drawFlowerPatch({1850.0f, 600.0f});
  drawFlowerPatch({3020.0f, 610.0f});
  drawHouse({360.0f, 460.0f}, 0.0f);
  drawHouse({760.0f, 520.0f}, 1.0f);
  drawHouse({1420.0f, 420.0f}, 2.0f);
  drawHouse({1740.0f, 760.0f}, 3.0f);
  drawHouse({2150.0f, 560.0f}, 4.0f);
  drawHouse({2760.0f, 430.0f}, 5.0f);
  drawHouse({3200.0f, 700.0f}, 6.0f);
  drawHouse({3660.0f, 500.0f}, 7.0f);
  art.signPose->Reset();
  Engine::VectorShapeTransform spark;
  spark.rotationDegrees = visualTime_ * 90.0f;
  spark.opacity = 0.55f + 0.45f * (0.5f + 0.5f * std::sin(visualTime_ * 5.0f));
  art.signPose->SetGroupTransform(art.signGroups[2], spark);
  DrawVectorArt(renderer, art.sign, WorldToScreen({2020.0f, 755.0f}, camera),
                {80.0f, 60.0f}, art.signPose.get());
  renderer.DrawText("TOWN", WorldToScreen({1994.0f, 734.0f}, camera), 14,
                    {48, 56, 36, 255});

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
      art.npcPose->Reset();
      Engine::VectorShapeTransform body;
      body.translation = {0.0f, std::sin(visualTime_ * 2.4f + npc.position.x * 0.01f) * 1.5f};
      art.npcPose->SetGroupTransform(NpcGroupId(art, NpcGroup::Body), body);
      Engine::VectorShapeTransform head;
      head.rotationDegrees = std::sin(visualTime_ * 1.8f + npc.position.y * 0.01f) * 5.0f;
      art.npcPose->SetGroupTransform(NpcGroupId(art, NpcGroup::Head), head);
      art.npcPose->SetGroupTransform(NpcGroupId(art, NpcGroup::Eyes), head);
      DrawVectorArt(renderer, art.npc, screen, {48.0f, 60.0f},
                    art.npcPose.get(), 0.0f, White);
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
    art.playerPose->Reset();
    const float walk = std::sin((playerPosition.x + playerPosition.y) / 14.0f +
                                visualTime_ * 8.0f);
    Engine::VectorShapeTransform leftLeg;
    leftLeg.translation = {walk * 2.5f, 0.0f};
    leftLeg.rotationDegrees = walk * 9.0f;
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::LeftLeg),
                                      leftLeg);
    Engine::VectorShapeTransform rightLeg;
    rightLeg.translation = {-walk * 2.5f, 0.0f};
    rightLeg.rotationDegrees = -walk * 9.0f;
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::RightLeg),
                                      rightLeg);
    Engine::VectorShapeTransform head;
    head.translation = {0.0f, std::sin(visualTime_ * 5.0f) * 1.2f};
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Head),
                                      head);
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Hat),
                                      head);
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Eyes),
                                      head);
    const float facingRotation = std::atan2(facing.y, facing.x) * 57.2957795f - 90.0f;
    DrawVectorArt(renderer, art.player, WorldToScreen(playerPosition, camera),
                  {44.0f, 56.0f}, art.playerPose.get(), facingRotation * 0.03f);
  }

  std::size_t entityCount = 0;
  for (const ObjectID id : world_.Entities()) {
    if (ObjectRef(id).Resolve())
      ++entityCount;
  }

  DrawFilledRect(renderer, {14.0f, 14.0f}, {560.0f, 78.0f}, {5, 8, 22, 220});
  renderer.DrawText("NEON SVG RPG - WASD / FLECHES POUR MARCHER",
                    {24.0f, 24.0f}, 20, {235, 245, 210, 255});
  renderer.DrawText(
      "DA plus proche RPG 16-bit: herbe claire, eau ondulee, passerelles",
      {24.0f, 48.0f}, 18, {125, 231, 255, 255});
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

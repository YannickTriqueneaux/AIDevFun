#include "Game/Game.h"

#include "Game/GameAudio.h"
#include "Game/GameConfig.h"
#include "Game/VectorArt.h"

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
constexpr float FootstepIntervalSeconds = 0.31f;
constexpr Engine::Color White{255, 255, 255, 255};

void DrawVectorArt(Engine::Renderer2D &renderer,
                   const Engine::VectorShape &shape, Engine::Vector2 center,
                   Engine::Vector2 size,
                   const Engine::VectorShapePose *pose = nullptr,
                   float rotationDegrees = 0.0f, Engine::Color tint = White) {
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

Engine::VectorShapeGroupID PlayerGroupId(const VectorArtResources &art,
                                         PlayerGroup group) {
  return art.playerGroups[static_cast<std::size_t>(group)];
}

Engine::VectorShapeGroupID NpcGroupId(const VectorArtResources &art,
                                      NpcGroup group) {
  return art.npcGroups[static_cast<std::size_t>(group)];
}

Engine::VectorShapeGroupID DragonGroupId(const VectorArtResources &art,
                                         DragonGroup group) {
  return art.dragonGroups[static_cast<std::size_t>(group)];
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

bool WasVirtualKeyPressed(int virtualKey, bool &wasDown) {
#if defined(_WIN32)
  const bool down = (::GetAsyncKeyState(virtualKey) & 0x8000) != 0;
#else
  (void)virtualKey;
  const bool down = false;
#endif
  const bool pressed = down && !wasDown;
  wasDown = down;
  return pressed;
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

static constexpr std::array<NpcData, 4> Npcs{{
    {{1960.0f, 655.0f}, {190, 48, 80, 255}, "Hi! Welcome to the village."},
    {{1510.0f, 575.0f}, {80, 90, 160, 255}, "The houses block the path."},
    {{2360.0f, 790.0f}, {48, 98, 48, 255}, "The lake is calm today."},
    {{3140.0f, 775.0f}, {245, 205, 92, 255}, "Keep going east!"},
}};

int NearbyNpcIndex(Engine::Vector2 playerPosition) {
  for (std::size_t index = 0; index < Npcs.size(); ++index) {
    if (DistanceSquared(playerPosition, Npcs[index].position) <= 72.0f * 72.0f)
      return static_cast<int>(index);
  }
  return -1;
}

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

void DrawKnightSprite(Engine::Renderer2D &renderer, Engine::Vector2 center,
                      float attackFlash) {
  renderer.DrawCircle({center.x, center.y + 17.0f}, 15.0f, {48, 56, 36, 85});
  DrawFilledRect(renderer, {center.x - 9.0f, center.y + 6.0f}, {6.0f, 15.0f},
                 {48, 48, 58, 255});
  DrawFilledRect(renderer, {center.x + 3.0f, center.y + 6.0f}, {6.0f, 15.0f},
                 {48, 48, 58, 255});
  DrawFilledRect(renderer, {center.x - 13.0f, center.y - 14.0f}, {26.0f, 24.0f},
                 {160, 170, 180, 255});
  DrawRectOutline(renderer, {center.x - 13.0f, center.y - 14.0f}, {26.0f, 24.0f},
                  2.0f, {70, 78, 88, 255});
  DrawFilledRect(renderer, {center.x - 9.0f, center.y - 34.0f}, {18.0f, 18.0f},
                 {185, 195, 205, 255});
  DrawFilledRect(renderer, {center.x - 13.0f, center.y - 38.0f}, {26.0f, 8.0f},
                 {105, 115, 128, 255});
  DrawFilledRect(renderer, {center.x - 5.0f, center.y - 27.0f}, {10.0f, 3.0f},
                 {15, 18, 28, 255});
  renderer.DrawLine({center.x + 12.0f, center.y - 10.0f},
                    {center.x + 34.0f, center.y - 28.0f}, 4.0f,
                    {220, 226, 232, 255});
  renderer.DrawLine({center.x + 9.0f, center.y - 7.0f},
                    {center.x + 18.0f, center.y - 1.0f}, 5.0f,
                    {92, 64, 48, 255});
  if (attackFlash > 0.0f) {
    const float alpha = std::clamp(attackFlash / 0.42f, 0.0f, 1.0f);
    renderer.DrawLine({center.x + 24.0f, center.y - 32.0f},
                      {center.x + 62.0f, center.y + 4.0f}, 6.0f,
                      {245, 245, 210, static_cast<std::uint8_t>(220.0f * alpha)});
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

void DrawSettingsMenu(Engine::Renderer2D &renderer, float volume) {
  const float width = 420.0f;
  const float height = 178.0f;
  const Engine::Vector2 topLeft{
      static_cast<float>(renderer.GetWidth()) * 0.5f - width * 0.5f,
      static_cast<float>(renderer.GetHeight()) * 0.5f - height * 0.5f};
  DrawFilledRect(renderer, topLeft, {width, height}, {5, 8, 22, 235});
  DrawRectOutline(renderer, topLeft, {width, height}, 4.0f, {245, 205, 92, 255});
  renderer.DrawText("SETTINGS", {topLeft.x + 142.0f, topLeft.y + 18.0f}, 28,
                    {245, 245, 210, 255});
  renderer.DrawText("Volume", {topLeft.x + 34.0f, topLeft.y + 72.0f}, 20,
                    {245, 245, 210, 255});

  const float barWidth = 260.0f;
  const Engine::Vector2 barTopLeft{topLeft.x + 126.0f, topLeft.y + 78.0f};
  DrawFilledRect(renderer, barTopLeft, {barWidth, 18.0f}, {48, 56, 70, 255});
  DrawFilledRect(renderer, barTopLeft,
                 {barWidth * std::clamp(volume, 0.0f, 1.0f), 18.0f},
                 {74, 203, 92, 255});
  DrawRectOutline(renderer, barTopLeft, {barWidth, 18.0f}, 2.0f,
                  {245, 245, 210, 255});

  const int percent = static_cast<int>(std::round(std::clamp(volume, 0.0f, 1.0f) * 100.0f));
  renderer.DrawText(std::to_string(percent) + "%", {topLeft.x + 316.0f, topLeft.y + 106.0f},
                    18, {245, 245, 210, 255});
  renderer.DrawText("Left/Right: change   M: close",
                    {topLeft.x + 54.0f, topLeft.y + 136.0f}, 18,
                    {190, 210, 190, 255});
}
} // namespace

ProceduralGame::ProceduralGame()
    : world_(*gameInstance_.GetWorld()),
      art_(std::make_unique<VectorArtResources>()),
      audio_(std::make_unique<AudioResources>()) {
  RegisterGameplayTypes();
  EnsureCoreEntities();
}

ProceduralGame::~ProceduralGame() = default;

void ProceduralGame::Initialize() {
  if (art_)
    art_->Initialize();
  if (audio_) {
    audio_->Initialize();
    audio_->SetMasterVolume(audioVolume_);
  }
}

void ProceduralGame::Shutdown() {
  if (audio_)
    audio_->Shutdown();
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
      Engine::Gameplay::MakeComponentType<BaseGame::DragonFollower>(
          "DragonFollower"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<BaseGame::KnightVisitor>(
          "KnightVisitor"));
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
      {BaseGame::DragonEntityType, "Dragon", {BaseGame::DragonFollower::Type}});
  world_.RegisterEntity(
      {BaseGame::KnightEntityType, "Knight", {BaseGame::KnightVisitor::Type}});
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
  Entity *dragon = nullptr;
  Entity *knight = nullptr;
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
    } else if (entity->GetTypeID() == BaseGame::DragonEntityType) {
      dragon = entity;
      dragonEntity_ = ObjectRef(id);
    } else if (entity->GetTypeID() == BaseGame::KnightEntityType) {
      knight = entity;
      knightEntity_ = ObjectRef(id);
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

  if (!dragon) {
    dragonEntity_ = world_.Spawn(BaseGame::DragonEntityType, "LittleDragon");
    world_.FlushSpawns();
    dragon = dragonEntity_.Resolve();
    dragon->transform.position = {player->transform.position.x - 58.0f,
                                  player->transform.position.y - 34.0f};
  }
  dragonFollower_ = dragon->GetComponent<BaseGame::DragonFollower>();
  if (auto *follower = dragonFollower_.Resolve())
    follower->SetTarget(playerEntity_);

  if (!knight) {
    knightEntity_ = world_.Spawn(BaseGame::KnightEntityType, "PassingKnight");
    world_.FlushSpawns();
    knight = knightEntity_.Resolve();
    knight->transform.position = {player->transform.position.x - 360.0f,
                                  player->transform.position.y + 24.0f};
  }
  knightVisitor_ = knight->GetComponent<BaseGame::KnightVisitor>();
  if (auto *visitor = knightVisitor_.Resolve())
    visitor->SetTarget(dragonEntity_);
}

void ProceduralGame::Update(const Engine::InputSystem &input, float deltaTime) {
  ENGINE_PROFILE_SCOPE("BaseGame Update");
  visualTime_ += deltaTime;
  if (audio_)
    audio_->Update();
  if (WasVirtualKeyPressed('M', mKeyWasDown_)) {
    settingsMenuOpen_ = !settingsMenuOpen_;
  }
  if (settingsMenuOpen_) {
#if defined(_WIN32)
    const bool volumeUp = WasVirtualKeyPressed(VK_RIGHT, volumeUpKeyWasDown_);
    const bool volumeDown = WasVirtualKeyPressed(VK_LEFT, volumeDownKeyWasDown_);
#else
    const bool volumeUp = WasVirtualKeyPressed(0, volumeUpKeyWasDown_);
    const bool volumeDown = WasVirtualKeyPressed(0, volumeDownKeyWasDown_);
#endif
    if (volumeUp || volumeDown) {
      audioVolume_ = std::clamp(audioVolume_ + (volumeUp ? 0.1f : -0.1f),
                                0.0f, 1.0f);
      if (audio_)
        audio_->SetMasterVolume(audioVolume_);
    }
  } else {
    volumeUpKeyWasDown_ = false;
    volumeDownKeyWasDown_ = false;
  }

  const PlayerCommand command =
      settingsMenuOpen_ ? PlayerCommand{} : inputBindings_.BuildPlayerCommand(input);
  playerWalking_ = command.movement.x != 0.0f || command.movement.y != 0.0f;
  footstepCooldown_ = std::max(0.0f, footstepCooldown_ - deltaTime);
  if (playerWalking_ && footstepCooldown_ <= 0.0f) {
    if (audio_ && audio_->ready)
      audio_->PlayFootstep(leftFootstep_);
    leftFootstep_ = !leftFootstep_;
    footstepCooldown_ = FootstepIntervalSeconds;
  } else if (!playerWalking_) {
    footstepCooldown_ = 0.0f;
  }
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

  int nearbyNpcIndex = -1;
  if (const auto *player = playerEntity_.Resolve())
    nearbyNpcIndex = NearbyNpcIndex(player->transform.position);
  if (nearbyNpcIndex != activeNpcIndex_) {
    activeNpcIndex_ = nearbyNpcIndex;
    if (nearbyNpcIndex >= 0 && audio_ && audio_->ready)
      audio_->PlayNpcVoice(static_cast<std::size_t>(nearbyNpcIndex));
  }
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
  for (const ObjectID id : world_.Entities()) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity || entity->GetTypeID() != BaseGame::KnightEntityType)
      continue;
    auto *knight = entity->GetComponent<BaseGame::KnightVisitor>().Resolve();
    auto *dragon = dragonFollower_.Resolve();
    if (knight && dragon && knight->ConsumeDragonAttack())
      dragon->Kill();
  }
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
    renderer.DrawText("Loading SVGs...", {24.0f, 24.0f}, 22,
                      {235, 245, 210, 255});
    return;
  }
  const VectorArtResources &art = *art_;

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
         x <
         camera.x + static_cast<float>(renderer.GetWidth()) + GroundTileSize;
         x += GroundTileSize) {
      DrawVectorArt(
          renderer, art.ground,
          WorldToScreen({x + GroundTileSize * 0.5f, y + GroundTileSize * 0.5f},
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
  drawRoad({1160.0f, 0.0f},
           {74.0f, static_cast<float>(GameConfig::WorldHeight)}, true);
  drawRoad({2040.0f, 0.0f},
           {74.0f, static_cast<float>(GameConfig::WorldHeight)}, true);
  drawRoad({3180.0f, 0.0f},
           {74.0f, static_cast<float>(GameConfig::WorldHeight)}, true);

  auto drawHouse = [&](Engine::Vector2 origin, float hueShift) {
    art.housePose->Reset();
    Engine::VectorShapeTransform windows;
    windows.opacity =
        0.68f + 0.32f * (0.5f + 0.5f * std::sin(visualTime_ * 3.0f + hueShift));
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
    DrawVectorArt(renderer, art.tree, WorldToScreen(origin, camera),
                  {56.0f, 76.0f}, art.treePose.get());
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
    DrawVectorArt(
        renderer, art.water,
        WorldToScreen({origin.x + size.x * 0.5f, origin.y + size.y * 0.5f},
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

  const NpcData *nearbyNpc = nullptr;
  for (const NpcData &npc : Npcs) {
    const Engine::Vector2 screen = WorldToScreen(npc.position, camera);
    if (screen.x >= -48.0f && screen.y >= -80.0f &&
        screen.x <= renderer.GetWidth() + 48.0f &&
        screen.y <= renderer.GetHeight() + 48.0f) {
      art.npcPose->Reset();
      Engine::VectorShapeTransform body;
      body.translation = {
          0.0f, std::sin(visualTime_ * 2.4f + npc.position.x * 0.01f) * 1.5f};
      art.npcPose->SetGroupTransform(NpcGroupId(art, NpcGroup::Body), body);
      Engine::VectorShapeTransform head;
      head.rotationDegrees =
          std::sin(visualTime_ * 1.8f + npc.position.y * 0.01f) * 5.0f;
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

  if (const auto *knight = knightEntity_.Resolve()) {
    const auto *visitor = knightVisitor_.Resolve();
    if (visitor && visitor->IsActive()) {
      DrawKnightSprite(renderer, WorldToScreen(knight->transform.position, camera),
                       visitor->AttackFlash());
    }
  }

  if (const auto *dragon = dragonEntity_.Resolve()) {
    const auto *follower = dragonFollower_.Resolve();
    if (follower && !follower->IsAlive()) {
      const Engine::Vector2 smoke = WorldToScreen(dragon->transform.position, camera);
      const float progress = follower->RespawnProgress();
      renderer.DrawCircle({smoke.x - 8.0f, smoke.y + 2.0f},
                          6.0f + progress * 8.0f,
                          {105, 115, 128,
                           static_cast<std::uint8_t>(140.0f * (1.0f - progress))});
      renderer.DrawCircle({smoke.x + 7.0f, smoke.y - 5.0f},
                          5.0f + progress * 7.0f,
                          {160, 170, 180,
                           static_cast<std::uint8_t>(120.0f * (1.0f - progress))});
      if (progress > 0.72f) {
        renderer.DrawCircle(smoke, 7.0f + std::sin(visualTime_ * 18.0f) * 2.0f,
                            {116, 255, 150,
                             static_cast<std::uint8_t>(255.0f * (progress - 0.72f) / 0.28f)});
      }
    } else {
      const float fireIntensity = follower ? follower->FireIntensity() : 0.0f;
      art.dragonPose->Reset();
      const float wingBeat =
          std::sin(visualTime_ * (fireIntensity > 0.0f ? 14.0f : 9.0f));
      Engine::VectorShapeTransform wings;
      wings.scale = {1.0f, 0.82f + 0.18f * (0.5f + 0.5f * wingBeat)};
      wings.rotationDegrees = wingBeat * (7.0f + fireIntensity * 8.0f);
      art.dragonPose->SetGroupTransform(DragonGroupId(art, DragonGroup::Wings),
                                        wings);
      Engine::VectorShapeTransform body;
      body.translation = {-fireIntensity * 2.0f,
                          std::sin(visualTime_ * 4.0f) * 2.2f};
      art.dragonPose->SetGroupTransform(DragonGroupId(art, DragonGroup::Body),
                                        body);
      art.dragonPose->SetGroupTransform(DragonGroupId(art, DragonGroup::Head),
                                        body);
      Engine::VectorShapeTransform tail;
      tail.rotationDegrees = std::sin(visualTime_ * 5.0f) * 10.0f -
                             fireIntensity * 8.0f;
      art.dragonPose->SetGroupTransform(DragonGroupId(art, DragonGroup::Tail),
                                        tail);
      Engine::VectorShapeTransform flame;
      if (fireIntensity > 0.0f) {
        flame.opacity = 1.0f;
        flame.translation = {fireIntensity * 5.0f, 0.0f};
        flame.scale = {1.2f + fireIntensity * 4.4f,
                       0.9f + fireIntensity * 2.0f};
      } else {
        flame.opacity = 0.0f;
        flame.scale = {0.15f, 0.15f};
      }
      art.dragonPose->SetGroupTransform(DragonGroupId(art, DragonGroup::Flame),
                                        flame);

      const Engine::Vector2 dragonScreen =
          WorldToScreen(dragon->transform.position, camera);
      if (fireIntensity > 0.0f) {
        const Engine::Vector2 mouth{dragonScreen.x + 21.0f,
                                    dragonScreen.y - 1.0f};
        const float flicker = 0.5f + 0.5f * std::sin(visualTime_ * 38.0f);
        const float length = 34.0f + fireIntensity * 42.0f + flicker * 8.0f;
        const float spread = 10.0f + fireIntensity * 14.0f;
        renderer.DrawLine(mouth, {mouth.x + length, mouth.y - spread * 0.55f},
                          14.0f * fireIntensity,
                          {255, 100, 32,
                           static_cast<std::uint8_t>(185.0f * fireIntensity)});
        renderer.DrawLine({mouth.x + 2.0f, mouth.y + 1.0f},
                          {mouth.x + length * 0.92f, mouth.y + spread * 0.38f},
                          18.0f * fireIntensity,
                          {255, 135, 36,
                           static_cast<std::uint8_t>(215.0f * fireIntensity)});
        renderer.DrawLine({mouth.x + 8.0f, mouth.y},
                          {mouth.x + length * 0.72f,
                           mouth.y + std::sin(visualTime_ * 22.0f) * spread * 0.22f},
                          8.0f * fireIntensity,
                          {255, 235, 92,
                           static_cast<std::uint8_t>(235.0f * fireIntensity)});
        renderer.DrawCircle(
            {mouth.x + length * 0.82f,
             mouth.y + std::sin(visualTime_ * 17.0f) * spread * 0.28f},
            5.0f + 5.0f * fireIntensity,
            {255, 74, 42, static_cast<std::uint8_t>(165.0f * fireIntensity)});
      }
      DrawVectorArt(renderer, art.dragon, dragonScreen, {46.0f, 38.0f},
                    art.dragonPose.get());
    }
  }

  if (player) {
    Engine::Vector2 facing{0.0f, 1.0f};
    if (const auto *movement =
            player->GetComponent<BaseGame::PlayerMovement>().Resolve())
      facing = movement->Facing();
    art.playerPose->Reset();
    const float stepProgress =
        playerWalking_
            ? std::clamp((FootstepIntervalSeconds - footstepCooldown_) /
                             FootstepIntervalSeconds,
                         0.0f, 1.0f)
            : 0.0f;
    const float phase = stepProgress * 3.14159265f;
    const float stride = stepProgress * 2.0f - 1.0f;
    const float lift = playerWalking_ ? std::sin(phase) : 0.0f;
    const bool leftIsSwinging = leftFootstep_;

    auto configureLeg = [&](PlayerGroup legGroup, PlayerGroup footGroup,
                            bool swinging, float side) {
      Engine::VectorShapeTransform leg;
      Engine::VectorShapeTransform foot;
      if (playerWalking_) {
        const float strideAmount = swinging ? stride : -stride * 0.38f;
        const float liftAmount = swinging ? lift : 0.0f;
        leg.translation = {strideAmount * 2.4f * side, -liftAmount * 2.1f};
        leg.rotationDegrees = strideAmount * 10.0f * side;
        foot.translation = {strideAmount * 3.1f * side,
                            -liftAmount * 3.0f + (swinging ? 0.0f : 0.45f)};
        foot.rotationDegrees = strideAmount * 7.0f * side;
        foot.scale = {swinging ? 0.96f : 1.08f, swinging ? 0.92f : 1.04f};
      }
      art.playerPose->SetGroupTransform(PlayerGroupId(art, legGroup), leg);
      art.playerPose->SetGroupTransform(PlayerGroupId(art, footGroup), foot);
    };
    configureLeg(PlayerGroup::LeftLeg, PlayerGroup::LeftFoot, leftIsSwinging,
                 -1.0f);
    configureLeg(PlayerGroup::RightLeg, PlayerGroup::RightFoot, !leftIsSwinging,
                 1.0f);

    const float bodyRise = playerWalking_ ? lift * 1.25f : 0.0f;
    Engine::VectorShapeTransform body;
    body.translation = {0.0f, -bodyRise};
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Body),
                                      body);
    Engine::VectorShapeTransform head;
    head.translation = {0.0f,
                        -bodyRise + (playerWalking_ ? lift * 0.45f : 0.0f)};
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Head),
                                      head);
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Hat),
                                      head);
    art.playerPose->SetGroupTransform(PlayerGroupId(art, PlayerGroup::Eyes),
                                      head);
    const float facingRotation =
        std::atan2(facing.y, facing.x) * 57.2957795f - 90.0f;
    DrawVectorArt(renderer, art.player, WorldToScreen(playerPosition, camera),
                  {44.0f, 56.0f}, art.playerPose.get(), facingRotation * 0.03f);
  }

  std::size_t entityCount = 0;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (entity && entity->GetTypeID() != BaseGame::ArenaDirectorEntityType)
      ++entityCount;
  }

  DrawFilledRect(renderer, {14.0f, 14.0f}, {260.0f, 34.0f}, {5, 8, 22, 220});
  renderer.DrawText("Entities: " + std::to_string(entityCount) +
                        "   NPCs: " + std::to_string(Npcs.size()) +
                        "   M: Settings",
                    {24.0f, 22.0f}, 18, {245, 245, 210, 255});

  if (settingsMenuOpen_)
    DrawSettingsMenu(renderer, audioVolume_);
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
  dragonFollower_ = {};
  dragonEntity_ = {};
  knightVisitor_ = {};
  knightEntity_ = {};
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

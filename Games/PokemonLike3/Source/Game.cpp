#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Core/Profile.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Serialization/Serializer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {
using Entity = Engine::Gameplay::Entity;
using ObjectID = Engine::Gameplay::ObjectID;
using ObjectRef = Engine::Gameplay::ObjectRef<Entity>;
using TypeID = Engine::Gameplay::TypeID;

constexpr std::array FootstepVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 170.0f,
                                 .volume = 0.18f,
                                 .envelope = {.attackSeconds = 0.0f,
                                              .decaySeconds = 0.018f,
                                              .sustainLevel = 0.12f,
                                              .releaseSeconds = 0.045f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 1200.0f,
                                 .noiseSeed = 0x7091a3u},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 92.0f,
                                 .volume = 0.08f,
                                 .envelope = {.attackSeconds = 0.0f,
                                              .decaySeconds = 0.012f,
                                              .sustainLevel = 0.1f,
                                              .releaseSeconds = 0.035f}}};
constexpr Engine::ProceduralSoundDefinition FootstepPatch{.durationSeconds = 0.07f,
                                                          .voices = FootstepVoices};

constexpr std::array TalkPitch{Engine::AudioEnvelopePoint{0.0f, 1.0f},
                               Engine::AudioEnvelopePoint{0.08f, 1.55f},
                               Engine::AudioEnvelopePoint{0.16f, 0.82f},
                               Engine::AudioEnvelopePoint{0.24f, 1.28f}};
constexpr std::array TalkVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Square,
                                 .frequencyHz = 430.0f,
                                 .volume = 0.18f,
                                 .dutyCycle = 0.42f,
                                 .envelope = {.attackSeconds = 0.006f,
                                              .decaySeconds = 0.035f,
                                              .sustainLevel = 0.55f,
                                              .releaseSeconds = 0.045f},
                                 .frequencyMultiplier = {.points = TalkPitch},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 2400.0f},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 650.0f,
                                 .volume = 0.11f,
                                 .pan = 0.56f,
                                 .envelope = {.attackSeconds = 0.004f,
                                              .decaySeconds = 0.03f,
                                              .sustainLevel = 0.42f,
                                              .releaseSeconds = 0.04f},
                                 .frequencyMultiplier = {.points = TalkPitch}}};
constexpr Engine::ProceduralSoundDefinition TalkPatch{
    .durationSeconds = 0.26f,
    .voices = TalkVoices,
    .effects = {.echoDelaySeconds = 0.035f, .echoFeedback = 0.12f, .echoMix = 0.08f}};

constexpr std::array PokemonCryPitch{Engine::AudioEnvelopePoint{0.0f, 1.0f},
                                     Engine::AudioEnvelopePoint{0.07f, 1.82f},
                                     Engine::AudioEnvelopePoint{0.16f, 1.08f}};
constexpr std::array PokemonCryVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 720.0f,
                                 .volume = 0.19f,
                                 .envelope = {.attackSeconds = 0.006f,
                                              .decaySeconds = 0.04f,
                                              .sustainLevel = 0.45f,
                                              .releaseSeconds = 0.08f},
                                 .frequencyMultiplier = {.points = PokemonCryPitch}},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Square,
                                 .frequencyHz = 360.0f,
                                 .volume = 0.09f,
                                 .dutyCycle = 0.28f,
                                 .envelope = {.attackSeconds = 0.004f,
                                              .decaySeconds = 0.03f,
                                              .sustainLevel = 0.35f,
                                              .releaseSeconds = 0.07f},
                                 .frequencyMultiplier = {.points = PokemonCryPitch},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 2600.0f}};
constexpr Engine::ProceduralSoundDefinition PokemonCryPatch{
    .durationSeconds = 0.24f,
    .voices = PokemonCryVoices,
    .effects = {.echoDelaySeconds = 0.06f, .echoFeedback = 0.16f, .echoMix = 0.1f}};

float DistanceSquared(Engine::Vector2 left, Engine::Vector2 right) {
  const float x = left.x - right.x;
  const float y = left.y - right.y;
  return x * x + y * y;
}

void DrawFilledRect(Engine::Renderer2D &renderer, Engine::Vector2 center,
                    Engine::Vector2 size, Engine::Color color) {
  const int rows = std::max(1, static_cast<int>(size.y));
  const float left = center.x - size.x * 0.5f;
  const float right = center.x + size.x * 0.5f;
  const float top = center.y - size.y * 0.5f;
  for (int row = 0; row < rows; ++row) {
    const float y = top + static_cast<float>(row) + 0.5f;
    renderer.DrawLine({left, y}, {right, y}, 1.0f, color);
  }
}

void DrawRectOutline(Engine::Renderer2D &renderer, Engine::Vector2 center,
                     Engine::Vector2 size, float thickness,
                     Engine::Color color) {
  const float left = center.x - size.x * 0.5f;
  const float right = center.x + size.x * 0.5f;
  const float top = center.y - size.y * 0.5f;
  const float bottom = center.y + size.y * 0.5f;
  renderer.DrawLine({left, top}, {right, top}, thickness, color);
  renderer.DrawLine({right, top}, {right, bottom}, thickness, color);
  renderer.DrawLine({right, bottom}, {left, bottom}, thickness, color);
  renderer.DrawLine({left, bottom}, {left, top}, thickness, color);
}

void DrawTileDither(Engine::Renderer2D &renderer, float cameraX) {
  const int startTile = std::max(0, static_cast<int>(cameraX) / GameConfig::TileSize - 1);
  const int endTile = std::min(GameConfig::WorldWidth / GameConfig::TileSize + 1,
                               startTile + GameConfig::PlayAreaWidth / GameConfig::TileSize + 4);
  for (int y = 0; y < GameConfig::PlayAreaHeight; y += GameConfig::TileSize) {
    for (int tile = startTile; tile <= endTile; ++tile) {
      const int x = tile * GameConfig::TileSize;
      const float screenX = static_cast<float>(x) - cameraX;
      const bool dark = (tile + (y / GameConfig::TileSize)) % 2 == 0;
      DrawFilledRect(renderer,
                     {screenX + static_cast<float>(GameConfig::TileSize) * 0.5f,
                      static_cast<float>(y + GameConfig::TileSize / 2)},
                     {static_cast<float>(GameConfig::TileSize),
                      static_cast<float>(GameConfig::TileSize)},
                     dark ? Engine::Color{112, 194, 148, 255}
                          : Engine::Color{126, 205, 157, 255});
      if ((x + y) % 96 == 0)
        renderer.DrawCircle({screenX + 9.0f, static_cast<float>(y + 21)}, 2.0f,
                            {73, 160, 118, 120});
    }
  }
}

void DrawPath(Engine::Renderer2D &renderer) {
  DrawFilledRect(renderer, {640.0f, 382.0f}, {1160.0f, 88.0f},
                 {221, 194, 126, 255});
  DrawFilledRect(renderer, {420.0f, 468.0f}, {108.0f, 210.0f},
                 {221, 194, 126, 255});
  DrawFilledRect(renderer, {840.0f, 258.0f}, {108.0f, 250.0f},
                 {221, 194, 126, 255});
  for (int x = 76; x < 1190; x += 44)
    renderer.DrawCircle({static_cast<float>(x), 408.0f}, 2.0f,
                        {190, 162, 93, 180});
}

void DrawLake(Engine::Renderer2D &renderer) {
  DrawFilledRect(renderer, {258.0f, 146.0f}, {278.0f, 176.0f},
                 {96, 176, 220, 255});
  DrawFilledRect(renderer, {334.0f, 236.0f}, {164.0f, 74.0f},
                 {96, 176, 220, 255});
  renderer.DrawCircle({142.0f, 94.0f}, 42.0f, {96, 176, 220, 255});
  renderer.DrawCircle({396.0f, 78.0f}, 52.0f, {96, 176, 220, 255});
  renderer.DrawCircle({428.0f, 220.0f}, 38.0f, {96, 176, 220, 255});
  renderer.DrawLine({110.0f, 246.0f}, {484.0f, 246.0f}, 5.0f,
                    {69, 128, 170, 160});
  renderer.DrawLine({130.0f, 70.0f}, {418.0f, 70.0f}, 4.0f,
                    {173, 225, 241, 180});
}

void DrawFence(Engine::Renderer2D &renderer) {
  for (int x = 58; x < 760; x += 34) {
    DrawFilledRect(renderer, {static_cast<float>(x), 300.0f}, {10.0f, 50.0f},
                   {165, 104, 66, 255});
    renderer.DrawCircle({static_cast<float>(x), 273.0f}, 6.0f,
                        {195, 129, 78, 255});
  }
  renderer.DrawLine({44.0f, 290.0f}, {774.0f, 290.0f}, 8.0f,
                    {190, 124, 76, 255});
}

void DrawHouse(Engine::Renderer2D &renderer, Engine::Vector2 center,
               Engine::Color wall, Engine::Color roof, bool clinic) {
  DrawFilledRect(renderer, {center.x, center.y + 30.0f}, {176.0f, 116.0f}, wall);
  DrawRectOutline(renderer, {center.x, center.y + 30.0f}, {176.0f, 116.0f}, 3.0f,
                  {77, 80, 98, 255});
  DrawFilledRect(renderer, {center.x, center.y - 54.0f}, {194.0f, 56.0f}, roof);
  DrawFilledRect(renderer, {center.x, center.y - 18.0f}, {182.0f, 22.0f},
                 {128, 76, 92, 255});
  DrawFilledRect(renderer, {center.x - 46.0f, center.y + 42.0f}, {34.0f, 46.0f},
                 {202, 89, 72, 255});
  DrawFilledRect(renderer, {center.x + 38.0f, center.y + 18.0f}, {52.0f, 34.0f},
                 {128, 183, 219, 255});
  DrawRectOutline(renderer, {center.x + 38.0f, center.y + 18.0f}, {52.0f, 34.0f},
                  3.0f, {72, 86, 121, 255});
  if (clinic) {
    renderer.DrawCircle({center.x - 80.0f, center.y - 58.0f}, 16.0f,
                        {245, 245, 245, 255});
    renderer.DrawLine({center.x - 90.0f, center.y - 58.0f},
                      {center.x - 70.0f, center.y - 58.0f}, 6.0f,
                      {211, 67, 67, 255});
    renderer.DrawLine({center.x - 80.0f, center.y - 68.0f},
                      {center.x - 80.0f, center.y - 48.0f}, 6.0f,
                      {211, 67, 67, 255});
  }
}

void DrawTree(Engine::Renderer2D &renderer, Engine::Vector2 base) {
  DrawFilledRect(renderer, {base.x, base.y + 16.0f}, {18.0f, 36.0f},
                 {122, 86, 45, 255});
  renderer.DrawCircle({base.x, base.y - 14.0f}, 31.0f, {47, 132, 73, 255});
  renderer.DrawCircle({base.x - 22.0f, base.y - 2.0f}, 24.0f,
                      {58, 154, 82, 255});
  renderer.DrawCircle({base.x + 22.0f, base.y - 2.0f}, 24.0f,
                      {58, 154, 82, 255});
  renderer.DrawCircle({base.x, base.y - 30.0f}, 20.0f, {83, 181, 92, 255});
}

void DrawFlowerPatch(Engine::Renderer2D &renderer, Engine::Vector2 center) {
  renderer.DrawCircle({center.x - 8.0f, center.y}, 5.0f, {236, 92, 151, 255});
  renderer.DrawCircle({center.x + 8.0f, center.y}, 5.0f, {255, 248, 230, 255});
  renderer.DrawCircle({center.x, center.y - 7.0f}, 4.0f, {250, 190, 80, 255});
  renderer.DrawLine({center.x - 8.0f, center.y + 6.0f}, {center.x - 8.0f, center.y + 20.0f},
                    3.0f, {45, 144, 75, 255});
  renderer.DrawLine({center.x + 8.0f, center.y + 6.0f}, {center.x + 8.0f, center.y + 20.0f},
                    3.0f, {45, 144, 75, 255});
}

const char *TalkText(PocketTown::NpcRole role) {
  switch (role) {
  case PocketTown::NpcRole::Elder:
    return "The lake has been calm today.";
  case PocketTown::NpcRole::Clerk:
    return "Fresh supplies just came in!";
  case PocketTown::NpcRole::Kid:
    return "I saw something sparkle by the trees!";
  case PocketTown::NpcRole::Fisher:
    return "The best catches hide near the reeds.";
  }
  return "Nice weather for a walk.";
}

const char *PokemonTalkText(PocketTown::PokemonSpecies species) {
  switch (species) {
  case PocketTown::PokemonSpecies::Leafling:
    return "Liiif! Leaf leaf!";
  case PocketTown::PokemonSpecies::Aquabbit:
    return "Bubba-bun!";
  case PocketTown::PokemonSpecies::Embercub:
    return "Mrrr-ember!";
  case PocketTown::PokemonSpecies::Sparko:
    return "Pika-zzt!";
  case PocketTown::PokemonSpecies::Puffowl:
    return "Hoo-puff!";
  }
  return "...!";
}

Engine::Color ShirtColor(PocketTown::NpcRole role) {
  switch (role) {
  case PocketTown::NpcRole::Elder:
    return {129, 78, 154, 255};
  case PocketTown::NpcRole::Clerk:
    return {218, 78, 90, 255};
  case PocketTown::NpcRole::Kid:
    return {70, 128, 217, 255};
  case PocketTown::NpcRole::Fisher:
    return {225, 180, 62, 255};
  }
  return {129, 78, 154, 255};
}

void DrawSpeechBubble(Engine::Renderer2D &renderer, Engine::Vector2 anchor,
                      const std::string &text) {
  const float width = std::clamp(28.0f + static_cast<float>(text.size()) * 9.0f,
                                 150.0f, 330.0f);
  const Engine::Vector2 center{anchor.x, anchor.y - 74.0f};
  DrawFilledRect(renderer, center, {width, 40.0f}, {250, 250, 244, 244});
  DrawRectOutline(renderer, center, {width, 40.0f}, 3.0f, {52, 62, 75, 255});
  renderer.DrawLine({anchor.x - 10.0f, center.y + 20.0f}, {anchor.x, anchor.y - 38.0f},
                    4.0f, {250, 250, 244, 244});
  renderer.DrawLine({anchor.x + 10.0f, center.y + 20.0f}, {anchor.x, anchor.y - 38.0f},
                    4.0f, {52, 62, 75, 255});
  renderer.DrawText(text, {center.x - width * 0.5f + 14.0f, center.y - 10.0f},
                    18, {35, 43, 54, 255});
}

void DrawPokemon(Engine::Renderer2D &renderer, Engine::Vector2 position,
                 PocketTown::PokemonSpecies species, PocketTown::Direction facing,
                 float cycle, bool moving) {
  const float bob = std::sin(cycle) * (moving ? 3.0f : 1.6f);
  const Engine::Vector2 p{position.x, position.y + bob};
  switch (species) {
  case PocketTown::PokemonSpecies::Leafling:
    renderer.DrawCircle({p.x, p.y + 4.0f}, 16.0f, {76, 181, 91, 255});
    renderer.DrawCircle({p.x - 11.0f, p.y - 8.0f}, 8.0f, {115, 215, 100, 255});
    renderer.DrawCircle({p.x + 11.0f, p.y - 8.0f}, 8.0f, {115, 215, 100, 255});
    renderer.DrawLine({p.x, p.y - 14.0f}, {p.x + 12.0f, p.y - 28.0f}, 6.0f,
                      {65, 156, 77, 255});
    break;
  case PocketTown::PokemonSpecies::Aquabbit:
    renderer.DrawCircle({p.x, p.y + 6.0f}, 15.0f, {89, 178, 226, 255});
    renderer.DrawLine({p.x - 8.0f, p.y - 6.0f}, {p.x - 15.0f, p.y - 25.0f}, 7.0f,
                      {121, 208, 242, 255});
    renderer.DrawLine({p.x + 8.0f, p.y - 6.0f}, {p.x + 15.0f, p.y - 25.0f}, 7.0f,
                      {121, 208, 242, 255});
    renderer.DrawCircle({p.x + 16.0f, p.y + 9.0f}, 6.0f, {206, 244, 255, 255});
    break;
  case PocketTown::PokemonSpecies::Embercub:
    renderer.DrawCircle({p.x, p.y + 6.0f}, 16.0f, {224, 103, 63, 255});
    renderer.DrawCircle({p.x - 10.0f, p.y - 9.0f}, 7.0f, {122, 70, 45, 255});
    renderer.DrawCircle({p.x + 10.0f, p.y - 9.0f}, 7.0f, {122, 70, 45, 255});
    renderer.DrawCircle({p.x + 17.0f, p.y - 2.0f}, 8.0f, {255, 191, 62, 255});
    break;
  case PocketTown::PokemonSpecies::Sparko:
    renderer.DrawCircle({p.x, p.y + 5.0f}, 14.0f, {244, 211, 68, 255});
    renderer.DrawLine({p.x - 14.0f, p.y - 6.0f}, {p.x - 25.0f, p.y - 20.0f}, 5.0f,
                      {244, 211, 68, 255});
    renderer.DrawLine({p.x + 14.0f, p.y - 6.0f}, {p.x + 25.0f, p.y - 20.0f}, 5.0f,
                      {244, 211, 68, 255});
    renderer.DrawLine({p.x + 13.0f, p.y + 10.0f}, {p.x + 29.0f, p.y + 2.0f}, 5.0f,
                      {99, 91, 65, 255});
    break;
  case PocketTown::PokemonSpecies::Puffowl:
    renderer.DrawCircle({p.x, p.y + 5.0f}, 17.0f, {218, 183, 229, 255});
    renderer.DrawCircle({p.x - 9.0f, p.y - 8.0f}, 6.0f, {246, 236, 251, 255});
    renderer.DrawCircle({p.x + 9.0f, p.y - 8.0f}, 6.0f, {246, 236, 251, 255});
    renderer.DrawLine({p.x - 16.0f, p.y + 1.0f}, {p.x - 29.0f, p.y + 10.0f}, 6.0f,
                      {186, 139, 205, 255});
    renderer.DrawLine({p.x + 16.0f, p.y + 1.0f}, {p.x + 29.0f, p.y + 10.0f}, 6.0f,
                      {186, 139, 205, 255});
    break;
  }
  if (facing != PocketTown::Direction::Up) {
    renderer.DrawCircle({p.x - 4.5f, p.y + 1.0f}, 1.8f, {36, 38, 45, 255});
    renderer.DrawCircle({p.x + 4.5f, p.y + 1.0f}, 1.8f, {36, 38, 45, 255});
  }
}

void DrawCharacter(Engine::Renderer2D &renderer, Engine::Vector2 position,
                   Engine::Color shirt, PocketTown::Direction facing,
                   float cycle, bool moving, bool player) {
  const float bob = moving ? std::sin(cycle) * 2.0f : 0.0f;
  const float step = moving ? std::sin(cycle) * 4.0f : 0.0f;
  const Engine::Vector2 p{position.x, position.y + bob};
  const Engine::Color skin{244, 190, 150, 255};
  const Engine::Color hair = player ? Engine::Color{44, 60, 90, 255}
                                    : Engine::Color{97, 69, 52, 255};

  renderer.DrawLine({p.x - 8.0f, p.y + 18.0f}, {p.x - 9.0f - step, p.y + 30.0f},
                    6.0f, {44, 63, 82, 255});
  renderer.DrawLine({p.x + 8.0f, p.y + 18.0f}, {p.x + 9.0f + step, p.y + 30.0f},
                    6.0f, {44, 63, 82, 255});
  renderer.DrawCircle({p.x, p.y + 7.0f}, 15.0f, shirt);
  renderer.DrawCircle({p.x, p.y - 12.0f}, 13.0f, skin);
  renderer.DrawCircle({p.x, p.y - 20.0f}, 9.0f, hair);
  if (player) {
    renderer.DrawLine({p.x - 15.0f, p.y - 22.0f}, {p.x + 15.0f, p.y - 22.0f},
                      6.0f, {239, 244, 250, 255});
    renderer.DrawCircle({p.x + 9.0f, p.y - 24.0f}, 5.0f, {84, 132, 207, 255});
  }

  if (facing != PocketTown::Direction::Up) {
    renderer.DrawCircle({p.x - 4.5f, p.y - 10.0f}, 1.8f, {35, 40, 50, 255});
    renderer.DrawCircle({p.x + 4.5f, p.y - 10.0f}, 1.8f, {35, 40, 50, 255});
  }
}

void DrawTown(Engine::Renderer2D &renderer, float cameraX) {
  DrawTileDither(renderer, cameraX);

  for (int segment = 0; segment < 4; ++segment) {
    const float offset = static_cast<float>(segment * GameConfig::PlayAreaWidth) - cameraX;
    if (offset > static_cast<float>(GameConfig::PlayAreaWidth) + 220.0f ||
        offset < -static_cast<float>(GameConfig::PlayAreaWidth) - 220.0f)
      continue;

    DrawFilledRect(renderer, {offset + 640.0f, 382.0f}, {1160.0f, 88.0f},
                   {221, 194, 126, 255});
    DrawFilledRect(renderer, {offset + 420.0f, 468.0f}, {108.0f, 210.0f},
                   {221, 194, 126, 255});
    DrawFilledRect(renderer, {offset + 840.0f, 258.0f}, {108.0f, 250.0f},
                   {221, 194, 126, 255});
    for (int x = 76; x < 1190; x += 44)
      renderer.DrawCircle({offset + static_cast<float>(x), 408.0f}, 2.0f,
                          {190, 162, 93, 180});

    DrawFilledRect(renderer, {offset + 258.0f, 146.0f}, {278.0f, 176.0f},
                   {96, 176, 220, 255});
    DrawFilledRect(renderer, {offset + 334.0f, 236.0f}, {164.0f, 74.0f},
                   {96, 176, 220, 255});
    renderer.DrawCircle({offset + 142.0f, 94.0f}, 42.0f, {96, 176, 220, 255});
    renderer.DrawCircle({offset + 396.0f, 78.0f}, 52.0f, {96, 176, 220, 255});
    renderer.DrawCircle({offset + 428.0f, 220.0f}, 38.0f, {96, 176, 220, 255});
    renderer.DrawLine({offset + 110.0f, 246.0f}, {offset + 484.0f, 246.0f},
                      5.0f, {69, 128, 170, 160});
    renderer.DrawLine({offset + 130.0f, 70.0f}, {offset + 418.0f, 70.0f},
                      4.0f, {173, 225, 241, 180});

    for (int x = 58; x < 760; x += 34) {
      DrawFilledRect(renderer, {offset + static_cast<float>(x), 300.0f},
                     {10.0f, 50.0f}, {165, 104, 66, 255});
      renderer.DrawCircle({offset + static_cast<float>(x), 273.0f}, 6.0f,
                          {195, 129, 78, 255});
    }
    renderer.DrawLine({offset + 44.0f, 290.0f}, {offset + 774.0f, 290.0f},
                      8.0f, {190, 124, 76, 255});

    DrawHouse(renderer, {offset + 1010.0f, 174.0f}, {221, 190, 124, 255},
              {198, 75, 77, 255}, false);
    DrawHouse(renderer, {offset + 210.0f, 538.0f}, {190, 214, 232, 255},
              {211, 89, 82, 255}, true);
    DrawHouse(renderer, {offset + 720.0f, 548.0f}, {215, 179, 110, 255},
              {154, 123, 87, 255}, false);

    for (const Engine::Vector2 tree : std::array<Engine::Vector2, 12>{
             Engine::Vector2{68.0f, 116.0f}, Engine::Vector2{126.0f, 116.0f},
             Engine::Vector2{80.0f, 634.0f}, Engine::Vector2{154.0f, 650.0f},
             Engine::Vector2{234.0f, 652.0f}, Engine::Vector2{1030.0f, 650.0f},
             Engine::Vector2{1110.0f, 636.0f}, Engine::Vector2{1194.0f, 628.0f},
             Engine::Vector2{1096.0f, 366.0f}, Engine::Vector2{1188.0f, 342.0f},
             Engine::Vector2{532.0f, 108.0f}, Engine::Vector2{600.0f, 116.0f}})
      DrawTree(renderer, {offset + tree.x, tree.y});

    DrawFlowerPatch(renderer, {offset + 690.0f, 184.0f});
    DrawFlowerPatch(renderer, {offset + 760.0f, 176.0f});
    DrawFlowerPatch(renderer, {offset + 930.0f, 292.0f});
  }
}
} // namespace

ProceduralGame::ProceduralGame() : world_(*gameInstance_.GetWorld()) {
  RegisterGameplayTypes();
  EnsureTownEntities();
}

void ProceduralGame::Initialize() {
  static_cast<void>(footstepSound_.Build(FootstepPatch));
  static_cast<void>(talkSound_.Build(TalkPatch));
  static_cast<void>(pokemonCrySound_.Build(PokemonCryPatch));
  static_cast<void>(footstepSound_.Upload());
  static_cast<void>(talkSound_.Upload());
  static_cast<void>(pokemonCrySound_.Upload());
}

void ProceduralGame::Shutdown() {
  pokemonCrySound_.Unload();
  talkSound_.Unload();
  footstepSound_.Unload();
}

void ProceduralGame::RegisterGameplayTypes() {
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<PocketTown::CharacterMotion>(
          "CharacterMotion"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<PocketTown::NpcWander>("NpcWander"));
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<PocketTown::PokemonRoam>("PokemonRoam"));

  world_.RegisterEntity({PocketTown::PlayerEntityType,
                         "TownPlayer",
                         {PocketTown::CharacterMotion::Type}});
  world_.RegisterEntity(
      {PocketTown::NpcEntityType, "TownNpc", {PocketTown::NpcWander::Type}});
  world_.RegisterEntity({PocketTown::PokemonEntityType,
                         "TownPokemon",
                         {PocketTown::PokemonRoam::Type}});
}

void ProceduralGame::EnsureTownEntities() {
  Entity *player = nullptr;
  int npcCount = 0;
  int pokemonCount = 0;
  for (const ObjectID id : world_.Entities()) {
    auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    if (entity->GetTypeID() == PocketTown::PlayerEntityType) {
      player = entity;
      playerEntity_ = ObjectRef(id);
    } else if (entity->GetTypeID() == PocketTown::NpcEntityType) {
      ++npcCount;
    } else if (entity->GetTypeID() == PocketTown::PokemonEntityType) {
      ++pokemonCount;
    }
  }

  if (!player) {
    playerEntity_ = world_.Spawn(PocketTown::PlayerEntityType, "TownPlayer");
    world_.FlushSpawns();
    player = playerEntity_.Resolve();
    player->transform.position = {640.0f, 390.0f};
  }
  playerMotion_ = player->GetComponent<PocketTown::CharacterMotion>();

  if (npcCount == 0) {
    (void)SpawnNpc(PocketTown::NpcRole::Elder, {720.0f, 286.0f}, 42.0f,
                   0x1144u);
    (void)SpawnNpc(PocketTown::NpcRole::Clerk, {930.0f, 386.0f}, 54.0f,
                   0x2718u);
    (void)SpawnNpc(PocketTown::NpcRole::Kid, {398.0f, 542.0f}, 70.0f,
                   0x3911u);
    (void)SpawnNpc(PocketTown::NpcRole::Fisher, {426.0f, 244.0f}, 36.0f,
                   0x5021u);
    npcCount = 4;
  }
  if (npcCount < 10) {
    (void)SpawnNpc(PocketTown::NpcRole::Clerk, {1530.0f, 386.0f}, 70.0f,
                   0x7111u);
    (void)SpawnNpc(PocketTown::NpcRole::Kid, {2050.0f, 548.0f}, 90.0f,
                   0x7222u);
    (void)SpawnNpc(PocketTown::NpcRole::Elder, {2680.0f, 300.0f}, 70.0f,
                   0x7333u);
    (void)SpawnNpc(PocketTown::NpcRole::Fisher, {3220.0f, 250.0f}, 70.0f,
                   0x7444u);
    (void)SpawnNpc(PocketTown::NpcRole::Clerk, {3860.0f, 410.0f}, 85.0f,
                   0x7555u);
    (void)SpawnNpc(PocketTown::NpcRole::Kid, {4540.0f, 540.0f}, 95.0f,
                   0x7666u);
  }

  if (pokemonCount == 0) {
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Leafling, {585.0f, 174.0f},
                       86.0f, 0x6111u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Aquabbit, {334.0f, 260.0f},
                       70.0f, 0x6222u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Embercub, {1018.0f, 420.0f},
                       92.0f, 0x6333u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Sparko, {846.0f, 650.0f},
                       76.0f, 0x6444u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Puffowl, {176.0f, 650.0f},
                       58.0f, 0x6555u);
    pokemonCount = 5;
  }
  if (pokemonCount < 14) {
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Leafling, {1450.0f, 190.0f},
                       95.0f, 0x8111u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Sparko, {1780.0f, 640.0f},
                       90.0f, 0x8222u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Aquabbit, {2320.0f, 250.0f},
                       85.0f, 0x8333u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Puffowl, {2810.0f, 600.0f},
                       80.0f, 0x8444u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Embercub, {3340.0f, 430.0f},
                       105.0f, 0x8555u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Aquabbit, {3820.0f, 230.0f},
                       90.0f, 0x8666u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Leafling, {4300.0f, 185.0f},
                       95.0f, 0x8777u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Sparko, {4770.0f, 620.0f},
                       75.0f, 0x8888u);
    (void)SpawnPokemon(PocketTown::PokemonSpecies::Puffowl, {4990.0f, 360.0f},
                       65.0f, 0x8999u);
  }
}

ObjectRef ProceduralGame::SpawnPokemon(PocketTown::PokemonSpecies species,
                                       Engine::Vector2 position, float radius,
                                       std::uint32_t seed) {
  const ObjectRef pokemon = world_.Spawn(
      PocketTown::PokemonEntityType,
      "Pokemon#" + std::to_string(gameInstance_.GetObjectManager()->LiveCount()));
  world_.FlushSpawns();
  auto *entity = pokemon.Resolve();
  entity->transform.position = position;
  entity->GetComponent<PocketTown::PokemonRoam>().Resolve()->Configure(
      species, position, radius, seed);
  return pokemon;
}

ObjectRef ProceduralGame::SpawnNpc(PocketTown::NpcRole role,
                                   Engine::Vector2 position, float radius,
                                   std::uint32_t seed) {
  const ObjectRef npc = world_.Spawn(
      PocketTown::NpcEntityType,
      "TownNpc#" + std::to_string(gameInstance_.GetObjectManager()->LiveCount()));
  world_.FlushSpawns();
  auto *entity = npc.Resolve();
  entity->transform.position = position;
  entity->GetComponent<PocketTown::NpcWander>().Resolve()->Configure(
      role, position, radius, seed);
  return npc;
}

void ProceduralGame::Update(const Engine::InputSystem &input, float deltaTime) {
  ENGINE_PROFILE_SCOPE("PocketTown Update");
  if (auto *motion = playerMotion_.Resolve())
    motion->SetCommand(inputBindings_.BuildPlayerCommand(input));

  world_.Update(deltaTime);
  UpdateFootsteps(deltaTime);
  UpdateNpcTalk(deltaTime);
  UpdatePokemonCries(deltaTime);
}

ObjectRef ProceduralGame::FindNearestTalker() const {
  const auto *player = playerEntity_.Resolve();
  if (!player)
    return {};

  ObjectRef nearest;
  float nearestDistance = 76.0f * 76.0f;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (!entity || entity->GetTypeID() != PocketTown::NpcEntityType)
      continue;
    const float distance = DistanceSquared(player->transform.position,
                                           entity->transform.position);
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = ObjectRef(id);
    }
  }
  return nearest;
}

void ProceduralGame::UpdateNpcTalk(float deltaTime) {
  const ObjectRef talker = FindNearestTalker();
  const bool sameTalker =
      talker.GetID().index == activeTalker_.GetID().index &&
      talker.GetID().version == activeTalker_.GetID().version;

  if (!talker.Resolve()) {
    activeTalker_ = {};
    talkBubbleTimer_ = 0.0f;
    talkSoundTimer_ = 0.0f;
    return;
  }

  activeTalker_ = talker;
  talkBubbleTimer_ = 0.35f;
  talkSoundTimer_ -= deltaTime;
  if (!sameTalker)
    talkSoundTimer_ = 0.0f;
  if (talkSoundTimer_ <= 0.0f) {
    const auto *entity = talker.Resolve();
    const float pan = entity ? std::clamp(entity->transform.position.x /
                                             static_cast<float>(GameConfig::WorldWidth),
                                         0.18f, 0.82f)
                             : 0.5f;
    const float pitch = 0.93f + static_cast<float>(talker.GetID().index % 5u) * 0.045f;
    talkSound_.Play(0.52f, pitch, pan);
    talkSoundTimer_ = 0.42f;
  }
}

ObjectRef ProceduralGame::FindNearestPokemon(float range) const {
  const auto *player = playerEntity_.Resolve();
  if (!player)
    return {};

  ObjectRef nearest;
  float nearestDistance = range * range;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (!entity || entity->GetTypeID() != PocketTown::PokemonEntityType)
      continue;
    const float distance = DistanceSquared(player->transform.position,
                                           entity->transform.position);
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = ObjectRef(id);
    }
  }
  return nearest;
}

void ProceduralGame::UpdatePokemonCries(float deltaTime) {
  pokemonCryTimer_ -= deltaTime;
  if (pokemonCryTimer_ > 0.0f)
    return;

  const ObjectRef pokemon = FindNearestPokemon(95.0f);
  const auto *entity = pokemon.Resolve();
  if (!entity) {
    pokemonCryTimer_ = 0.35f;
    return;
  }

  const auto *roam = entity->GetComponent<PocketTown::PokemonRoam>().Resolve();
  const float speciesPitch = roam ? static_cast<float>(static_cast<std::uint8_t>(roam->Species())) * 0.075f : 0.0f;
  const float pan = std::clamp(entity->transform.position.x /
                                   static_cast<float>(GameConfig::WorldWidth),
                               0.18f, 0.82f);
  pokemonCrySound_.Play(0.38f, 0.9f + speciesPitch, pan);
  pokemonCryTimer_ = 1.2f;
}

void ProceduralGame::UpdateFootsteps(float deltaTime) {
  auto *motion = playerMotion_.Resolve();
  if (!motion || !motion->IsMoving()) {
    footstepTimer_ = 0.0f;
    return;
  }

  footstepTimer_ -= deltaTime;
  if (footstepTimer_ <= 0.0f) {
    const float pan = 0.5f + std::clamp(motion->Velocity().x / 300.0f, -0.25f, 0.25f);
    footstepSound_.Play(0.5f, 0.96f + std::sin(motion->WalkCycle()) * 0.035f, pan);
    footstepTimer_ = 0.28f;
  }
}

Engine::Color ProceduralGame::GetClearColor() const {
  return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext &context) const {
  ENGINE_PROFILE_SCOPE("PocketTown Render");
  Engine::Renderer2D &renderer = context.Draw2D();
  const auto *playerForCamera = playerEntity_.Resolve();
  const float cameraX = playerForCamera
                            ? std::clamp(playerForCamera->transform.position.x -
                                             static_cast<float>(renderer.GetWidth()) * 0.5f,
                                         0.0f,
                                         static_cast<float>(GameConfig::WorldWidth -
                                                            GameConfig::PlayAreaWidth))
                            : 0.0f;
  const auto toScreen = [cameraX](Engine::Vector2 worldPosition) {
    return Engine::Vector2{worldPosition.x - cameraX, worldPosition.y};
  };

  DrawTown(renderer, cameraX);

  struct DrawItem {
    Engine::Vector2 position;
    Engine::Color shirt;
    PocketTown::Direction facing;
    PocketTown::PokemonSpecies species;
    float cycle;
    bool moving;
    bool player;
    bool pokemon;
  };
  std::vector<DrawItem> characters;
  characters.reserve(8);
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (!entity)
      continue;
    if (entity->GetTypeID() == PocketTown::PlayerEntityType) {
      const auto *motion = entity->GetComponent<PocketTown::CharacterMotion>().Resolve();
      characters.push_back({toScreen(entity->transform.position), {72, 120, 212, 255},
                            motion ? motion->Facing() : PocketTown::Direction::Down,
                            PocketTown::PokemonSpecies::Leafling,
                            motion ? motion->WalkCycle() : 0.0f,
                            motion ? motion->IsMoving() : false, true, false});
    } else if (entity->GetTypeID() == PocketTown::NpcEntityType) {
      const auto *wander = entity->GetComponent<PocketTown::NpcWander>().Resolve();
      characters.push_back({toScreen(entity->transform.position),
                            wander ? ShirtColor(wander->Role())
                                   : Engine::Color{129, 78, 154, 255},
                            wander ? wander->Facing() : PocketTown::Direction::Down,
                            PocketTown::PokemonSpecies::Leafling,
                            wander ? wander->WalkCycle() : 0.0f,
                            wander ? wander->IsMoving() : false, false, false});
    } else if (entity->GetTypeID() == PocketTown::PokemonEntityType) {
      const auto *roam = entity->GetComponent<PocketTown::PokemonRoam>().Resolve();
      characters.push_back({toScreen(entity->transform.position), {255, 255, 255, 255},
                            roam ? roam->Facing() : PocketTown::Direction::Down,
                            roam ? roam->Species() : PocketTown::PokemonSpecies::Leafling,
                            roam ? roam->BobCycle() : 0.0f,
                            roam ? roam->IsMoving() : false, false, true});
    }
  }
  std::sort(characters.begin(), characters.end(),
            [](const DrawItem &left, const DrawItem &right) {
              return left.position.y < right.position.y;
            });
  for (const DrawItem &item : characters) {
    if (item.pokemon)
      DrawPokemon(renderer, item.position, item.species, item.facing, item.cycle,
                  item.moving);
    else
      DrawCharacter(renderer, item.position, item.shirt, item.facing, item.cycle,
                    item.moving, item.player);
  }

  if (talkBubbleTimer_ > 0.0f) {
    const auto *talker = activeTalker_.Resolve();
    const auto *wander = talker ? talker->GetComponent<PocketTown::NpcWander>().Resolve() : nullptr;
    if (talker && wander)
      DrawSpeechBubble(renderer, toScreen(talker->transform.position), TalkText(wander->Role()));
  }

  const ObjectRef nearbyPokemon = FindNearestPokemon(95.0f);
  const auto *pokemon = nearbyPokemon.Resolve();
  const auto *roam = pokemon ? pokemon->GetComponent<PocketTown::PokemonRoam>().Resolve() : nullptr;
  if (pokemon && roam)
    DrawSpeechBubble(renderer, toScreen(pokemon->transform.position),
                     PokemonTalkText(roam->Species()));

  const int zone = static_cast<int>(cameraX / static_cast<float>(GameConfig::PlayAreaWidth)) + 1;
  renderer.DrawText("ARROW KEYS: MOVE   AREA " + std::to_string(zone) + "/4",
                    {24.0f, 22.0f}, 20, {31, 57, 54, 230});
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
  playerMotion_ = {};
  activeTalker_ = {};
  talkBubbleTimer_ = 0.0f;
  talkSoundTimer_ = 0.0f;
  pokemonCryTimer_ = 0.0f;
  EnsureTownEntities();
}

#if defined(ENGINE_AUTOTESTS)
void ProceduralGame::SerializeAutoTestState(Engine::Serializer &serializer) {
  const auto *player = playerEntity_.Resolve();
  int npcs = 0;
  int pokemon = 0;
  for (const ObjectID id : world_.Entities()) {
    const auto *entity = ObjectRef(id).Resolve();
    if (entity && entity->GetTypeID() == PocketTown::NpcEntityType)
      ++npcs;
    if (entity && entity->GetTypeID() == PocketTown::PokemonEntityType)
      ++pokemon;
  }
  float x = player ? player->transform.position.x : 0.0f;
  float y = player ? player->transform.position.y : 0.0f;
  serializer.Value("player.position.x", x);
  serializer.Value("player.position.y", y);
  int entityIndex = static_cast<int>(playerEntity_.GetID().index);
  int entityVersion = static_cast<int>(playerEntity_.GetID().version);
  int objectCount =
      static_cast<int>(gameInstance_.GetObjectManager()->LiveCount());
  serializer.Value("player.entity.index", entityIndex);
  serializer.Value("player.entity.version", entityVersion);
  serializer.Value("world.npcCount", npcs);
  serializer.Value("world.pokemonCount", pokemon);
  serializer.Value("world.objectCount", objectCount);
}
#endif

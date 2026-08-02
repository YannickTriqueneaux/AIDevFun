#pragma once

#include "Engine/Audio/ProceduralAudio.h"

#include <array>
#include <cstddef>

struct AudioResources {
  void Initialize();
  void Update();
  void SetMasterVolume(float volume);
  [[nodiscard]] float GetMasterVolume() const;
  void PlayFootstep(bool leftFoot) const;
  void PlayNpcVoice(std::size_t index) const;
  void PlayDragonVoice(float volume = 1.0f, float pitch = 1.0f) const;
  void PlayDragonFlame(float volume = 1.0f, float pan = 0.5f) const;
  void PlayDragonFlameBurst(float volume = 1.0f, float pan = 0.5f) const;
  void PlayCarPass(float volume = 1.0f, float pan = 0.5f, float pitch = 1.0f) const;
  void PlayKnightSpawn(float pan = 0.5f) const;
  void PlayKnightStrike(float pan = 0.5f) const;
  void PlayKnightDespawn(float pan = 0.5f) const;
  void Shutdown();

  Engine::ProceduralMusic music;
  Engine::ProceduralSound footstep;
  Engine::ProceduralSound dragonVoice;
  Engine::ProceduralSound dragonFlame;
  Engine::ProceduralSound dragonFlameBurst;
  Engine::ProceduralSound carPass;
  Engine::ProceduralSound knightSpawn;
  Engine::ProceduralSound knightStrike;
  Engine::ProceduralSound knightDespawn;
  std::array<Engine::ProceduralSound, 4> npcVoices;
  float masterVolume = 1.0f;
  bool ready = false;
};

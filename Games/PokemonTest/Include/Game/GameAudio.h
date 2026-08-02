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
  void Shutdown();

  Engine::ProceduralMusic music;
  Engine::ProceduralSound footstep;
  std::array<Engine::ProceduralSound, 4> npcVoices;
  float masterVolume = 1.0f;
  bool ready = false;
};

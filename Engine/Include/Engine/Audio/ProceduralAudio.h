#pragma once

#include "Engine/Core/Export.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Engine {
enum class AudioWaveform { Sine, Square, Triangle, Saw, Noise };
enum class AudioFilterType { None, LowPass, HighPass };

struct AudioEnvelopePoint {
  float timeSeconds = 0.0f;
  float value = 0.0f;
};

struct AudioEnvelope {
  std::span<const AudioEnvelopePoint> points;
  float defaultValue = 1.0f;
};

struct AudioAdsr {
  float attackSeconds = 0.0f;
  float decaySeconds = 0.0f;
  float sustainLevel = 1.0f;
  float releaseSeconds = 0.0f;
};

struct ProceduralSoundVoice {
  AudioWaveform waveform = AudioWaveform::Sine;
  float frequencyHz = 440.0f;
  float volume = 1.0f;
  float pan = 0.5f;
  float phase = 0.0f;
  float dutyCycle = 0.5f;
  float detuneCents = 0.0f;
  AudioAdsr envelope;
  AudioEnvelope frequencyMultiplier;
  AudioEnvelope volumeMultiplier;
  float vibratoFrequencyHz = 0.0f;
  float vibratoDepthCents = 0.0f;
  float tremoloFrequencyHz = 0.0f;
  float tremoloDepth = 0.0f;
  AudioFilterType filter = AudioFilterType::None;
  float filterCutoffHz = 20'000.0f;
  std::uint32_t noiseSeed = 1;
};

struct ProceduralAudioEffects {
  float echoDelaySeconds = 0.0f;
  float echoFeedback = 0.0f;
  float echoMix = 0.0f;
  float reverbSeconds = 0.0f;
  float reverbDecay = 0.0f;
  float reverbMix = 0.0f;
};

struct ProceduralSoundDefinition {
  float durationSeconds = 0.25f;
  std::uint32_t sampleRate = 44'100;
  float masterVolume = 0.8f;
  std::span<const ProceduralSoundVoice> voices;
  ProceduralAudioEffects effects;
};

class ENGINE_API ProceduralSound {
public:
  ProceduralSound();
  ~ProceduralSound();
  ProceduralSound(const ProceduralSound &) = delete;
  ProceduralSound &operator=(const ProceduralSound &) = delete;
  ProceduralSound(ProceduralSound &&other) noexcept;
  ProceduralSound &operator=(ProceduralSound &&other) noexcept;

  [[nodiscard]] bool Build(const ProceduralSoundDefinition &definition);
  [[nodiscard]] bool Upload();
  void Unload();
  void Play(float volume = 1.0f, float pitch = 1.0f, float pan = 0.5f) const;
  void Stop() const;

  [[nodiscard]] bool IsBuilt() const;
  [[nodiscard]] bool IsUploaded() const;
  [[nodiscard]] bool IsPlaying() const;
  [[nodiscard]] float GetDuration() const;
  [[nodiscard]] std::size_t GetSampleCount() const;
  [[nodiscard]] float GetPeakAmplitude() const;
  [[nodiscard]] std::uint64_t GetPcmHash() const;

private:
  struct Implementation;
  Implementation *implementation_ = nullptr;
};

struct ProceduralMusicNote {
  float startBeat = 0.0f;
  float durationBeats = 1.0f;
  int midiNote = 60;
  float velocity = 1.0f;
};

struct ProceduralMusicTrack {
  const ProceduralSoundDefinition *instrument = nullptr;
  std::span<const ProceduralMusicNote> notes;
  float volume = 1.0f;
  float pan = 0.5f;
};

struct ProceduralMusicDefinition {
  float tempoBeatsPerMinute = 120.0f;
  float lengthBeats = 4.0f;
  std::uint32_t sampleRate = 44'100;
  float masterVolume = 0.8f;
  std::span<const ProceduralMusicTrack> tracks;
  ProceduralAudioEffects effects;
};

class ENGINE_API ProceduralMusic {
public:
  ProceduralMusic();
  ~ProceduralMusic();
  ProceduralMusic(const ProceduralMusic &) = delete;
  ProceduralMusic &operator=(const ProceduralMusic &) = delete;
  ProceduralMusic(ProceduralMusic &&other) noexcept;
  ProceduralMusic &operator=(ProceduralMusic &&other) noexcept;

  [[nodiscard]] bool Build(const ProceduralMusicDefinition &definition);
  [[nodiscard]] bool Upload();
  void Unload();
  void Play(bool loop = true);
  void Update();
  void Stop();
  void Pause();
  void Resume();
  void Seek(float seconds);
  void SetVolume(float volume);
  void SetPitch(float pitch);
  void SetPan(float pan);

  [[nodiscard]] bool IsBuilt() const;
  [[nodiscard]] bool IsUploaded() const;
  [[nodiscard]] bool IsPlaying() const;
  [[nodiscard]] float GetDuration() const;
  [[nodiscard]] float GetPlaybackTime() const;
  [[nodiscard]] std::size_t GetSampleCount() const;
  [[nodiscard]] float GetPeakAmplitude() const;
  [[nodiscard]] std::uint64_t GetPcmHash() const;

private:
  struct Implementation;
  Implementation *implementation_ = nullptr;
};
} // namespace Engine

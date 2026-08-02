#include "Engine/Audio/ProceduralAudio.h"

#include "Engine/Core/Memory.h"

#include "raylib.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;
constexpr std::uint64_t HashOffset = 1469598103934665603ULL;
constexpr std::uint64_t HashPrime = 1099511628211ULL;

struct RenderedAudio {
  std::uint32_t sampleRate = 0;
  std::vector<float> samples;
  float peak = 0.0f;
  std::uint64_t hash = 0;
};

float ClampUnit(float value) { return std::clamp(value, -1.0f, 1.0f); }

float SampleEnvelope(const Engine::AudioEnvelope &envelope, float time) {
  if (envelope.points.empty())
    return envelope.defaultValue;
  if (time <= envelope.points.front().timeSeconds)
    return envelope.points.front().value;
  for (std::size_t index = 1; index < envelope.points.size(); ++index) {
    const auto &previous = envelope.points[index - 1];
    const auto &next = envelope.points[index];
    if (time <= next.timeSeconds) {
      const float range = next.timeSeconds - previous.timeSeconds;
      const float amount =
          range <= 0.0f ? 0.0f : (time - previous.timeSeconds) / range;
      return previous.value + (next.value - previous.value) * amount;
    }
  }
  return envelope.points.back().value;
}

float SampleAdsr(const Engine::AudioAdsr &envelope, float time,
                 float duration) {
  const float attack = std::max(0.0f, envelope.attackSeconds);
  const float decay = std::max(0.0f, envelope.decaySeconds);
  const float release = std::max(0.0f, envelope.releaseSeconds);
  const float sustain = std::clamp(envelope.sustainLevel, 0.0f, 1.0f);
  if (attack > 0.0f && time < attack)
    return time / attack;
  if (decay > 0.0f && time < attack + decay) {
    const float amount = (time - attack) / decay;
    return 1.0f + (sustain - 1.0f) * amount;
  }
  const float releaseStart = std::max(attack + decay, duration - release);
  if (release > 0.0f && time >= releaseStart)
    return sustain * std::clamp((duration - time) / std::max(release, 0.0001f),
                                0.0f, 1.0f);
  return sustain;
}

float Oscillator(Engine::AudioWaveform waveform, float phase, float dutyCycle,
                 std::uint32_t &noiseState) {
  switch (waveform) {
  case Engine::AudioWaveform::Sine:
    return std::sin(phase * TwoPi);
  case Engine::AudioWaveform::Square:
    return phase < std::clamp(dutyCycle, 0.01f, 0.99f) ? 1.0f : -1.0f;
  case Engine::AudioWaveform::Triangle:
    return 1.0f - 4.0f * std::abs(phase - 0.5f);
  case Engine::AudioWaveform::Saw:
    return phase * 2.0f - 1.0f;
  case Engine::AudioWaveform::Noise:
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return static_cast<float>(noiseState & 0xffffU) / 32767.5f - 1.0f;
  }
  return 0.0f;
}

struct FilterState {
  float lowLeft = 0.0f;
  float lowRight = 0.0f;
  float previousLeft = 0.0f;
  float previousRight = 0.0f;
};

void ApplyFilter(Engine::AudioFilterType type, float cutoff,
                 std::uint32_t sampleRate, FilterState &state, float &left,
                 float &right) {
  if (type == Engine::AudioFilterType::None)
    return;
  const float safeCutoff =
      std::clamp(cutoff, 10.0f, static_cast<float>(sampleRate) * 0.45f);
  const float dt = 1.0f / static_cast<float>(sampleRate);
  const float rc = 1.0f / (TwoPi * safeCutoff);
  if (type == Engine::AudioFilterType::LowPass) {
    const float alpha = dt / (rc + dt);
    state.lowLeft += alpha * (left - state.lowLeft);
    state.lowRight += alpha * (right - state.lowRight);
    left = state.lowLeft;
    right = state.lowRight;
  } else {
    const float alpha = rc / (rc + dt);
    const float filteredLeft =
        alpha * (state.lowLeft + left - state.previousLeft);
    const float filteredRight =
        alpha * (state.lowRight + right - state.previousRight);
    state.previousLeft = left;
    state.previousRight = right;
    state.lowLeft = filteredLeft;
    state.lowRight = filteredRight;
    left = filteredLeft;
    right = filteredRight;
  }
}

void ApplyEffects(std::vector<float> &samples, std::uint32_t sampleRate,
                  const Engine::ProceduralAudioEffects &effects) {
  const std::size_t frames = samples.size() / 2;
  const auto mixDelay = [&](float delaySeconds, float feedback, float mix) {
    if (delaySeconds <= 0.0f || mix <= 0.0f)
      return;
    const std::size_t delay =
        static_cast<std::size_t>(delaySeconds * static_cast<float>(sampleRate));
    if (delay == 0 || delay >= frames)
      return;
    const float safeFeedback = std::clamp(feedback, 0.0f, 0.95f);
    const float safeMix = std::clamp(mix, 0.0f, 1.0f);
    for (std::size_t frame = delay; frame < frames; ++frame) {
      for (std::size_t channel = 0; channel < 2; ++channel) {
        const std::size_t index = frame * 2 + channel;
        const float delayed = samples[(frame - delay) * 2 + channel];
        samples[index] += delayed * safeMix;
        samples[index] += delayed * safeFeedback * safeMix;
      }
    }
  };
  mixDelay(effects.echoDelaySeconds, effects.echoFeedback, effects.echoMix);
  if (effects.reverbSeconds > 0.0f && effects.reverbMix > 0.0f) {
    const float base = effects.reverbSeconds;
    mixDelay(base * 0.37f, effects.reverbDecay * 0.65f,
             effects.reverbMix * 0.45f);
    mixDelay(base * 0.61f, effects.reverbDecay * 0.45f,
             effects.reverbMix * 0.30f);
    mixDelay(base * 0.89f, effects.reverbDecay * 0.30f,
             effects.reverbMix * 0.20f);
  }
}

void Finalize(RenderedAudio &audio) {
  float peak = 0.0f;
  for (float sample : audio.samples)
    peak = std::max(peak, std::abs(sample));
  if (peak > 1.0f) {
    const float inverse = 1.0f / peak;
    for (float &sample : audio.samples)
      sample *= inverse;
    peak = 1.0f;
  }
  audio.peak = peak;
  std::uint64_t hash = HashOffset;
  for (float sample : audio.samples) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);
    for (int byte = 0; byte < 4; ++byte) {
      hash ^= (bits >> (byte * 8)) & 0xffU;
      hash *= HashPrime;
    }
  }
  audio.hash = hash;
}

RenderedAudio RenderSound(const Engine::ProceduralSoundDefinition &definition,
                          float durationOverride = 0.0f,
                          float frequencyScale = 1.0f, float velocity = 1.0f,
                          float panOverride = -1.0f, bool applyEffects = true) {
  RenderedAudio audio;
  audio.sampleRate = std::clamp(definition.sampleRate, 8'000U, 192'000U);
  const float duration = std::clamp(
      durationOverride > 0.0f ? durationOverride : definition.durationSeconds,
      0.001f, 600.0f);
  const std::size_t frames = static_cast<std::size_t>(
      std::ceil(duration * static_cast<float>(audio.sampleRate)));
  audio.samples.assign(frames * 2, 0.0f);

  for (const Engine::ProceduralSoundVoice &voice : definition.voices) {
    float phase = voice.phase - std::floor(voice.phase);
    std::uint32_t noiseState = voice.noiseSeed == 0 ? 1 : voice.noiseSeed;
    FilterState filter;
    const float detune = std::pow(2.0f, voice.detuneCents / 1200.0f);
    const float pan =
        std::clamp(panOverride >= 0.0f ? panOverride : voice.pan, 0.0f, 1.0f);
    const float leftGain = std::cos(pan * Pi * 0.5f);
    const float rightGain = std::sin(pan * Pi * 0.5f);
    for (std::size_t frame = 0; frame < frames; ++frame) {
      const float time =
          static_cast<float>(frame) / static_cast<float>(audio.sampleRate);
      const float vibrato =
          voice.vibratoDepthCents == 0.0f
              ? 1.0f
              : std::pow(2.0f,
                         std::sin(time * voice.vibratoFrequencyHz * TwoPi) *
                             voice.vibratoDepthCents / 1200.0f);
      const float frequency =
          std::max(0.0f, voice.frequencyHz * frequencyScale * detune * vibrato *
                             SampleEnvelope(voice.frequencyMultiplier, time));
      const float tremolo =
          1.0f - std::clamp(voice.tremoloDepth, 0.0f, 1.0f) * 0.5f +
          std::sin(time * voice.tremoloFrequencyHz * TwoPi) *
              std::clamp(voice.tremoloDepth, 0.0f, 1.0f) * 0.5f;
      const float amplitude =
          voice.volume * velocity * definition.masterVolume *
          SampleAdsr(voice.envelope, time, duration) *
          SampleEnvelope(voice.volumeMultiplier, time) * tremolo;
      const float value =
          Oscillator(voice.waveform, phase, voice.dutyCycle, noiseState) *
          amplitude;
      float left = value * leftGain;
      float right = value * rightGain;
      ApplyFilter(voice.filter, voice.filterCutoffHz, audio.sampleRate, filter,
                  left, right);
      audio.samples[frame * 2] += left;
      audio.samples[frame * 2 + 1] += right;
      phase += frequency / static_cast<float>(audio.sampleRate);
      phase -= std::floor(phase);
    }
  }
  if (applyEffects)
    ApplyEffects(audio.samples, audio.sampleRate, definition.effects);
  Finalize(audio);
  return audio;
}

void Write16(std::vector<unsigned char> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<unsigned char>(value));
  bytes.push_back(static_cast<unsigned char>(value >> 8));
}

void Write32(std::vector<unsigned char> &bytes, std::uint32_t value) {
  Write16(bytes, static_cast<std::uint16_t>(value));
  Write16(bytes, static_cast<std::uint16_t>(value >> 16));
}

std::vector<unsigned char> MakeWaveFile(const RenderedAudio &audio) {
  const std::uint32_t dataSize =
      static_cast<std::uint32_t>(audio.samples.size() * sizeof(std::int16_t));
  std::vector<unsigned char> bytes;
  bytes.reserve(44 + dataSize);
  const auto text = [&bytes](const char *value) {
    bytes.insert(bytes.end(), value, value + 4);
  };
  text("RIFF");
  Write32(bytes, 36 + dataSize);
  text("WAVE");
  text("fmt ");
  Write32(bytes, 16);
  Write16(bytes, 1);
  Write16(bytes, 2);
  Write32(bytes, audio.sampleRate);
  Write32(bytes, audio.sampleRate * 4);
  Write16(bytes, 4);
  Write16(bytes, 16);
  text("data");
  Write32(bytes, dataSize);
  for (float sample : audio.samples) {
    const auto pcm =
        static_cast<std::int16_t>(std::round(ClampUnit(sample) * 32767.0f));
    Write16(bytes, static_cast<std::uint16_t>(pcm));
  }
  return bytes;
}

RenderedAudio RenderMusic(const Engine::ProceduralMusicDefinition &definition) {
  RenderedAudio music;
  music.sampleRate = std::clamp(definition.sampleRate, 8'000U, 192'000U);
  const float secondsPerBeat =
      60.0f / std::clamp(definition.tempoBeatsPerMinute, 1.0f, 1'000.0f);
  const float duration =
      std::clamp(definition.lengthBeats * secondsPerBeat, 0.001f, 3'600.0f);
  const std::size_t frames = static_cast<std::size_t>(
      std::ceil(duration * static_cast<float>(music.sampleRate)));
  music.samples.assign(frames * 2, 0.0f);

  for (const Engine::ProceduralMusicTrack &track : definition.tracks) {
    if (track.instrument == nullptr)
      continue;
    for (const Engine::ProceduralMusicNote &note : track.notes) {
      if (note.durationBeats <= 0.0f)
        continue;
      Engine::ProceduralSoundDefinition instrument = *track.instrument;
      instrument.sampleRate = music.sampleRate;
      const float noteDuration = note.durationBeats * secondsPerBeat;
      const float frequencyScale =
          std::pow(2.0f, (static_cast<float>(note.midiNote) - 69.0f) / 12.0f);
      RenderedAudio rendered =
          RenderSound(instrument, noteDuration, frequencyScale,
                      std::clamp(note.velocity * track.volume, 0.0f, 4.0f),
                      track.pan, false);
      const std::size_t startFrame = static_cast<std::size_t>(
          std::max(0.0f, note.startBeat) * secondsPerBeat * music.sampleRate);
      const std::size_t available =
          startFrame >= frames ? 0 : frames - startFrame;
      const std::size_t noteFrames =
          std::min(available, rendered.samples.size() / 2);
      for (std::size_t frame = 0; frame < noteFrames; ++frame) {
        music.samples[(startFrame + frame) * 2] += rendered.samples[frame * 2];
        music.samples[(startFrame + frame) * 2 + 1] +=
            rendered.samples[frame * 2 + 1];
      }
    }
  }
  for (float &sample : music.samples)
    sample *= definition.masterVolume;
  ApplyEffects(music.samples, music.sampleRate, definition.effects);
  Finalize(music);
  return music;
}
} // namespace

namespace Engine {
struct ProceduralSound::Implementation {
  RenderedAudio audio;
  Sound sound{};
  bool uploaded = false;
};

struct ProceduralMusic::Implementation {
  RenderedAudio audio;
  std::vector<unsigned char> waveFile;
  Music music{};
  bool uploaded = false;
};

ProceduralSound::ProceduralSound()
    : implementation_(NEW_MEMORY(Implementation).release()) {}
ProceduralSound::~ProceduralSound() {
  Unload();
  DELETE_MEMORY(implementation_);
}
ProceduralSound::ProceduralSound(ProceduralSound &&other) noexcept
    : implementation_(std::exchange(other.implementation_, nullptr)) {}
ProceduralSound &ProceduralSound::operator=(ProceduralSound &&other) noexcept {
  if (this != &other) {
    Unload();
    DELETE_MEMORY(implementation_);
    implementation_ = std::exchange(other.implementation_, nullptr);
  }
  return *this;
}
bool ProceduralSound::Build(const ProceduralSoundDefinition &definition) {
  if (!implementation_)
    implementation_ = NEW_MEMORY(Implementation).release();
  Unload();
  implementation_->audio = RenderSound(definition);
  return IsBuilt();
}
bool ProceduralSound::Upload() {
  if (!IsBuilt() || !IsAudioDeviceReady())
    return false;
  if (implementation_->uploaded)
    return true;
  Wave wave{.frameCount = static_cast<unsigned int>(GetSampleCount()),
            .sampleRate = implementation_->audio.sampleRate,
            .sampleSize = 32,
            .channels = 2,
            .data = implementation_->audio.samples.data()};
  implementation_->sound = LoadSoundFromWave(wave);
  implementation_->uploaded = IsSoundValid(implementation_->sound);
  return implementation_->uploaded;
}
void ProceduralSound::Unload() {
  if (!implementation_)
    return;
  if (implementation_->uploaded)
    UnloadSound(implementation_->sound);
  implementation_->sound = {};
  implementation_->uploaded = false;
  implementation_->audio = {};
}
void ProceduralSound::Play(float volume, float pitch, float pan) const {
  if (!IsUploaded())
    return;
  SetSoundVolume(implementation_->sound, std::clamp(volume, 0.0f, 1.0f));
  SetSoundPitch(implementation_->sound, std::max(0.01f, pitch));
  SetSoundPan(implementation_->sound, std::clamp(pan, 0.0f, 1.0f));
  PlaySound(implementation_->sound);
}
void ProceduralSound::Stop() const {
  if (IsUploaded())
    StopSound(implementation_->sound);
}
bool ProceduralSound::IsBuilt() const {
  return implementation_ && !implementation_->audio.samples.empty();
}
bool ProceduralSound::IsUploaded() const {
  return implementation_ && implementation_->uploaded;
}
bool ProceduralSound::IsPlaying() const {
  return IsUploaded() && IsSoundPlaying(implementation_->sound);
}
float ProceduralSound::GetDuration() const {
  return IsBuilt() ? static_cast<float>(GetSampleCount()) /
                         static_cast<float>(implementation_->audio.sampleRate)
                   : 0.0f;
}
std::size_t ProceduralSound::GetSampleCount() const {
  return IsBuilt() ? implementation_->audio.samples.size() / 2 : 0;
}
float ProceduralSound::GetPeakAmplitude() const {
  return IsBuilt() ? implementation_->audio.peak : 0.0f;
}
std::uint64_t ProceduralSound::GetPcmHash() const {
  return IsBuilt() ? implementation_->audio.hash : 0;
}

ProceduralMusic::ProceduralMusic()
    : implementation_(NEW_MEMORY(Implementation).release()) {}
ProceduralMusic::~ProceduralMusic() {
  Unload();
  DELETE_MEMORY(implementation_);
}
ProceduralMusic::ProceduralMusic(ProceduralMusic &&other) noexcept
    : implementation_(std::exchange(other.implementation_, nullptr)) {}
ProceduralMusic &ProceduralMusic::operator=(ProceduralMusic &&other) noexcept {
  if (this != &other) {
    Unload();
    DELETE_MEMORY(implementation_);
    implementation_ = std::exchange(other.implementation_, nullptr);
  }
  return *this;
}
bool ProceduralMusic::Build(const ProceduralMusicDefinition &definition) {
  if (!implementation_)
    implementation_ = NEW_MEMORY(Implementation).release();
  Unload();
  implementation_->audio = RenderMusic(definition);
  return IsBuilt();
}
bool ProceduralMusic::Upload() {
  if (!IsBuilt() || !IsAudioDeviceReady())
    return false;
  if (implementation_->uploaded)
    return true;
  implementation_->waveFile = MakeWaveFile(implementation_->audio);
  implementation_->music = LoadMusicStreamFromMemory(
      ".wav", implementation_->waveFile.data(),
      static_cast<int>(implementation_->waveFile.size()));
  implementation_->uploaded = IsMusicValid(implementation_->music);
  if (implementation_->uploaded)
    implementation_->music.looping = true;
  return implementation_->uploaded;
}
void ProceduralMusic::Unload() {
  if (!implementation_)
    return;
  if (implementation_->uploaded)
    UnloadMusicStream(implementation_->music);
  implementation_->music = {};
  implementation_->uploaded = false;
  implementation_->waveFile.clear();
  implementation_->audio = {};
}
void ProceduralMusic::Play(bool loop) {
  if (!IsUploaded())
    return;
  implementation_->music.looping = loop;
  PlayMusicStream(implementation_->music);
}
void ProceduralMusic::Update() {
  if (IsUploaded())
    UpdateMusicStream(implementation_->music);
}
void ProceduralMusic::Stop() {
  if (IsUploaded())
    StopMusicStream(implementation_->music);
}
void ProceduralMusic::Pause() {
  if (IsUploaded())
    PauseMusicStream(implementation_->music);
}
void ProceduralMusic::Resume() {
  if (IsUploaded())
    ResumeMusicStream(implementation_->music);
}
void ProceduralMusic::Seek(float seconds) {
  if (IsUploaded())
    SeekMusicStream(implementation_->music,
                    std::clamp(seconds, 0.0f, GetDuration()));
}
void ProceduralMusic::SetVolume(float volume) {
  if (IsUploaded())
    SetMusicVolume(implementation_->music, std::clamp(volume, 0.0f, 1.0f));
}
void ProceduralMusic::SetPitch(float pitch) {
  if (IsUploaded())
    SetMusicPitch(implementation_->music, std::max(0.01f, pitch));
}
void ProceduralMusic::SetPan(float pan) {
  if (IsUploaded())
    SetMusicPan(implementation_->music, std::clamp(pan, 0.0f, 1.0f));
}
bool ProceduralMusic::IsBuilt() const {
  return implementation_ && !implementation_->audio.samples.empty();
}
bool ProceduralMusic::IsUploaded() const {
  return implementation_ && implementation_->uploaded;
}
bool ProceduralMusic::IsPlaying() const {
  return IsUploaded() && IsMusicStreamPlaying(implementation_->music);
}
float ProceduralMusic::GetDuration() const {
  return IsBuilt() ? static_cast<float>(GetSampleCount()) /
                         static_cast<float>(implementation_->audio.sampleRate)
                   : 0.0f;
}
float ProceduralMusic::GetPlaybackTime() const {
  return IsUploaded() ? GetMusicTimePlayed(implementation_->music) : 0.0f;
}
std::size_t ProceduralMusic::GetSampleCount() const {
  return IsBuilt() ? implementation_->audio.samples.size() / 2 : 0;
}
float ProceduralMusic::GetPeakAmplitude() const {
  return IsBuilt() ? implementation_->audio.peak : 0.0f;
}
std::uint64_t ProceduralMusic::GetPcmHash() const {
  return IsBuilt() ? implementation_->audio.hash : 0;
}
} // namespace Engine

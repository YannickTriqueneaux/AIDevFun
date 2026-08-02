#include "Game/GameAudio.h"

#include <algorithm>
#include <array>

namespace {
static constexpr std::array MusicLeadVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Square,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.22f,
                                 .pan = 0.5f,
                                 .dutyCycle = 0.42f,
                                 .envelope = {.attackSeconds = 0.01f,
                                              .decaySeconds = 0.04f,
                                              .sustainLevel = 0.78f,
                                              .releaseSeconds = 0.07f},
                                 .vibratoFrequencyHz = 5.0f,
                                 .vibratoDepthCents = 8.0f,
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 3800.0f},
};
static constexpr Engine::ProceduralSoundDefinition MusicLeadPatch{
    .durationSeconds = 0.5f,
    .masterVolume = 0.55f,
    .voices = MusicLeadVoices,
};

static constexpr std::array MusicBassVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.46f,
                                 .pan = 0.48f,
                                 .envelope = {.attackSeconds = 0.004f,
                                              .decaySeconds = 0.045f,
                                              .sustainLevel = 0.72f,
                                              .releaseSeconds = 0.06f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 1200.0f},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Sine,
                                 .frequencyHz = 220.0f,
                                 .volume = 0.34f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.008f,
                                              .decaySeconds = 0.05f,
                                              .sustainLevel = 0.86f,
                                              .releaseSeconds = 0.08f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 520.0f},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Saw,
                                 .frequencyHz = 880.0f,
                                 .volume = 0.13f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.006f,
                                              .decaySeconds = 0.045f,
                                              .sustainLevel = 0.58f,
                                              .releaseSeconds = 0.05f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 950.0f},
};
static constexpr Engine::ProceduralSoundDefinition MusicBassPatch{
    .durationSeconds = 0.5f,
    .masterVolume = 0.82f,
    .voices = MusicBassVoices,
};

static constexpr std::array MusicPadVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Sine,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.13f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.18f,
                                              .decaySeconds = 0.08f,
                                              .sustainLevel = 0.72f,
                                              .releaseSeconds = 0.22f},
                                 .vibratoFrequencyHz = 2.2f,
                                 .vibratoDepthCents = 6.0f},
};
static constexpr Engine::ProceduralSoundDefinition MusicPadPatch{
    .durationSeconds = 1.8f,
    .masterVolume = 0.45f,
    .voices = MusicPadVoices,
};

static constexpr std::array KickPitch{
    Engine::AudioEnvelopePoint{0.0f, 1.85f},
    Engine::AudioEnvelopePoint{0.045f, 0.82f},
    Engine::AudioEnvelopePoint{0.18f, 0.52f},
};
static constexpr std::array KickVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Sine,
                                 .frequencyHz = 92.0f,
                                 .volume = 1.0f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.001f,
                                              .decaySeconds = 0.16f,
                                              .sustainLevel = 0.08f,
                                              .releaseSeconds = 0.13f},
                                 .frequencyMultiplier = {.points = KickPitch},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 620.0f},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.10f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.001f,
                                              .decaySeconds = 0.018f,
                                              .sustainLevel = 0.0f,
                                              .releaseSeconds = 0.018f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 1200.0f,
                                 .noiseSeed = 0x1039u},
};
static constexpr Engine::ProceduralSoundDefinition KickPatch{
    .durationSeconds = 0.34f,
    .masterVolume = 0.95f,
    .voices = KickVoices,
};

static constexpr std::array SnareVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.58f,
                                 .pan = 0.54f,
                                 .envelope = {.attackSeconds = 0.002f,
                                              .decaySeconds = 0.045f,
                                              .sustainLevel = 0.10f,
                                              .releaseSeconds = 0.08f},
                                 .filter = Engine::AudioFilterType::HighPass,
                                 .filterCutoffHz = 1500.0f,
                                 .noiseSeed = 0x5a31u},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Triangle,
                                 .frequencyHz = 185.0f,
                                 .volume = 0.11f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.003f,
                                              .decaySeconds = 0.035f,
                                              .sustainLevel = 0.0f,
                                              .releaseSeconds = 0.06f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 900.0f},
};
static constexpr Engine::ProceduralSoundDefinition SnarePatch{
    .durationSeconds = 0.24f,
    .masterVolume = 0.78f,
    .voices = SnareVoices,
    .effects = {.reverbSeconds = 0.06f,
                .reverbDecay = 0.18f,
                .reverbMix = 0.06f},
};

static constexpr std::array HatVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 440.0f,
                                 .volume = 0.28f,
                                 .pan = 0.62f,
                                 .envelope = {.attackSeconds = 0.001f,
                                              .decaySeconds = 0.018f,
                                              .sustainLevel = 0.0f,
                                              .releaseSeconds = 0.028f},
                                 .filter = Engine::AudioFilterType::HighPass,
                                 .filterCutoffHz = 5200.0f,
                                 .noiseSeed = 0x8c77u},
};
static constexpr Engine::ProceduralSoundDefinition HatPatch{
    .durationSeconds = 0.085f,
    .masterVolume = 0.62f,
    .voices = HatVoices,
};

static constexpr std::array ThemeLeadNotes{
    // Couplet A: motif doux et lisible.
    Engine::ProceduralMusicNote{0.0f, 0.5f, 76, 0.72f},
    Engine::ProceduralMusicNote{0.5f, 0.5f, 79, 0.64f},
    Engine::ProceduralMusicNote{1.0f, 0.5f, 81, 0.78f},
    Engine::ProceduralMusicNote{1.5f, 0.5f, 79, 0.64f},
    Engine::ProceduralMusicNote{2.0f, 0.5f, 74, 0.70f},
    Engine::ProceduralMusicNote{2.5f, 0.5f, 76, 0.66f},
    Engine::ProceduralMusicNote{3.0f, 1.0f, 72, 0.80f},
    Engine::ProceduralMusicNote{4.0f, 0.5f, 76, 0.72f},
    Engine::ProceduralMusicNote{4.5f, 0.5f, 79, 0.62f},
    Engine::ProceduralMusicNote{5.0f, 0.5f, 83, 0.78f},
    Engine::ProceduralMusicNote{5.5f, 0.5f, 81, 0.72f},
    Engine::ProceduralMusicNote{6.0f, 0.5f, 79, 0.66f},
    Engine::ProceduralMusicNote{6.5f, 0.5f, 76, 0.64f},
    Engine::ProceduralMusicNote{7.0f, 1.0f, 74, 0.78f},
    // Refrain: plus haut, plus chantant.
    Engine::ProceduralMusicNote{8.0f, 0.5f, 84, 0.92f},
    Engine::ProceduralMusicNote{8.5f, 0.5f, 83, 0.80f},
    Engine::ProceduralMusicNote{9.0f, 0.5f, 81, 0.86f},
    Engine::ProceduralMusicNote{9.5f, 0.5f, 79, 0.74f},
    Engine::ProceduralMusicNote{10.0f, 0.5f, 81, 0.88f},
    Engine::ProceduralMusicNote{10.5f, 0.5f, 84, 0.94f},
    Engine::ProceduralMusicNote{11.0f, 1.0f, 86, 0.98f},
    Engine::ProceduralMusicNote{12.0f, 0.5f, 83, 0.84f},
    Engine::ProceduralMusicNote{12.5f, 0.5f, 81, 0.76f},
    Engine::ProceduralMusicNote{13.0f, 0.5f, 79, 0.82f},
    Engine::ProceduralMusicNote{13.5f, 0.5f, 76, 0.72f},
    Engine::ProceduralMusicNote{14.0f, 0.5f, 79, 0.84f},
    Engine::ProceduralMusicNote{14.5f, 0.5f, 81, 0.86f},
    Engine::ProceduralMusicNote{15.0f, 1.0f, 84, 0.98f},
    // Couplet B: variation plus calme.
    Engine::ProceduralMusicNote{16.0f, 0.5f, 74, 0.68f},
    Engine::ProceduralMusicNote{16.5f, 0.5f, 76, 0.62f},
    Engine::ProceduralMusicNote{17.0f, 0.5f, 79, 0.72f},
    Engine::ProceduralMusicNote{17.5f, 0.5f, 76, 0.64f},
    Engine::ProceduralMusicNote{18.0f, 0.5f, 72, 0.66f},
    Engine::ProceduralMusicNote{18.5f, 0.5f, 74, 0.62f},
    Engine::ProceduralMusicNote{19.0f, 1.0f, 71, 0.76f},
    Engine::ProceduralMusicNote{20.0f, 0.5f, 76, 0.70f},
    Engine::ProceduralMusicNote{20.5f, 0.5f, 79, 0.64f},
    Engine::ProceduralMusicNote{21.0f, 0.5f, 81, 0.76f},
    Engine::ProceduralMusicNote{21.5f, 0.5f, 83, 0.74f},
    Engine::ProceduralMusicNote{22.0f, 0.5f, 81, 0.70f},
    Engine::ProceduralMusicNote{22.5f, 0.5f, 79, 0.66f},
    Engine::ProceduralMusicNote{23.0f, 1.0f, 76, 0.82f},
    // Refrain final: reprise avec cadence de boucle.
    Engine::ProceduralMusicNote{24.0f, 0.5f, 84, 0.96f},
    Engine::ProceduralMusicNote{24.5f, 0.5f, 86, 0.86f},
    Engine::ProceduralMusicNote{25.0f, 0.5f, 88, 0.98f},
    Engine::ProceduralMusicNote{25.5f, 0.5f, 86, 0.84f},
    Engine::ProceduralMusicNote{26.0f, 0.5f, 84, 0.92f},
    Engine::ProceduralMusicNote{26.5f, 0.5f, 81, 0.82f},
    Engine::ProceduralMusicNote{27.0f, 1.0f, 83, 0.96f},
    Engine::ProceduralMusicNote{28.0f, 0.5f, 81, 0.86f},
    Engine::ProceduralMusicNote{28.5f, 0.5f, 79, 0.78f},
    Engine::ProceduralMusicNote{29.0f, 0.5f, 76, 0.82f},
    Engine::ProceduralMusicNote{29.5f, 0.5f, 74, 0.76f},
    Engine::ProceduralMusicNote{30.0f, 0.5f, 76, 0.84f},
    Engine::ProceduralMusicNote{30.5f, 0.5f, 79, 0.86f},
    Engine::ProceduralMusicNote{31.0f, 1.0f, 76, 0.92f},
};
static constexpr std::array ThemeBassNotes{
    Engine::ProceduralMusicNote{0.0f, 0.5f, 40, 0.95f},
    Engine::ProceduralMusicNote{0.5f, 0.5f, 47, 0.68f},
    Engine::ProceduralMusicNote{1.0f, 0.5f, 40, 0.84f},
    Engine::ProceduralMusicNote{1.5f, 0.5f, 47, 0.64f},
    Engine::ProceduralMusicNote{2.0f, 0.5f, 43, 0.88f},
    Engine::ProceduralMusicNote{2.5f, 0.5f, 50, 0.66f},
    Engine::ProceduralMusicNote{3.0f, 0.5f, 43, 0.80f},
    Engine::ProceduralMusicNote{3.5f, 0.5f, 47, 0.66f},
    Engine::ProceduralMusicNote{4.0f, 0.5f, 45, 0.90f},
    Engine::ProceduralMusicNote{4.5f, 0.5f, 52, 0.66f},
    Engine::ProceduralMusicNote{5.0f, 0.5f, 45, 0.82f},
    Engine::ProceduralMusicNote{5.5f, 0.5f, 52, 0.62f},
    Engine::ProceduralMusicNote{6.0f, 0.5f, 43, 0.88f},
    Engine::ProceduralMusicNote{6.5f, 0.5f, 50, 0.64f},
    Engine::ProceduralMusicNote{7.0f, 0.5f, 43, 0.78f},
    Engine::ProceduralMusicNote{7.5f, 0.5f, 47, 0.70f},
    Engine::ProceduralMusicNote{8.0f, 0.5f, 36, 1.00f},
    Engine::ProceduralMusicNote{8.5f, 0.5f, 43, 0.78f},
    Engine::ProceduralMusicNote{9.0f, 0.5f, 36, 0.92f},
    Engine::ProceduralMusicNote{9.5f, 0.5f, 43, 0.72f},
    Engine::ProceduralMusicNote{10.0f, 0.5f, 38, 0.96f},
    Engine::ProceduralMusicNote{10.5f, 0.5f, 45, 0.76f},
    Engine::ProceduralMusicNote{11.0f, 0.5f, 38, 0.88f},
    Engine::ProceduralMusicNote{11.5f, 0.5f, 45, 0.72f},
    Engine::ProceduralMusicNote{12.0f, 0.5f, 40, 1.00f},
    Engine::ProceduralMusicNote{12.5f, 0.5f, 47, 0.78f},
    Engine::ProceduralMusicNote{13.0f, 0.5f, 40, 0.92f},
    Engine::ProceduralMusicNote{13.5f, 0.5f, 47, 0.72f},
    Engine::ProceduralMusicNote{14.0f, 0.5f, 43, 0.96f},
    Engine::ProceduralMusicNote{14.5f, 0.5f, 50, 0.76f},
    Engine::ProceduralMusicNote{15.0f, 0.5f, 43, 0.90f},
    Engine::ProceduralMusicNote{15.5f, 0.5f, 47, 0.78f},
    Engine::ProceduralMusicNote{16.0f, 0.5f, 40, 0.86f},
    Engine::ProceduralMusicNote{16.5f, 0.5f, 47, 0.62f},
    Engine::ProceduralMusicNote{17.0f, 0.5f, 40, 0.78f},
    Engine::ProceduralMusicNote{17.5f, 0.5f, 47, 0.62f},
    Engine::ProceduralMusicNote{18.0f, 0.5f, 43, 0.82f},
    Engine::ProceduralMusicNote{18.5f, 0.5f, 50, 0.62f},
    Engine::ProceduralMusicNote{19.0f, 0.5f, 43, 0.76f},
    Engine::ProceduralMusicNote{19.5f, 0.5f, 50, 0.60f},
    Engine::ProceduralMusicNote{20.0f, 0.5f, 45, 0.86f},
    Engine::ProceduralMusicNote{20.5f, 0.5f, 52, 0.64f},
    Engine::ProceduralMusicNote{21.0f, 0.5f, 45, 0.78f},
    Engine::ProceduralMusicNote{21.5f, 0.5f, 52, 0.62f},
    Engine::ProceduralMusicNote{22.0f, 0.5f, 47, 0.84f},
    Engine::ProceduralMusicNote{22.5f, 0.5f, 54, 0.64f},
    Engine::ProceduralMusicNote{23.0f, 0.5f, 47, 0.80f},
    Engine::ProceduralMusicNote{23.5f, 0.5f, 43, 0.70f},
    Engine::ProceduralMusicNote{24.0f, 0.5f, 36, 1.00f},
    Engine::ProceduralMusicNote{24.5f, 0.5f, 43, 0.80f},
    Engine::ProceduralMusicNote{25.0f, 0.5f, 36, 0.94f},
    Engine::ProceduralMusicNote{25.5f, 0.5f, 43, 0.78f},
    Engine::ProceduralMusicNote{26.0f, 0.5f, 38, 0.98f},
    Engine::ProceduralMusicNote{26.5f, 0.5f, 45, 0.78f},
    Engine::ProceduralMusicNote{27.0f, 0.5f, 38, 0.90f},
    Engine::ProceduralMusicNote{27.5f, 0.5f, 45, 0.76f},
    Engine::ProceduralMusicNote{28.0f, 0.5f, 40, 1.00f},
    Engine::ProceduralMusicNote{28.5f, 0.5f, 47, 0.80f},
    Engine::ProceduralMusicNote{29.0f, 0.5f, 40, 0.94f},
    Engine::ProceduralMusicNote{29.5f, 0.5f, 47, 0.76f},
    Engine::ProceduralMusicNote{30.0f, 0.5f, 43, 0.98f},
    Engine::ProceduralMusicNote{30.5f, 0.5f, 50, 0.80f},
    Engine::ProceduralMusicNote{31.0f, 0.5f, 43, 0.90f},
    Engine::ProceduralMusicNote{31.5f, 0.5f, 40, 0.82f},
};
static constexpr std::array ThemePadNotes{
    Engine::ProceduralMusicNote{0.0f, 4.0f, 64, 0.36f},
    Engine::ProceduralMusicNote{0.0f, 4.0f, 67, 0.32f},
    Engine::ProceduralMusicNote{4.0f, 4.0f, 62, 0.34f},
    Engine::ProceduralMusicNote{4.0f, 4.0f, 69, 0.30f},
    Engine::ProceduralMusicNote{8.0f, 4.0f, 60, 0.46f},
    Engine::ProceduralMusicNote{8.0f, 4.0f, 64, 0.40f},
    Engine::ProceduralMusicNote{12.0f, 4.0f, 64, 0.44f},
    Engine::ProceduralMusicNote{12.0f, 4.0f, 67, 0.38f},
    Engine::ProceduralMusicNote{16.0f, 4.0f, 64, 0.32f},
    Engine::ProceduralMusicNote{16.0f, 4.0f, 67, 0.28f},
    Engine::ProceduralMusicNote{20.0f, 4.0f, 69, 0.34f},
    Engine::ProceduralMusicNote{20.0f, 4.0f, 72, 0.30f},
    Engine::ProceduralMusicNote{24.0f, 4.0f, 60, 0.46f},
    Engine::ProceduralMusicNote{24.0f, 4.0f, 64, 0.40f},
    Engine::ProceduralMusicNote{28.0f, 4.0f, 64, 0.42f},
    Engine::ProceduralMusicNote{28.0f, 4.0f, 67, 0.38f},
};
static constexpr std::array ThemeKickNotes{
    Engine::ProceduralMusicNote{0.0f, 0.25f, 69, 0.92f},
    Engine::ProceduralMusicNote{1.5f, 0.25f, 69, 0.72f},
    Engine::ProceduralMusicNote{2.0f, 0.25f, 69, 0.88f},
    Engine::ProceduralMusicNote{3.5f, 0.25f, 69, 0.70f},
    Engine::ProceduralMusicNote{4.0f, 0.25f, 69, 0.90f},
    Engine::ProceduralMusicNote{5.5f, 0.25f, 69, 0.72f},
    Engine::ProceduralMusicNote{6.0f, 0.25f, 69, 0.86f},
    Engine::ProceduralMusicNote{7.5f, 0.25f, 69, 0.78f},
    Engine::ProceduralMusicNote{8.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{9.0f, 0.25f, 69, 0.64f},
    Engine::ProceduralMusicNote{9.5f, 0.25f, 69, 0.82f},
    Engine::ProceduralMusicNote{10.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{11.5f, 0.25f, 69, 0.84f},
    Engine::ProceduralMusicNote{12.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{13.0f, 0.25f, 69, 0.64f},
    Engine::ProceduralMusicNote{13.5f, 0.25f, 69, 0.82f},
    Engine::ProceduralMusicNote{14.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{15.5f, 0.25f, 69, 0.92f},
    Engine::ProceduralMusicNote{16.0f, 0.25f, 69, 0.82f},
    Engine::ProceduralMusicNote{18.0f, 0.25f, 69, 0.78f},
    Engine::ProceduralMusicNote{20.0f, 0.25f, 69, 0.82f},
    Engine::ProceduralMusicNote{22.0f, 0.25f, 69, 0.80f},
    Engine::ProceduralMusicNote{23.5f, 0.25f, 69, 0.70f},
    Engine::ProceduralMusicNote{24.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{25.0f, 0.25f, 69, 0.66f},
    Engine::ProceduralMusicNote{25.5f, 0.25f, 69, 0.84f},
    Engine::ProceduralMusicNote{26.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{27.5f, 0.25f, 69, 0.88f},
    Engine::ProceduralMusicNote{28.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{29.0f, 0.25f, 69, 0.66f},
    Engine::ProceduralMusicNote{29.5f, 0.25f, 69, 0.84f},
    Engine::ProceduralMusicNote{30.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{31.5f, 0.25f, 69, 0.96f},
};
static constexpr std::array ThemeSnareNotes{
    Engine::ProceduralMusicNote{1.0f, 0.25f, 69, 0.68f},
    Engine::ProceduralMusicNote{3.0f, 0.25f, 69, 0.72f},
    Engine::ProceduralMusicNote{5.0f, 0.25f, 69, 0.68f},
    Engine::ProceduralMusicNote{7.0f, 0.25f, 69, 0.78f},
    Engine::ProceduralMusicNote{9.0f, 0.25f, 69, 0.88f},
    Engine::ProceduralMusicNote{11.0f, 0.25f, 69, 0.92f},
    Engine::ProceduralMusicNote{13.0f, 0.25f, 69, 0.88f},
    Engine::ProceduralMusicNote{15.0f, 0.25f, 69, 1.0f},
    Engine::ProceduralMusicNote{17.0f, 0.25f, 69, 0.58f},
    Engine::ProceduralMusicNote{19.0f, 0.25f, 69, 0.64f},
    Engine::ProceduralMusicNote{21.0f, 0.25f, 69, 0.60f},
    Engine::ProceduralMusicNote{23.0f, 0.25f, 69, 0.76f},
    Engine::ProceduralMusicNote{25.0f, 0.25f, 69, 0.92f},
    Engine::ProceduralMusicNote{27.0f, 0.25f, 69, 0.96f},
    Engine::ProceduralMusicNote{29.0f, 0.25f, 69, 0.92f},
    Engine::ProceduralMusicNote{31.0f, 0.25f, 69, 1.0f},
};
static constexpr std::array ThemeHatNotes{
    Engine::ProceduralMusicNote{0.5f, 0.125f, 69, 0.38f},
    Engine::ProceduralMusicNote{1.0f, 0.125f, 69, 0.32f},
    Engine::ProceduralMusicNote{1.5f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{2.5f, 0.125f, 69, 0.38f},
    Engine::ProceduralMusicNote{3.0f, 0.125f, 69, 0.30f},
    Engine::ProceduralMusicNote{3.5f, 0.125f, 69, 0.44f},
    Engine::ProceduralMusicNote{4.5f, 0.125f, 69, 0.40f},
    Engine::ProceduralMusicNote{5.0f, 0.125f, 69, 0.32f},
    Engine::ProceduralMusicNote{5.5f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{6.5f, 0.125f, 69, 0.38f},
    Engine::ProceduralMusicNote{7.0f, 0.125f, 69, 0.30f},
    Engine::ProceduralMusicNote{7.5f, 0.125f, 69, 0.46f},
    Engine::ProceduralMusicNote{8.5f, 0.125f, 69, 0.54f},
    Engine::ProceduralMusicNote{9.0f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{9.5f, 0.125f, 69, 0.58f},
    Engine::ProceduralMusicNote{10.5f, 0.125f, 69, 0.54f},
    Engine::ProceduralMusicNote{11.0f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{11.5f, 0.125f, 69, 0.60f},
    Engine::ProceduralMusicNote{12.5f, 0.125f, 69, 0.54f},
    Engine::ProceduralMusicNote{13.0f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{13.5f, 0.125f, 69, 0.58f},
    Engine::ProceduralMusicNote{14.5f, 0.125f, 69, 0.56f},
    Engine::ProceduralMusicNote{15.0f, 0.125f, 69, 0.44f},
    Engine::ProceduralMusicNote{15.5f, 0.125f, 69, 0.66f},
    Engine::ProceduralMusicNote{16.5f, 0.125f, 69, 0.28f},
    Engine::ProceduralMusicNote{17.5f, 0.125f, 69, 0.34f},
    Engine::ProceduralMusicNote{18.5f, 0.125f, 69, 0.28f},
    Engine::ProceduralMusicNote{19.5f, 0.125f, 69, 0.36f},
    Engine::ProceduralMusicNote{20.5f, 0.125f, 69, 0.30f},
    Engine::ProceduralMusicNote{21.5f, 0.125f, 69, 0.36f},
    Engine::ProceduralMusicNote{22.5f, 0.125f, 69, 0.32f},
    Engine::ProceduralMusicNote{23.5f, 0.125f, 69, 0.42f},
    Engine::ProceduralMusicNote{24.5f, 0.125f, 69, 0.56f},
    Engine::ProceduralMusicNote{25.0f, 0.125f, 69, 0.44f},
    Engine::ProceduralMusicNote{25.5f, 0.125f, 69, 0.60f},
    Engine::ProceduralMusicNote{26.5f, 0.125f, 69, 0.56f},
    Engine::ProceduralMusicNote{27.0f, 0.125f, 69, 0.44f},
    Engine::ProceduralMusicNote{27.5f, 0.125f, 69, 0.62f},
    Engine::ProceduralMusicNote{28.5f, 0.125f, 69, 0.56f},
    Engine::ProceduralMusicNote{29.0f, 0.125f, 69, 0.44f},
    Engine::ProceduralMusicNote{29.5f, 0.125f, 69, 0.60f},
    Engine::ProceduralMusicNote{30.5f, 0.125f, 69, 0.58f},
    Engine::ProceduralMusicNote{31.0f, 0.125f, 69, 0.46f},
    Engine::ProceduralMusicNote{31.5f, 0.125f, 69, 0.68f},
};
static constexpr std::array ThemeTracks{
    Engine::ProceduralMusicTrack{.instrument = &MusicLeadPatch,
                                 .notes = ThemeLeadNotes,
                                 .volume = 0.58f,
                                 .pan = 0.58f},
    Engine::ProceduralMusicTrack{.instrument = &MusicBassPatch,
                                 .notes = ThemeBassNotes,
                                 .volume = 1.18f,
                                 .pan = 0.45f},
    Engine::ProceduralMusicTrack{.instrument = &MusicPadPatch,
                                 .notes = ThemePadNotes,
                                 .volume = 0.38f,
                                 .pan = 0.5f},
    Engine::ProceduralMusicTrack{.instrument = &KickPatch,
                                 .notes = ThemeKickNotes,
                                 .volume = 1.20f,
                                 .pan = 0.5f},
    Engine::ProceduralMusicTrack{.instrument = &SnarePatch,
                                 .notes = ThemeSnareNotes,
                                 .volume = 0.92f,
                                 .pan = 0.54f},
    Engine::ProceduralMusicTrack{.instrument = &HatPatch,
                                 .notes = ThemeHatNotes,
                                 .volume = 0.82f,
                                 .pan = 0.62f},
};
static constexpr Engine::ProceduralMusicDefinition ThemeMusic{
    .tempoBeatsPerMinute = 112.0f,
    .lengthBeats = 32.0f,
    .masterVolume = 0.72f,
    .tracks = ThemeTracks,
    .effects = {.reverbSeconds = 0.18f,
                .reverbDecay = 0.24f,
                .reverbMix = 0.08f},
};

static constexpr std::array FootstepPitch{
    Engine::AudioEnvelopePoint{0.0f, 1.0f},
    Engine::AudioEnvelopePoint{0.08f, 0.58f},
};
static constexpr std::array FootstepVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                 .frequencyHz = 140.0f,
                                 .volume = 0.55f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.003f,
                                              .decaySeconds = 0.045f,
                                              .sustainLevel = 0.18f,
                                              .releaseSeconds = 0.04f},
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 2200.0f,
                                 .noiseSeed = 0x4217u},
    Engine::ProceduralSoundVoice{
        .waveform = Engine::AudioWaveform::Triangle,
        .frequencyHz = 105.0f,
        .volume = 0.24f,
        .pan = 0.5f,
        .envelope = {.attackSeconds = 0.002f,
                     .decaySeconds = 0.03f,
                     .sustainLevel = 0.0f,
                     .releaseSeconds = 0.025f},
        .frequencyMultiplier = {.points = FootstepPitch}},
};
static constexpr Engine::ProceduralSoundDefinition FootstepPatch{
    .durationSeconds = 0.13f,
    .masterVolume = 1.0f,
    .voices = FootstepVoices,
};

static constexpr std::array NpcVoiceVoices{
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Square,
                                 .frequencyHz = 360.0f,
                                 .volume = 0.18f,
                                 .pan = 0.5f,
                                 .dutyCycle = 0.35f,
                                 .envelope = {.attackSeconds = 0.015f,
                                              .decaySeconds = 0.05f,
                                              .sustainLevel = 0.74f,
                                              .releaseSeconds = 0.08f},
                                 .vibratoFrequencyHz = 7.5f,
                                 .vibratoDepthCents = 95.0f,
                                 .tremoloFrequencyHz = 8.0f,
                                 .tremoloDepth = 0.72f,
                                 .filter = Engine::AudioFilterType::LowPass,
                                 .filterCutoffHz = 2400.0f},
    Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Sine,
                                 .frequencyHz = 620.0f,
                                 .volume = 0.10f,
                                 .pan = 0.5f,
                                 .envelope = {.attackSeconds = 0.02f,
                                              .decaySeconds = 0.03f,
                                              .sustainLevel = 0.68f,
                                              .releaseSeconds = 0.08f},
                                 .vibratoFrequencyHz = 5.0f,
                                 .vibratoDepthCents = 130.0f,
                                 .tremoloFrequencyHz = 10.0f,
                                 .tremoloDepth = 0.65f},
};
static constexpr Engine::ProceduralSoundDefinition NpcVoicePatch0{
    .durationSeconds = 1.25f,
    .masterVolume = 0.74f,
    .voices = NpcVoiceVoices,
    .effects = {.echoDelaySeconds = 0.045f,
                .echoFeedback = 0.08f,
                .echoMix = 0.06f},
};
static constexpr Engine::ProceduralSoundDefinition NpcVoicePatch1{
    .durationSeconds = 1.45f,
    .masterVolume = 0.74f,
    .voices = NpcVoiceVoices,
    .effects = {.echoDelaySeconds = 0.045f,
                .echoFeedback = 0.08f,
                .echoMix = 0.06f},
};
static constexpr Engine::ProceduralSoundDefinition NpcVoicePatch2{
    .durationSeconds = 1.35f,
    .masterVolume = 0.74f,
    .voices = NpcVoiceVoices,
    .effects = {.echoDelaySeconds = 0.045f,
                .echoFeedback = 0.08f,
                .echoMix = 0.06f},
};
static constexpr Engine::ProceduralSoundDefinition NpcVoicePatch3{
    .durationSeconds = 0.92f,
    .masterVolume = 0.74f,
    .voices = NpcVoiceVoices,
    .effects = {.echoDelaySeconds = 0.045f,
                .echoFeedback = 0.08f,
                .echoMix = 0.06f},
};
static constexpr std::array<const Engine::ProceduralSoundDefinition *, 4>
    NpcVoicePatches{&NpcVoicePatch0, &NpcVoicePatch1, &NpcVoicePatch2,
                    &NpcVoicePatch3};
} // namespace

void AudioResources::Initialize() {
  Shutdown();
  if (music.Build(ThemeMusic) && music.Upload()) {
    music.SetVolume(0.82f * masterVolume);
    music.Play(true);
  }
  if (footstep.Build(FootstepPatch))
    (void)footstep.Upload();
  for (std::size_t index = 0; index < npcVoices.size(); ++index) {
    if (npcVoices[index].Build(*NpcVoicePatches[index]))
      (void)npcVoices[index].Upload();
  }
  ready = true;
}

void AudioResources::Update() { music.Update(); }

void AudioResources::SetMasterVolume(float volume) {
  masterVolume = std::clamp(volume, 0.0f, 1.0f);
  music.SetVolume(0.82f * masterVolume);
}

float AudioResources::GetMasterVolume() const { return masterVolume; }

void AudioResources::PlayFootstep(bool leftFoot) const {
  const float pitch = leftFoot ? 0.92f : 1.07f;
  const float pan = leftFoot ? 0.45f : 0.55f;
  footstep.Play(0.95f * masterVolume, pitch, pan);
}

void AudioResources::PlayNpcVoice(std::size_t index) const {
  if (index >= npcVoices.size())
    return;
  const float pitch = 0.92f + static_cast<float>(index) * 0.07f;
  const float pan = 0.47f + static_cast<float>(index % 2) * 0.06f;
  npcVoices[index].Play(0.62f * masterVolume, pitch, pan);
}

void AudioResources::Shutdown() {
  music.Stop();
  music.Unload();
  footstep.Unload();
  for (Engine::ProceduralSound &voice : npcVoices)
    voice.Unload();
  ready = false;
}

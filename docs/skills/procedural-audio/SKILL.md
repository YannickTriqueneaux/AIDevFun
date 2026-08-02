---
name: procedural-audio
description: Create or modify AI-authored synthesized game sound effects, instruments, music sequences, audio playback, or resumable music state using Engine::ProceduralSound and Engine::ProceduralMusic. Use for weapons, impacts, pickups, UI feedback, footsteps, ambience, retro music, tracker-like composition, any Game audio work, and proactively when player-visible gameplay would naturally benefit from audio even if the user did not mention it.
---

# Procedural audio

Use `Engine/Audio/ProceduralAudio.h`. Do not generate WAV files, allocate
samples in gameplay, or synthesize during `Update`.

## Take audio initiative

For every player-visible gameplay or presentation change, actively evaluate
whether audio would make the result feel complete. Users commonly omit audio
from otherwise complete ideas; treat that omission as permission to add a
small, coherent audio layer when it is a natural part of the experience.
Examples include weapon fire and impact, damage and death, spawning, pickups,
movement accents, UI confirmation, scene transitions, environmental ambience,
and a fitting music loop.

Exercise creative judgment rather than adding sound mechanically. Add audio
when it communicates an event, reinforces feedback, establishes atmosphere, or
materially improves game feel. Keep it out when silence is intentional, the
event is insignificant or excessively frequent, or the new audio would conflict
with the requested tone. Prefer a focused set of distinctive, reusable sounds
over indiscriminate effects on every update.

## Organize Game audio

Keep patches, envelopes, note arrays, scores, and transient playback resources
in dedicated audio `.h/.cpp` files whenever practical. Do not bury large sound
or music definitions in `Game.cpp`, entity implementations, or unrelated
gameplay Components. Let the main Game code own or call a focused resource
container such as `AudioResources`; keep event orchestration there, not the
audio data. Split sound effects and music into additional dedicated files when
one audio source becomes unwieldy.

## Create a sound

Build frequency and volume curves from deterministic envelope points. Combine
voices when one oscillator is insufficient.

```cpp
static constexpr std::array LaserPitch{
    Engine::AudioEnvelopePoint{0.00f, 1.0f},
    Engine::AudioEnvelopePoint{0.22f, 0.12f},
};

static constexpr std::array LaserVoices{
    Engine::ProceduralSoundVoice{
        .waveform = Engine::AudioWaveform::Square,
        .frequencyHz = 880.0f,
        .volume = 0.7f,
        .pan = 0.5f,
        .dutyCycle = 0.35f,
        .envelope = {.attackSeconds = 0.003f,
                     .decaySeconds = 0.04f,
                     .sustainLevel = 0.6f,
                     .releaseSeconds = 0.05f},
        .frequencyMultiplier = {.points = LaserPitch},
        .filter = Engine::AudioFilterType::LowPass,
        .filterCutoffHz = 6000.0f},
};

static constexpr Engine::ProceduralSoundDefinition LaserPatch{
    .durationSeconds = 0.22f,
    .voices = LaserVoices,
    .effects = {.echoDelaySeconds = 0.055f,
                .echoFeedback = 0.18f,
                .echoMix = 0.12f},
};
```

Create one reusable transient resource after the audio device exists:

```cpp
if (!laser_.Build(LaserPatch) || !laser_.Upload())
  throw std::runtime_error("Could not create laser sound");

laser_.Play(0.8f, randomPitch, 0.5f);
```

Call `Unload()` from Game shutdown. Use `Sine` for pure tones, `Square` for
retro leads, `Triangle` for soft bass, `Saw` for bright/buzzy sounds, and
seeded `Noise` for impacts, explosions, wind, and percussion. Use ADSR for the
note contour, envelopes for arbitrary frequency/volume motion, vibrato for
pitch motion, tremolo for amplitude motion, and filters to control harshness.

## Compose music

Treat each `ProceduralSoundDefinition` as an instrument tuned around MIDI note
69 (A4). Author notes in beats, then combine tracks into a score.

```cpp
static constexpr std::array LeadNotes{
    Engine::ProceduralMusicNote{0.0f, 0.5f, 60, 1.0f},
    Engine::ProceduralMusicNote{0.5f, 0.5f, 64, 0.9f},
    Engine::ProceduralMusicNote{1.0f, 1.0f, 67, 1.0f},
};
static constexpr std::array Tracks{
    Engine::ProceduralMusicTrack{.instrument = &LeadPatch,
                                 .notes = LeadNotes,
                                 .volume = 0.7f,
                                 .pan = 0.55f},
};
static constexpr Engine::ProceduralMusicDefinition Theme{
    .tempoBeatsPerMinute = 128.0f,
    .lengthBeats = 4.0f,
    .tracks = Tracks,
    .effects = {.reverbSeconds = 0.14f,
                .reverbDecay = 0.25f,
                .reverbMix = 0.10f},
};
```

Call `Build`, `Upload`, and `Play(true)` during initialization. Call
`music.Update()` every frame while uploaded; streaming stalls without it. Use
`Pause`, `Resume`, `Stop`, `Seek`, `SetVolume`, `SetPitch`, and `SetPan` for
runtime control.

## Use an attached MIDI reference

The assistant prompt may include a locally decoded Standard MIDI File
reference. Its text block contains PPQ timing, tempo changes, track names, and
notes in `startBeat,durationBeat,note,velocity,channel` form. Treat it as
musical reference material for rhythm, contour, harmony, instrumentation, and
structure, then author native `ProceduralMusicNote` arrays and patches in the
Game's dedicated audio sources. Do not add a runtime MIDI dependency or copy
the decoded text into Game assets.

Keep the generated score compact and adapted to the game loop. Preserve the
musical characteristics requested by the user while making reasonable changes
for the available procedural instruments and loop length.

## Preserve resume

Keep definitions, PCM, backend handles, `ProceduralSound`, and
`ProceduralMusic` transient and outside Components. Store only stable audio
TypeIDs/enum values and playback state in focused resumable Components:

- active music identity;
- playback time from `GetPlaybackTime()`;
- playing/paused and loop state;
- volume, pitch, and pan;
- music-state-machine choice.

After reload, rebuild/upload the transient resource, call `Play`, restore
controls, then `Seek` to the saved position. Follow
`../gameplay-resume/SKILL.md` for state versions and replacement decisions.
Short one-shot effects normally need no resume state.

## Keep generation safe and efficient

- Build/upload once, reuse the same resource, and trigger only `Play` at event
  time.
- Keep sound effects short. Pre-rendered music uses memory proportional to
  duration and sample rate; prefer compact loops over long scores.
- Use deterministic nonzero noise seeds. Never use frame time or random_device
  during synthesis.
- Keep master and voice volumes conservative; the renderer normalizes only
  clipped mixes.
- Do not call audio upload/playback APIs in headless tests. Test `Build`, sample
  count, duration, peak amplitude, and `GetPcmHash()` determinism.
- Use external recorded/generated assets instead for speech or realistic
  acoustic material; procedural synthesis is intended for stylized audio.

## Sound recipes

- Laser: square/saw voice, steep descending frequency envelope, short release.
- Explosion: two seeded noise voices, fast attack, long release, low-pass sweep.
- Pickup: two or three short ascending notes or sine voices with light echo.
- Footstep: very short filtered noise plus a quiet low triangle transient.
- UI click: 20-50 ms square or sine with near-zero sustain.
- Wind: low-volume noise, low-pass filter, slow tremolo.
- Kick: sine with a rapid pitch drop plus a short noise click.
- Snare: high-pass noise plus a quiet triangle body.

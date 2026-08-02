# Architecture

This document records implementation conventions that are useful to developers
and AI agents but are intentionally kept out of the product-facing README.

The runtime-selectable assistant backend and provider DLL contract are
documented in [AIProviders.md](AIProviders.md).

## Runtime and development-host boundaries

`Engine` is the reusable runtime. It owns graphics, audio, input, UI primitives,
gameplay objects, serialization, hot-reload support, platform services, and
generic IPC transport. It must not own product-specific AI or development-host
behavior.

`AssistantHost` owns the assistant window, conversation state, prompt UI and
processing, Game-tool client, prompt attachment decoding, and the generic
assistant-provider loader. Provider-specific transport, authentication,
pricing, models, and settings live in separate provider DLLs. The neutral ABI
is under `Development`; new OpenAI, Codex, Claude, or other integrations belong
in provider DLLs, never in `Engine`.

`GameHost` owns the reloadable Game DLL and the restricted server-side Game
tools. The Game-tool pipe name is shared development protocol metadata under
`Development/`; only the generic named-pipe transport remains in `Engine`.

The intended dependency direction is:

```text
Game ----------> Engine
GameHost ------> Engine
AssistantHost -> Engine
Launcher ------> Engine

Engine -X-> Game, GameHost, AssistantHost, assistant providers
```

Before adding an Engine subsystem, ask whether Game or GameRelease needs it. If
only AssistantHost consumes it, place it under `AssistantHost`. If it is a
development protocol shared by AssistantHost and GameHost, place its neutral
contract under `Development` while keeping each endpoint in its owning host.

## Incremental compilation and shadow DLLs

CMake and the compiler reuse previous build artifacts, so unchanged translation
units are not rebuilt.

Windows locks a loaded DLL. GameHost avoids that lock by copying the latest
successful `Game.dll` to uniquely named generations:

```text
GameHotReload/
├── Game_0001.dll
├── Game_0002.dll
└── Game_0003.dll
```

GameHost loads the next generation without restarting the Engine or Assistant.
Whenever possible, the Engine restores the running game so play can continue
from where it was interrupted, even after the game code has changed.

## Resumable gameplay architecture

Each loaded game owns one master `GameInstance`. It is the active gameplay
singleton and owns the `ObjectManager` plus exactly one active `World`. Other
game-wide systems derive from `GameInstanceComponent` and are attached below
this root with `AddComponent<T>()`. They use stable TypeIDs, are retrieved with
`GetComponent<T>()`, and are renewed with their owning instance instead of
becoming unrelated global singletons.

Every gameplay object has a versioned `ObjectID`; persistent relationships use
`ObjectRef` rather than C++ pointers. `ObjectRef::Resolve()` needs no manager
argument: it resolves through
`GameInstance::GetInstance()->GetObjectManager()`. An entity contains its
transform and an ordered collection of `ObjectRef<Component>` values, while
each component contains one behavior and the state required to resume that
behavior. Gameplay uses parameterless `Entity::GetComponent<T>()` instead of
depending on a component's layout index or reconstructing references from raw
IDs.

Hot reload constructs and activates a fresh `GameInstance` with the new game
code before restoring the snapshot. The old and new DLL generations can briefly
coexist, but only one instance is active. If loading or resume fails, the host
reactivates the old instance. On success, existing ObjectIDs resolve against the
restored manager in the new instance automatically.

The `MakeComponentType<T>()` registration list defines update order across the
world. Each concrete component class has a paged sparse pool in its owning
GameInstance domain. Slots inside a page have a constant memory stride, while
free slots retain sparse generations for safe recycling. `World::Update()`
visits live component pointers directly in each pool: it does not maintain a
parallel ObjectID list and performs no ObjectManager lookup for dispatch.

Pool domains isolate old and new GameInstances while two DLL generations
briefly coexist. Destroying an instance releases its empty pool pages after its
World and ObjectManager have destroyed their objects. Components produce
requests, while the game-level spawning handler applies entity creation and
destruction at the frame boundary.

BaseGame demonstrates the intended composition:

| Entity | Components | Purpose |
| --- | --- | --- |
| Arena director | `ArenaDirector` | Produces deterministic random enemy spawn requests. |
| Player | `PlayerMovement`, `PlayerWeapon`, `Health` | Separates input-driven movement, firing, and survivability. |
| Enemy | `EnemyMovement`, `EnemyWeapon`, `Health` | Separates pursuit, firing, and survivability. |
| Player projectile | `ProjectileMovement`, `ProjectileDamage` | Represents a player shot as a resumable world object. |
| Enemy projectile | `ProjectileMovement`, `ProjectileDamage` | Represents an enemy shot as a resumable world object. |

The per-frame sequence is:

1. Convert input into component commands.
2. Update all components in registered type order.
3. Resolve collisions and consume component requests.
4. Apply queued destruction and spawning at the frame boundary.
5. Render the resulting world state.

`Games/BaseGame/Include/Game/GameplayComponents.h` defines the stable TypeIDs
and component boundaries. `Games/BaseGame/Source/GameplayComponents.cpp`
implements state-driven behavior and serialization.
`Games/BaseGame/Source/Game.cpp` is the reference for registration, orchestration,
rendering, and rebuilding transient handles after resume. The mandatory rules
for modifying this model live in `docs/skills/gameplay-resume/SKILL.md`.

## Embedded shaders

Keep shaders as C++ raw strings in the game source so they can be created and
modified without adding runtime asset files. Load them only after the window
exists:

```cpp
void MyGame::Initialize() {
  static constexpr std::string_view FragmentShader = R"glsl(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform float time;

void main()
{
    float pulse = 0.5 + 0.5*sin(time);
    finalColor = vec4(pulse, 0.3, 1.0 - pulse, 1.0)*fragColor;
}
)glsl";

  shader_.LoadFromMemory({}, FragmentShader);
}

void MyGame::Render(Engine::RenderContext &context) const {
  shader_.SetFloat("time", elapsedTime_);
  auto &renderer = context.Draw3D();
  renderer.Begin(camera_);
  renderer.BeginShader(shader_);
  renderer.DrawCube({0, 0, 0}, {1, 1, 1}, {255, 255, 255, 255});
  renderer.EndShader();
  renderer.End();
}

void MyGame::Shutdown() { shader_.Unload(); }
```

Release builds compile these strings into the executable. No shader source file
needs to be copied beside the final `.exe`.

## Vector shapes

`Engine::VectorShape` loads an embedded deterministic SVG subset into immutable
meshes. Loading and tessellation are CPU-only; `Upload()` creates GPU resources
once and `Renderer2D::DrawVectorShape()` draws them by group.

Named SVG groups form a transform hierarchy. `VectorShapePose` stores transient
per-group translation, rotation, scale, and opacity, while
`VectorShapeAnimation` samples reusable keyframe tracks. Animation changes
matrices rather than retessellating paths. Gameplay Components serialize only
animation identity and playback state, then rebuild transient poses after a
reload.

Game code declares a shape-specific enum and a constexpr group-name table next
to each embedded SVG. Names are resolved once after loading into validated
`VectorShapeGroupID` handles. Animation sampling and rendering use only those
compact handles and contiguous track arrays; they perform no string lookup.

The supported SVG subset and authoring workflow live in
`docs/skills/vector-shapes/SKILL.md`. Repository assistants discover that skill
through `AGENTS.md` and the embedded Assistant skill tools.

## Procedural audio

`Engine::ProceduralSound` turns declarative oscillator patches into immutable
stereo PCM during initialization and uploads them to the audio backend once.
Patches support deterministic noise, arbitrary envelopes, ADSR, vibrato,
tremolo, pan, low/high-pass filtering, echo, and compact reverb. Triggering a
sound during gameplay performs no synthesis.

`Engine::ProceduralMusic` renders beat-based note tracks from the same patches
and exposes streamed loop, pause, seek, volume, pitch, and pan controls. The
Application owns the audio-device lifetime. Game shutdown releases audio
resources before the Application closes the device.

Patches, scores, PCM, and backend handles are transient. Resumable Components
store stable music identity and playback controls/time, then rebuild the
resource and seek after reload. Authoring rules and sound recipes live in
`docs/skills/procedural-audio/SKILL.md`.

## Codex development control

`Tools/GameDev.ps1` gives external development agents a scriptable control
surface over a Debug session. It starts the launcher, calls the existing
game-specific IPC service, waits for a hot reload, and tails process logs.

This allows an external coding agent such as Codex or Claude Code to run the
same observe-edit-reload-verify loop as the in-game Assistant. Unlike the
Assistant's deliberately restricted Game-source tools, an external agent can
iterate across the complete development workspace when authorized: `Game.dll`,
Engine, GameHost, AssistantHost, Launcher, development scripts, and the build
system. Changes outside `Game.dll` require rebuilding and restarting the
affected development processes; Game-only changes can continue to use hot
reload. This control surface is strictly for development and is excluded from
the shipped Release executable.

```powershell
# Standard per-game build (Games/MyGame/build/Debug)
powershell -File Tools/GameDev.ps1 start -Game MyGame
powershell -File Tools/GameDev.ps1 build-reload -Game MyGame
powershell -File Tools/GameDev.ps1 verify -Game MyGame
powershell -File Tools/GameDev.ps1 logs -Game MyGame -Follow -Lines 40

# A build directory configured elsewhere
powershell -File Tools/GameDev.ps1 reload-wait -BuildDirectory build
```

Available commands are `start`, `ping`, `state`, `build`, `reload`, `reload-wait`,
`build-reload`, `logs`, and `verify`. `build-reload` asks GameHost to run its
controlled incremental Game build before requesting the hot reload. `verify`
pings the host, requests a reload, waits for its final status, and reports the
available logs. The controller rejects paths containing a `Release` directory.
It is not part of `GameRelease` and is never copied into the shipped game.

## Automated tests

The development test framework has two headless suites:

- `Engine.Pure` tests Engine math, value defaults, and deterministic injected
  input without creating a window or touching raylib's live input state.
- `Game.HeadlessHotReload` loads `Game.dll` in-process, reads entity state
  directly through the development ABI, advances gameplay with a controlled
  delta time and injected input, loads a second DLL generation, and verifies
  that the game can continue afterward.

Build and run them with:

```powershell
cmake -S . -B Games/MyGame/build -DGAME_PROJECT=MyGame
cmake --build Games/MyGame/build --config Debug --target AutoTests --parallel
ctest --test-dir Games/MyGame/build -C Debug --output-on-failure
```

No test calls `InitWindow`, `Render`, `RenderUi`, shader initialization, or GPU
APIs, so gameplay and hot-reload tests can run in GitHub Actions without a
visible window. `.github/workflows/auto-tests.yml` runs the suites on a Windows
runner using `BaseGame`.

All state inspection hooks, injected input, and IPC state commands are guarded
by `ENGINE_AUTOTESTS`, which CMake defines only outside the Release
configuration. The test executables are excluded from normal builds and are
not dependencies of `GameRelease`; the shipped executable contains none of
this instrumentation.

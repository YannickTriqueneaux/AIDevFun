# Architecture

This document records implementation conventions that are useful to developers
and AI agents but are intentionally kept out of the product-facing README.

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
cmake --build Games/MyGame/build --config Debug --target AutoTests
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

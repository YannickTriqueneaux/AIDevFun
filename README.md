# [**MakeYourOwnGame.AI**](https://github.com/YannickTriqueneaux/MakeYourOwnGame.AI)

MakeYourOwnGame.AI is a lightweight C++ environment for creating small,
procedural games with an AI development assistant.

The goal is productivity: launch a game, play it, describe what you want to
change, and keep playing while the assistant edits and rebuilds the game.
When the result feels right, the same C++ game can be compiled in Release and
prepared for distribution.

> **License notice:** This source-available project is intended for creating
> free mini-games to enjoy, building proofs of concept, experimenting, and
> sharing games with friends. Game code and content remain private, while
> infrastructure changes outside `Games/` must be shared with the repository
> owner. No royalty is due for copies shared entirely free of charge. When a
> game is used commercially or sold to players, the commercial terms apply,
> including a fixed royalty of CAD $1.00 per paid copy. Read
> [LICENSE](LICENSE) and [CONTRIBUTING.md](CONTRIBUTING.md) before using the
> project.

![A Snake mini-game running beside its AI development assistant](docs/images/game-and-ai-assistant.png)

*A game created and hot-reloaded from the assistant while the playable session
stays open. The assistant also reports the latest prompt and session token-cost
estimates.*

## The workflow

```text
Play the game
      ↓
Describe a change to the AI
      ↓
AI reads and patches the selected Game
      ↓
Only Game.dll is recompiled
      ↓
GameHost hot-reloads the new DLL
      ↓
Keep playing and iterating
```

The game and assistant run in separate windows:

- `<game-name> - Game`
- `<game-name> - AI Assistant`

Press `Tab` in either window to focus its partner. The assistant conversation
survives Game DLL reloads, allowing an ongoing discussion instead of isolated
one-shot prompts.

## Quick start

Requirements:

- Windows
- CMake 3.24 or newer
- a C++20 compiler
- Git
- an OpenAI API key

Run:

```bat
Start.bat
```

`Start.bat`:

1. checks the OpenAI configuration;
2. opens [the OpenAI API key page](https://platform.openai.com/api-keys) and
   securely asks for a key when none is configured;
3. lists the projects under `Games/` and offers to create a new one;
4. asks which game to open, or asks for a name and copies `BaseGame`;
5. configures and builds that game in its private build directory; and
6. starts its Game and AI Assistant windows.

A game can also be selected directly:

```bat
Start.bat CurrentGame
```

## Game projects

Every direct child of `Games/` containing `Source/GameModule.cpp` is an
independent game:

```text
Games/
├── BaseGame/
│   ├── Include/
│   ├── Source/
│   └── build/
└── MyGame/
    ├── Include/
    ├── Source/
    └── build/
```

`BaseGame` is the clean, versioned starting point. Other game directories can
also be versioned normally. The assistant edits the selected
`Games/<game-name>/` directly; there is no temporary source copy.

### Starting a new game

It is strongly recommended to fork this repository before starting a game.
Clone your fork and keep the game under version control so that every change
made manually or by the AI assistant can be reviewed, reverted, and preserved.

To create and start a game:

1. run `Start.bat`;
2. select **Create a new game**;
3. enter the new game's name;
4. wait while `BaseGame` is copied, configured, and built; and
5. use the Game and AI Assistant windows to begin iterating.

The new project is created under `Games/<game-name>/`. Commit that directory to
your fork:

```bat
git add Games/MyGame
git commit -m "Start MyGame"
git push
```

Build outputs under the game directory remain ignored, so commits contain the
game source and content rather than generated files.

To rebuild a known project without the interactive menu:

```bat
dev.bat MyGame
```

To reset a game from `BaseGame` while preserving a timestamped backup:

```bat
reset_game.bat MyGame
```

## How rapid iteration works

### Stable runtime, reloadable game

Reusable systems live in the persistent `Engine` DLL:

- application and window lifecycle;
- procedural 2D and 3D rendering primitives;
- input;
- Dear ImGui;
- serialization interfaces;
- OpenAI communication;
- logging and IPC.

Gameplay lives in a much smaller `Game.dll`:

- input bindings and actions;
- game rules and behaviors;
- entities and state;
- what gets rendered;
- game-specific configuration.

Because the Engine and hosts stay alive, an iteration normally compiles only
the `Game` CMake target.

### Choosing 2D or 3D in a game

`GameInterface::Render` receives an `Engine::RenderContext`. A game can use
`context.Draw2D()`, `context.Draw3D()`, or both in the same frame. For 3D,
call `Renderer3D::Begin(camera)` before drawing the world and
`Renderer3D::End()` afterward. Draw 2D overlays after the 3D pass.

![A game converted to real 3D with perspective, lighting, and an embedded shader by the AI assistant](docs/images/ai-assisted-3d-game-conversion.png)

*The assistant can convert an existing game to real 3D, add a perspective
camera and an embedded lighting shader, build the updated `Game.dll`, and
hot-reload it while preserving the rapid iteration workflow.*

### Embedded shaders

Shaders are C++ raw strings in the game source, so the AI assistant can create
them without adding runtime asset files. Load them after the window exists:

```cpp
void MyGame::Initialize()
{
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

void MyGame::Render(Engine::RenderContext& context) const
{
    shader_.SetFloat("time", elapsedTime_);
    auto& renderer = context.Draw3D();
    renderer.Begin(camera_);
    renderer.BeginShader(shader_);
    renderer.DrawCube({0, 0, 0}, {1, 1, 1}, {255, 255, 255, 255});
    renderer.EndShader();
    renderer.End();
}

void MyGame::Shutdown()
{
    shader_.Unload();
}
```

The release build compiles these strings into the executable. No shader source
file needs to be copied beside the final `.exe`.

### Crash recovery and AI-assisted repair

Launcher monitors `GameHost` and distinguishes a normal exit from a crash. It
does not silently restart a crashed game. Instead, it preserves the available
diagnostics and displays a **Game crash detected** dialog with two choices:

![Game crash recovery dialog offering AI-assisted repair or an immediate relaunch](docs/images/game-crash-recovery-dialog.png)

*After a crash, the user can ask the AI assistant to investigate and repair the
game, or relaunch it unchanged.*

- **Yes** starts AI-assisted recovery. The game remains stopped while a
  recovery-only GameHost keeps the controlled development tools available.
- **No** relaunches the same game immediately without asking the assistant to
  modify it.

In a non-Release build, every process installs crash hooks that write a `.txt`
report and a `.dmp` minidump under the runtime `Crashes/` directory. Text
reports include the process and thread, exception information, and a
symbolized call stack with module names, functions, source files, and line
numbers when matching PDB files are available. Process logs are preserved
across restarts. These crash hooks and the DbgHelp dependency are disabled in
Release builds.

When AI-assisted recovery is accepted:

1. Launcher starts `GameHost --recovery` without loading `Game.dll` or opening
   the game window.
2. The assistant is brought to the foreground and automatically receives one
   recovery request.
3. The assistant reads the newest crash reports and process logs, correlates
   them with the Game source, and applies the smallest justified repair.
4. It builds the Debug `Game` target and repairs compilation errors when
   possible.
5. Only after a successful build may it call `launch_game`, which returns
   control to Launcher and starts the repaired game.

The automatic recovery request is consumed once. It is not submitted again
after the repaired game is running, even if deleting its request file fails.
If the available evidence does not identify a safe repair, the assistant is
instructed to explain that instead of guessing.

### Controlled AI tools

The assistant cannot execute arbitrary shell commands. It communicates with
the selected GameHost through a game-specific local named pipe and receives a
small set of controlled tools:

- list, read, and search selected Game source files;
- list, read, and search Engine C++ source files in read-only mode;
- read several files in a single batch;
- apply exact replacements individually or as a validated batch;
- build only the selected `Game` target;
- inspect compiler output;
- request a DLL reload and inspect its status;
- inspect recent crash reports and process logs during recovery;
- launch a repaired game from recovery mode after a successful build.

Paths are canonicalized and confined to either the selected Game or the Engine
tree. Only C++ source extensions inside the selected Game are editable. Engine
access has separate list, read, batch-read, and search tools with no patch
operation. Build directories and path traversal are explicitly excluded.

Batch reads and patches reduce AI round trips. The assistant is instructed to
inspect related files together, prepare a coherent set of changes, compile
once, repair compiler failures if necessary, and reload only after a successful
build.

### Incremental compilation and shadow DLLs

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
The current implementation initializes a fresh game instance during a reload,
so transient runtime game state may reset.

### Persistent conversation

AssistantHost owns the conversation and OpenAI response context. GameHost owns
the playable window and reloadable Game DLL. Recompiling or reloading the Game
therefore does not erase the active AI discussion.

## Multiple games at once

Each game owns:

- `Games/<game-name>/build/`;
- its executables and `Game.dll`;
- hot-reload generations;
- logs;
- a game-specific IPC pipe;
- uniquely titled Game and Assistant windows.

Several games can run simultaneously. Starting or rebuilding one game stops
only processes launched from that game's build directory; other sessions keep
running.

## Architecture

```text
Engine/             Persistent reusable runtime
GameHost/           Game window and Game.dll hot-reload host
AssistantHost/      AI conversation window
Launcher/           Starts the two hosts for one game session
Games/BaseGame/     Versioned starting game
Games/<name>/       Independently editable game project
```

The dependency direction is intentionally strict:

```text
Game → Engine
Engine ✕ Game
```

This boundary keeps the runtime stable while game code changes frequently.

## Configuration

The local OpenAI configuration is stored in:

```text
AssistantHost/Config/settings.json
```

Example:

```json
{
  "openai": {
    "apiKey": "your-api-key",
    "model": "gpt-5.5",
    "pricing": {
      "model": "gpt-5.5",
      "inputUsdPerMillion": 5.0,
      "cachedInputUsdPerMillion": 0.5,
      "outputUsdPerMillion": 30.0,
      "longContextThreshold": 272000,
      "longContextInputMultiplier": 2.0,
      "longContextOutputMultiplier": 1.5
    }
  }
}
```

The file is ignored by Git and copied into a game's build only when
`AssistantHost` is built. Building only `Game.dll` does not touch it. API keys
and HTTP authorization headers are never written to application logs.

The Responses API reports real input, cached-input, and output token usage, but
OpenAI does not currently expose an official pricing API. Token prices are
therefore configuration data rather than compiled constants. AssistantHost
uses them to display an estimated cost for the latest completed prompt and for
the complete AssistantHost session. Update the pricing block when OpenAI
changes a model's rates.

## Manual builds

Configure and build one game:

```powershell
cmake -S . -B Games/MyGame/build -DGAME_PROJECT=MyGame
cmake --build Games/MyGame/build --config Debug
```

Run:

```text
Games/MyGame/build/Debug/Launcher.exe
```

## Shipping a game

Run the interactive release script:

```bat
Release.bat
```

It asks which game to release and produces:

```text
Games/<game-name>/Release/<game-name>.exe
```

The Release target is a native, optimized, monolithic executable. It statically
links the Engine and selected Game and does not ship GameHost, AssistantHost,
Launcher, `Game.dll`, development logs, or hot-reload code. The development FPS
counter is compiled out. `Release.bat` prints the absolute executable path when
the build succeeds.

Distribution metadata, icons, installers, store SDKs, and platform packaging
remain game-specific responsibilities.

Commercial distribution is subject to [LICENSE](LICENSE), including the fixed
CAD $1.00 royalty for each paid copy sold.

## Logs

Every session writes fresh logs under its own build:

```text
Games/<game-name>/build/Debug/Logs/Launcher.log
Games/<game-name>/build/Debug/Logs/GameHost.log
Games/<game-name>/build/Debug/Logs/AssistantHost.log
```

Assistant logs contain prompts, response IDs, bounded tool arguments/results,
and streaming status. GameHost logs contain controlled builds, IPC requests,
and DLL reload results.

### Codex development control

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

Available commands are `start`, `ping`, `build`, `reload`, `reload-wait`,
`build-reload`, `logs`, and `verify`. `build-reload` asks GameHost to run its
controlled incremental Game build before requesting the hot reload. `verify`
pings the host, requests a reload, waits for its final
status, and reports the available logs. The controller rejects paths containing
a `Release` directory. It is not part of `GameRelease` and is never copied into
the shipped game.

## BaseGame controls

- Move: arrow keys
- `Q`: dash
- `W`: pulse
- `E`: toggle shield
- `R`: reset
- `Tab`: switch between Game and Assistant
- `Escape`: close the focused window

## License

Copyright (c) 2026 Yannick Triqueneaux.

MakeYourOwnGame.AI uses the custom
**MakeYourOwnGame.AI Source-Available Contribution and Commercial Game License
1.0**.

In summary:

- infrastructure modifications outside `Games/` must be provided to the
  Licensor, including private infrastructure modifications;
- gameplay code and content under `Games/<game-name>/` do not have to be
  disclosed;
- game creators retain ownership of their Game Content;
- commercial use requires quarterly reporting; and
- every paid copy of a game created with the repository carries a fixed
  CAD $1.00 royalty.

The full [LICENSE](LICENSE) controls if this summary differs from its terms.
Third-party dependencies remain governed by their respective licenses.

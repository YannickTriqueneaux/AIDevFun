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
3. lists the projects under `Games/`;
4. asks which game to open;
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

`BaseGame` is the clean, versioned starting point. Other game directories are
local and ignored by Git by default. The assistant edits the selected
`Games/<game-name>/` directly; there is no temporary source copy.

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
- procedural 2D rendering primitives;
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

### Controlled AI tools

The assistant cannot execute arbitrary shell commands. It communicates with
the selected GameHost through a game-specific local named pipe and receives a
small set of controlled tools:

- list, read, and search selected Game source files;
- read several files in a single batch;
- apply exact replacements individually or as a validated batch;
- build only the selected `Game` target;
- inspect compiler output;
- request a DLL reload and inspect its status.

Paths are canonicalized and confined to the selected game. Only C++ source
extensions are editable. The game's `build/` directory is explicitly excluded
from listing, searching, reading, and patching.

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

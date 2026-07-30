# Procedural Raylib Game

A small, code-only C++ project with a strict separation between reusable engine
code and game-specific code.

## Architecture

```text
Engine/
  Include/Engine/   Stable public engine API
  Source/           Persistent raylib-backed runtime

Games/
  BaseGame/         Versioned clean starting game
  CurrentGame/      Example local game currently being iterated

Launcher/
  Source/           Process orchestrator

GameHost/
  Source/           Stable game window process that loads Game.dll

AssistantHost/
  Source/           Separate ImGui/OpenAI conversation process
```

The persistent `Engine` shared library owns only reusable systems such as the
application loop, renderer primitives, raw input state, math types, dynamic
library loading, serialization interfaces, Dear ImGui integration, and the
engine prompt console. It has no dependency on `Game`.

The active `Game` shared library maps keys to actions, chooses what to draw, and
implements gameplay behavior. `Launcher` loads it through a versioned API and
destroys all game-owned objects before unloading it. This is the foundation for
loading uniquely named DLL generations during hot reload.

Each directory directly under `Games/` is an independent project. `BaseGame/`
is committed as the distributable starting point. Other game directories are
local by default and ignored by Git. Once selected, the build, hot reload, and
assistant tools all point directly to `Games/<game-name>/`; there is no
intermediate workspace copy.

Each project owns its build directory at `Games/<game-name>/build/`. These
directories and `Games/.Backups/` are ignored by Git. The ignore rules
explicitly keep `Games/BaseGame/` source tracked while still excluding its
local build products.

## Controls

- Move: arrow keys
- Action Q: dash
- Action W: pulse
- Action E: toggle shield
- Action R: reset the player
- Exit: `Escape`

## Build

Requirements:

- CMake 3.24 or newer
- A C++20 compiler
- Git

```powershell
cmake -S . -B Games/MyGame/build -DGAME_PROJECT=MyGame
cmake --build Games/MyGame/build --config Debug
```

On a multi-config generator, run
`Games/MyGame/build/Debug/Launcher.exe`. `Game.dll` and `Engine.dll` are emitted
beside it.

For the normal Windows development loop, run `Start.bat` from the repository
root. It lists every valid directory under `Games/`, asks which game to use,
configures CMake for that project, builds Debug, and starts the launcher.
`dev.bat <game-name>` remains available as a non-interactive rebuild.

Before listing games, `Start.bat` validates
`AssistantHost/Config/settings.json`. When no API key is configured, it opens
the [OpenAI API key page](https://platform.openai.com/api-keys), displays the
same URL in the terminal, and securely prompts for a key without echoing it.
The settings file is then created locally with `gpt-5.5` as its default model.

## Engine prompt console

The launcher starts two independent windows. `GameHost` owns the game window,
while `AssistantHost` owns the OpenAI conversation and operational logs. Enter
a prompt in the assistant text box and press `Enter` to submit it.

Press `Tab` in either window to switch focus to the other process.
The focus target includes the game name, so `Tab` stays within its own session
when several games are open.

Multiple games can run simultaneously. Each session has:

- its own build and executable directory;
- its own `Game.dll` hot-reload generations;
- its own logs;
- a game-specific IPC pipe;
- windows titled `<game-name> - Game` and `<game-name> - AI Assistant`.

Starting or rebuilding one game stops only processes launched from that game's
build directory. Other game sessions remain open.

- Submitted prompts appear on the right.
- Results and engine information appear on the left.
- The history automatically scrolls to the latest message.
- Prompt state belongs to `AssistantHost` and therefore survives game process
  and future `Game.dll` reloads.

The prompt processor is separate from the panel and calls the OpenAI Responses
API asynchronously, keeping network latency off the render thread.

Copy or edit `AssistantHost/Config/settings.json` before building:

```json
{
  "openai": {
    "apiKey": "your-api-key",
    "model": "gpt-5.5"
  }
}
```

`AssistantHost/Config/settings.json` is ignored by Git and copied beside the
executable only when the `AssistantHost` target is built. Rebuilding `Game.dll`
does not touch it. Never commit a real API key. The tracked
`AssistantHost/Config/settings.example.json` documents the expected structure.

The model is instructed to act as a developer for the hot-reloadable `Game`
module. It can use a local named-pipe tool service hosted by `GameHost`.

Available controlled tools:

- list and read C++ files under the selected `Games/<game-name>/`, including batch reads;
- search exact text in Game source;
- apply one exact-text replacement or a validated batch of ordered replacements;
- build only the Debug `Game` target;
- inspect the latest build output;
- request and inspect a generation-based Game DLL reload.

Paths are canonicalized and confined to the selected game directory. Only `.cpp`, `.h`, `.hpp`, and
`.inl` files are writable, file and message sizes are limited, arbitrary shell
commands are not exposed, and Game builds use a fixed CMake command. The
project's `build/` subtree is explicitly excluded from listing, searching,
reading, and patching.

## Logs

Each process creates a fresh log file at startup:

```text
Games/<game-name>/build/Debug/Logs/Launcher.log
Games/<game-name>/build/Debug/Logs/GameHost.log
Games/<game-name>/build/Debug/Logs/AssistantHost.log
```

The assistant log records prompts, response IDs, streaming status, tool rounds,
tool arguments and bounded tool results. The GameHost log records IPC commands,
controlled builds, and DLL reloads. API keys and HTTP authorization headers are
never logged.

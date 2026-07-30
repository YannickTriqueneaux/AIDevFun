# Procedural Raylib Game

A small, code-only C++ project with a strict separation between reusable engine
code and game-specific code.

## Architecture

```text
Engine/
  Include/Engine/   Stable public engine API
  Source/           Persistent raylib-backed runtime

Game/
  Include/Game/     Reloadable game types and configuration
  Source/           Input bindings, rendering, behavior, and DLL exports

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

The `Game` shared library maps keys to actions, chooses what to draw, and
implements gameplay behavior. `Launcher` loads it through a versioned API and
destroys all game-owned objects before unloading it. This is the foundation for
loading uniquely named DLL generations during hot reload.

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
cmake -S . -B build
cmake --build build --config Debug
```

On a multi-config generator, run `build/Debug/Launcher.exe`. `Game.dll` and
`Engine.dll` are emitted beside it.

For the normal Windows development loop, run `dev.bat` from the repository
root. It stops all project processes, configures CMake, builds Debug, and starts
the launcher only after a successful build.

## Engine prompt console

The launcher starts two independent windows. `GameHost` owns the game window,
while `AssistantHost` owns the OpenAI conversation and operational logs. Enter
a prompt in the assistant text box and press `Enter` to submit it.

Press `Tab` in either window to switch focus to the other process.

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
module. It currently returns implementation guidance only; local file tools and
approval-controlled patch application will be added separately.

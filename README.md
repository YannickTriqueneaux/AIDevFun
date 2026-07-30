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
  Source/           Stable executable that loads Game.dll
```

The persistent `Engine` shared library owns only reusable systems such as the
application loop, renderer primitives, raw input state, math types, dynamic
library loading, and serialization interfaces. It has no dependency on `Game`.

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

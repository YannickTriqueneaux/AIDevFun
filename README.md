# Procedural Raylib Starter

A small, code-only C++ game foundation built with raylib. It uses procedural
shapes and keeps window management, input mapping, game flow, and player logic
separate.

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

On a multi-config generator, the executable is placed under `build/Debug`.

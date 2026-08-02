# Repository agent rules

For gameplay, entity, component, spawning, state serialization, hot-reload, or resume work, read and follow `docs/skills/gameplay-resume/SKILL.md` before editing.

For rendering, shaders, graphics resources, incremental builds, DLL loading, shadow-copy work, development controls, or automated tests, read `docs/Architecture.md` before editing.

For SVG, vector shapes, AI-authored game visuals, named visual groups, or vector animation work, read and follow `docs/skills/vector-shapes/SKILL.md` before editing.

For synthesized sound effects, instruments, music, audio playback, or resumable music state, read and follow `docs/skills/procedural-audio/SKILL.md` before editing.

Keep assistant-only behavior under `AssistantHost`: prompts, OpenAI transport,
pricing, settings, conversation state, model tools, and attachment decoding do
not belong in `Engine`. Shared development protocols belong under
`Development`; Engine contains only reusable runtime services and generic
transport.

Before adding or renaming an API, search for an existing operation with the same semantics. Keep one canonical name and migrate callers; do not add forwarding aliases merely to mirror wording from a request or example unless compatibility is explicitly required.

Prefer cohesive architecture over minimizing file or type count. Create new
Game `.h/.cpp` files, EntityTypes, and focused ComponentTypes whenever a new
concept has its own lifecycle, state, behavior, or resume boundary. Do not
accumulate unrelated gameplay, vector art, or audio in `Game.cpp`, `Game.h`, or
other catch-all files merely because they already exist.

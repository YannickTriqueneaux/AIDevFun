# Repository agent rules

For gameplay, entity, component, spawning, state serialization, hot-reload, or resume work, read and follow `docs/skills/gameplay-resume/SKILL.md` before editing.

For rendering, shaders, graphics resources, incremental builds, DLL loading, shadow-copy work, development controls, or automated tests, read `docs/Architecture.md` before editing.

For SVG, vector shapes, AI-authored game visuals, named visual groups, or vector animation work, read and follow `docs/skills/vector-shapes/SKILL.md` before editing.

For synthesized sound effects, instruments, music, audio playback, or resumable music state, read and follow `docs/skills/procedural-audio/SKILL.md` before editing.

For every player-visible gameplay or presentation change, proactively consider audio even when the user did not mention it. Add focused sound effects, ambience, or music when they naturally improve feedback, atmosphere, or game feel; read the procedural-audio skill before doing so. Do not add audio mechanically when silence or restraint better serves the idea.

When a request replaces the entire game, genre, core loop, or creative identity,
treat the old gameplay as obsolete. Do not silently reduce the work to a reskin
or first conversion pass. Create replacement types with new stable TypeIDs,
remove obsolete gameplay and presentation resources, and complete the requested
replacement now unless the user explicitly requests staged migration.
Staging limits new scope only: remove obsolete template gameplay in the first
stage, never keep it dormant, and never restore it during later work without an
explicit user request.
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

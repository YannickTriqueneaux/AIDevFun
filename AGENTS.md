# Repository agent rules

For gameplay, entity, component, spawning, state serialization, hot-reload, or resume work, read and follow `docs/skills/gameplay-resume/SKILL.md` before editing.

For rendering, shaders, graphics resources, incremental builds, DLL loading, shadow-copy work, development controls, or automated tests, read `docs/Architecture.md` before editing.

For SVG, vector shapes, AI-authored game visuals, named visual groups, or vector animation work, read and follow `docs/skills/vector-shapes/SKILL.md` before editing.

Before adding or renaming an API, search for an existing operation with the same semantics. Keep one canonical name and migrate callers; do not add forwarding aliases merely to mirror wording from a request or example unless compatibility is explicitly required.

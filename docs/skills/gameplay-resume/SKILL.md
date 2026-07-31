---
name: gameplay-resume
description: Implement or modify gameplay entities, components, spawning, references, serialization, hot reload, or resume in this repository. Use for every gameplay change that adds state, an EntityType, a ComponentType, or a relationship between gameplay objects.
---

# Gameplay resume architecture

Preserve these invariants:

1. Give every `Entity` and `Component` an `ObjectID { index, version }` through `World`/`ObjectManager`. Never manufacture IDs in gameplay.
2. Store persistent relationships only as `ObjectRef<T>`. Never keep a pointer/reference to another gameplay object across frames. Resolve locally, use it, then discard the pointer.
3. Declare stable compile-time `TypeID`s with `StableTypeID("Game.Namespace.Type")`. Never derive them from registration order. Renaming the string deliberately breaks that type's saved state.
4. Register component types in global update order. Declare each entity's component TypeID layout in its `EntityType`; spawn only through `World::Spawn`.
5. Request spawn/destruction during gameplay and apply it at the frame boundary. A newly returned ref must remain unresolved until `FlushSpawns`.
6. Put spatial state in `Entity::transform`; put behavior data in components. Keep gameplay state-driven.
7. Implement `CurrentStateVersion`, `MinimumStateVersion`, `SaveState`, and `LoadState` on every component. Save the current schema, migrate supported legacy versions, and reject versions outside the declared range.
8. At the first gameplay iteration of a new work session, when explicitly requested, remove obsolete legacy branches, set the minimum to the pre-iteration current version, and bump the current version only when the serialized layout changes.
9. Keep debug names deterministic: entity name and `EntityName+ComponentType` for components.
10. Construct gameplay objects only with `NEW_OBJECT(Type, ...)`; release standalone owners with `DELETE_OBJECT(owner)`. ObjectManager normally owns and releases them automatically. Never use raw `new`/`delete` for `Object` subclasses.
11. Use `NEW_MEMORY`/`DELETE_MEMORY` for other explicitly owned allocations. For STL storage that must use engine buckets, select `Engine::Memory::Allocator<T>` explicitly. Do not add process-global `operator new/delete` overrides across Engine/Game DLLs: initialization order can recurse before the Engine allocator registry exists.
12. Classify every gameplay change before editing: **compatible** (same TypeIDs/schema), **migratable** (same TypeIDs with a versioned `LoadState` migration), or **replacement** (meaning/structure changed substantially). For a replacement, create new EntityType and ComponentType strings/TypeIDs. Never reuse the previous IDs merely to force old state into a fundamentally different design.
13. Resume skips entities whose EntityType is absent or whose declared ComponentType layout changed. Their saved ObjectIDs are generation-invalidated so stale refs cannot resolve to replacements. Ensure the game factory spawns the new default entity when its predecessor was skipped. Other compatible entities must continue resuming normally.

## Validation

Run `cmake --build build --config Debug --target AutoTests`, then `ctest --test-dir build -C Debug --output-on-failure`.

Test deferred resolution, update order, slot reuse/version invalidation, stale refs, exact ID preservation, all state fields, debug names, unsupported versions, malformed snapshots, and an actual DLL reload. Assert state equality before and after reload.

Review persistent gameplay fields before finishing. Replace every object pointer with an `ObjectRef`, or prove the value is transient and cannot cross a frame/reload boundary.

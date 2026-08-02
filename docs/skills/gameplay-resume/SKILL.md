---
name: gameplay-resume
description: Implement or modify gameplay entities, components, spawning, references, serialization, hot reload, or resume in this repository. Use for every gameplay change that adds state, an EntityType, a ComponentType, or a relationship between gameplay objects.
---

# Gameplay resume architecture

## Mental model

Treat the world as resumable data operated on by replaceable code:

- An `Entity` is an identity, a `Transform`, and an ordered collection of
  `ObjectRef<Component>` values.
- A `Component` owns one behavior and all persistent state for that behavior.
- The active `GameInstance` owns the `ObjectManager` and its single `World`.
- `ObjectRef::Resolve()` always uses the active `GameInstance`.
- Component registration order is the global update order.
- `ProceduralGame` translates input, coordinates frame-boundary requests, and
  renders state. Do not turn it into a second monolithic gameplay object.

Build gameplay by composing focused components. Prefer separate movement,
weapon, health, lifetime, damage, spawning, and decision components over one
class that implements an entire actor. Projectiles, directors, triggers, and
other world concepts are entities too; do not keep them as anonymous arrays in
the game class.

Create new types and files freely when they improve ownership and cohesion.
Do not optimize for the smallest file count or append every new behavior to
`Game.cpp`, `Game.h`, or a growing catch-all `GameplayComponents` pair. A new
world concept normally deserves its own EntityType; an independently updated,
serialized, reusable, or replaceable behavior normally deserves its own
ComponentType. Prefer cohesive feature files such as
`DragonEntity.h/.cpp` and `DragonComponents.h/.cpp`, or a similarly clear
feature grouping. Keep trivial declarations together only when separating
them would obscure rather than clarify ownership.

Use this frame flow:

1. Translate external input into commands and submit them to components.
2. Call `World::Update`; components update by registered type order.
3. Let components expose value-based spawn/destruction/action requests.
4. Resolve collisions and consume requests in the game-level spawning handler.
5. Apply `World::Spawn`/`World::Destroy` at the frame boundary, then configure
   newly resolvable components after `FlushSpawns`.

Components must not spawn or destroy entities themselves. Their parameterless
`Update(float)` resolves relationships through `ObjectRef`; it does not receive
the manager or ownership of the `World`. Store pending requests as state when
they can exist at snapshot time so a reload cannot silently lose them.

Preserve these invariants:

1. Give every `Entity` and `Component` an `ObjectID { index, version }` through `World`/`ObjectManager`. Never manufacture IDs in gameplay.
2. Store persistent relationships only as `ObjectRef<T>`. Never keep a pointer/reference to another gameplay object across frames. Call parameterless `Resolve()`, use the resulting pointer locally, then discard it. Resolution must flow through the active `GameInstance`; never pass an `ObjectManager` through gameplay APIs.
3. Declare stable compile-time `TypeID`s with `StableTypeID("Game.Namespace.Type")`. Never derive them from registration order. Renaming the string deliberately breaks that type's saved state.
4. Register each concrete component class with `MakeComponentType<T>()` in global update order. One ComponentType must map to one concrete C++ class and one physical object pool. `World::Update()` must visit live pointers directly in each active GameInstance pool; never rebuild parallel ObjectID update lists or resolve every component through ObjectManager. Declare each entity's component TypeID layout in its `EntityType`; spawn only through `World::Spawn`.
5. Request spawn/destruction during gameplay and apply it at the frame boundary. A newly returned ref must remain unresolved until `FlushSpawns`.
6. Put spatial state in `Entity::transform`; put behavior data in components. Keep gameplay state-driven.
7. Implement `CurrentStateVersion`, `MinimumStateVersion`, `SaveState`, and `LoadState` on every component. Save the current schema, migrate supported legacy versions, and reject versions outside the declared range.
8. At the first gameplay iteration of a new work session, when explicitly requested, remove obsolete legacy branches, set the minimum to the pre-iteration current version, and bump the current version only when the serialized layout changes.
9. Keep debug names deterministic: entity name and `EntityName+ComponentType` for components.
10. Construct gameplay objects only with `NEW_OBJECT(Type, ...)`; release standalone owners with `DELETE_OBJECT(owner)`. ObjectManager normally owns and releases them automatically. Never use raw `new`/`delete` for `Object` subclasses.
11. Use `NEW_MEMORY`/`DELETE_MEMORY` for other explicitly owned allocations. For STL storage that must use engine buckets, select `Engine::Memory::Allocator<T>` explicitly. Do not add process-global `operator new/delete` overrides across Engine/Game DLLs: initialization order can recurse before the Engine allocator registry exists.
12. Classify every gameplay change before editing: **compatible** (same TypeIDs/schema), **migratable** (same TypeIDs with a versioned `LoadState` migration), or **replacement** (meaning/structure changed substantially). For a replacement, create new EntityType and ComponentType strings/TypeIDs. Never reuse the previous IDs merely to force old state into a fundamentally different design.
13. Resume skips entities whose EntityType is absent or whose declared ComponentType layout changed. Their saved ObjectIDs are generation-invalidated so stale refs cannot resolve to replacements. Ensure the game factory spawns the new default entity when its predecessor was skipped. Other compatible entities must continue resuming normally.
14. Keep component references on their owning `Entity`. Use parameterless
    `entity.GetComponent<T>()` for typed access. Do not expose layout
    indices in gameplay or rebuild component refs manually from raw ObjectIDs.
    Component refs cached elsewhere are transient conveniences only and must be
    reacquired after resume.
15. Give each loaded game exactly one `GameInstance`. It owns exactly one
    `ObjectManager` and one active `World`. Put future game-wide systems under
    this root as `GameInstanceComponent`s, not in unrelated process globals.
    Give each such component a stable TypeID and use `AddComponent<T>()` /
    `GetComponent<T>()`. During hot reload, install the new instance before
    resume; on failure reactivate the old instance before gameplay continues.
16. Challenge requested API sketches before implementing them. Search for an
    existing operation with identical semantics, retain one canonical name,
    and migrate its callers. Do not create forwarding aliases solely to make
    code resemble pseudocode from a request.

17. Treat a request to replace the whole game, genre, core loop, or creative
    identity as an explicit replacement boundary. Do not reinterpret it as a
    reskin, compatibility migration, or "first pass." Create new EntityTypes
    and ComponentTypes with new TypeIDs, remove obsolete gameplay and
    presentation resources, and allow incompatible old state to be skipped.
    Deliver the complete replacement in the current request unless the user
    explicitly asks for staged work. Preserve only infrastructure and concepts
    that remain genuinely compatible with the new game.
## BaseGame reference

Use `Games/BaseGame` as the concrete architecture example:

- `ArenaDirectorEntity` + `ArenaDirector`: deterministic random enemy requests.
- `PlayerEntity` + `PlayerMovement` + `PlayerWeapon` + `Health`.
- `EnemyEntity` + `EnemyMovement` + `EnemyWeapon` + `Health`.
- Player/enemy projectile entities + `ProjectileMovement` +
  `ProjectileDamage`.

Read `Games/BaseGame/Include/Game/GameplayComponents.h` for stable TypeIDs and
component state boundaries. Read `Games/BaseGame/Source/GameplayComponents.cpp`
for update/serialization patterns. Read `Games/BaseGame/Source/Game.cpp` for
registration order, request consumption, collision handling, rendering, and
resume-time reacquisition of core `ObjectRef`s. Read `Game.h` to see the game
own its `GameInstance` and borrow that instance's single `World`.

When adding a behavior, first decide whether it belongs in an existing focused
component or needs a new ComponentType. When adding a world concept, create an
EntityType with an explicit component layout. Never add a parallel unmanaged
collection to `ProceduralGame` merely because it is convenient.

Before editing an existing type, ask whether the requested concept has a
distinct lifecycle, state schema, update responsibility, spawn policy, or
resume compatibility boundary. If it does, create a new EntityType,
ComponentType, and dedicated source files instead of expanding an unrelated
type. When a feature substantially replaces an existing concept, use new
stable TypeIDs as required by the replacement rules rather than reshaping the
old type solely to preserve its state.

## Validation

Run `cmake --build build --config Debug --target AutoTests --parallel`, then `ctest --test-dir build -C Debug --output-on-failure`.

Test deferred resolution, update order, slot reuse/version invalidation, stale refs, exact ID preservation, all state fields, debug names, unsupported versions, malformed snapshots, and an actual DLL reload. Also assert the gameplay outcome of each new component, such as spawned entity counts or projectile factions. Assert state equality before and after reload.

For `GameInstance` changes, test newest-instance activation, explicit
reactivation, inactive-instance destruction, clearing the active singleton,
the single-World invariant, duplicate GameInstanceComponent TypeIDs, reverse
component destruction order, and one pre-reload `ObjectRef` resolving the
restored object through the renewed manager.

For component update changes, assert type-batched update order, constant-stride
addresses inside a pool page, isolation between simultaneous GameInstances,
and that `World::Update()` does not increment ObjectManager's lookup counter.

Review persistent gameplay fields before finishing. Replace every object pointer with an `ObjectRef`, or prove the value is transient and cannot cross a frame/reload boundary.

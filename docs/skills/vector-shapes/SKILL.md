---
name: vector-shapes
description: Create or modify AI-authored SVG vector game visuals, named shape groups, GPU-backed VectorShape resources, keyframe or procedural group animation, and resumable animation components. Use for vector art, icons, characters, projectiles, effects, or shape animation in Engine or Game code.
---

# Vector shapes

Use `Engine::VectorShape` from
`Engine/Include/Engine/Graphics/VectorShape.h`. Use standalone SVG XML as the
source format; never introduce HTML, CSS, scripts, data URLs, or external asset
dependencies.

## Author the resource

Embed SVG as a C++ raw string with a `viewBox`. Give every independently
animated part a stable, descriptive group `id`.

```cpp
static constexpr std::string_view PlayerSvg = R"svg(
<svg viewBox="0 0 100 100">
  <g id="body"><circle cx="50" cy="50" r="42" fill="#4ADE9C"/></g>
  <g id="weapon">
    <path d="M48 16 L55 16 L55 54 L48 54 Z" fill="#FFFFFF"/>
  </g>
</svg>
)svg";

enum class PlayerGroup : std::size_t { Body, Weapon, Count };
static constexpr std::array<std::string_view,
                            static_cast<std::size_t>(PlayerGroup::Count)>
    PlayerGroupNames{"body", "weapon"};
```

Keep the enum and name table adjacent to the SVG. The strings are authoring
metadata used once to bind the SVG; frame code uses compact group IDs.

Use only the deterministic subset:

- Elements: `svg`, `g`, `path`, `circle`, `rect`, `ellipse`, `line`,
  `polyline`, and `polygon`.
- Path commands: `M`, `L`, `H`, `V`, and `Z`, absolute or relative.
- Attributes: `viewBox`, `id`, `transform`, `fill`, `stroke`, `stroke-width`,
  and `opacity`, plus geometry attributes for each element.
- Transforms: `translate`, `scale`, `rotate`, and `matrix`.
- Colors: `#RGB`, `#RRGGBB`, `#RRGGBBAA`, `none`, and basic named colors.

Keep filled paths simple, closed, and without holes. Treat a failed
`LoadFromSvg()` and its `GetLastError()` as an authoring error.

## Load and draw

Parse on initialization or hot reload, then upload while the graphics context
is active. `position` is the shape center; `size` is its rendered dimensions.

```cpp
Engine::VectorShape playerShape;
if (!playerShape.LoadFromSvg(PlayerSvg) || !playerShape.Upload())
  throw std::runtime_error(playerShape.GetLastError());

Engine::VectorShapeDrawParameters draw;
draw.position = worldPosition;
draw.size = {48.0f, 48.0f};
draw.tint = {255, 255, 255, 255};
context.Draw2D().DrawVectorShape(playerShape, draw);
```

Call `Unload()` before destroying the graphics context. Never call `Upload()`
from headless tests. Reuse one loaded `VectorShape` for every visually identical
entity; it owns immutable GPU meshes grouped by SVG `id`.

## Animate named groups

Create one `VectorShapePose` for each independently posed draw instance. Use
`VectorShapeAnimation` for reusable keyframe clips. Sampling interpolates
translation, shortest-path rotation, scale, and opacity and resets untracked
groups to identity. Rotation and scale use the center of the group's
tessellated bounds as their pivot; author geometry around the intended part,
not around the SVG document origin.

```cpp
Engine::VectorShapePose pose(playerShape);
const auto groups = playerShape.ResolveGroups(PlayerGroupNames);
const auto group = [&groups](PlayerGroup value) {
  return groups[static_cast<std::size_t>(value)];
};

Engine::VectorShapeAnimation recoil(playerShape);
recoil.AddKeyframe(group(PlayerGroup::Weapon), 0.0f, {});

Engine::VectorShapeTransform kicked;
kicked.translation = {0.0f, 5.0f};
kicked.rotationDegrees = -12.0f;
recoil.AddKeyframe(group(PlayerGroup::Weapon), 0.08f, kicked);
recoil.AddKeyframe(group(PlayerGroup::Weapon), 0.16f, {});

recoil.Sample(playbackTime, false, pose);
draw.pose = &pose;
context.Draw2D().DrawVectorShape(playerShape, draw);
```

Resolve the complete table after every SVG load and fail initialization if any
ID is invalid. For procedural motion, pass the cached `VectorShapeGroupID` to
`pose.SetGroupTransform()`. Never resolve names, hash strings, animate XML, or
modify path points in the frame loop. A group ID carries its resource identity,
so accidentally passing an ID from another shape safely fails.

## Preserve hot-reload state

Treat shapes, poses, clips, parsed SVG, and GPU meshes as transient graphics
resources. Never serialize them or make them gameplay Objects.

Store persistent playback state in a focused gameplay Component: clip TypeID,
playback time, speed, loop mode, direction, and state-machine choice. Follow
`../gameplay-resume/SKILL.md`. Rebuild transient resources after reload, then
sample the restored playback time into a fresh pose.

## Validate performance and behavior

- Never parse, tessellate, upload, or allocate SVG resources during `Render`.
- Animate group transforms; this changes matrices without rebuilding meshes.
- Prefer a few meaningful groups over splitting every primitive.
- Use `GetTriangleCount()` in headless tests without a GPU context.
- Test unsafe SVG, missing group IDs, interpolation, looping, and restored
  playback state.
- Build Engine and Game after public graphics API changes; Game-only SVG
  authoring can use the normal hot-reload loop.

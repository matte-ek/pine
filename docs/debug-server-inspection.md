# Filtered and batched scene inspection

`POST /entities/query` reads scene entities without a capture or visible viewport.
Use it to fetch lights with their properties and world transforms, read a known
set of IDs, or find objects near a placement point.

## Select entities

Fetch all lights, expanding only their Light components:

```json
{
  "filter": {"component": "Light"},
  "include": {
    "components": ["Light"],
    "properties": true,
    "worldTransform": true
  },
  "limit": 128
}
```

Alternatively, read 1–128 existing scene references in request order:

```json
{
  "entities": [{"id": "<lamp-id>"}, {"id": "<parent-id>"}],
  "include": {"properties": true, "localTransform": true, "worldTransform": true},
  "sceneGeneration": 3
}
```

Explicit batches preserve duplicates and reject the whole request if any reference
is invalid, missing or beneath a temporary editor entity. They cannot supply
`filter` or `limit`, and never return a truncated batch. Batch creation refs and
pool-slot references are not accepted.

Without `entities`, the optional `filter` fields combine with AND:

| Field | Meaning |
| --- | --- |
| `component` | Exact engine type name, such as `Light`, `ModelRenderer` or `TerrainRenderer`. Tests component membership, including types without an editing adapter. Explicit 2D component queries are outside this API's scope and rejected. |
| `name` | `{"value":"lamp","match":"contains"}`. `match` is `exact` (default) or `contains`; both are case-sensitive byte comparisons, without regex, wildcards or Unicode normalization. |
| `hierarchy` | `{"root":{"id":"..."},"mode":"descendants","includeRoot":false}`. Mode is `descendants` (default) or direct `children`. `includeRoot` adds the root as a candidate; it must still satisfy the other filters. |
| `includeInactive` | Boolean, default `true`. When false, excludes inactive entities, requires an active matching component for `component`, and excludes inactive geometry components from bounds tests. Parent inactivity does not implicitly disable children, matching Pine's `IsWorldEnabled` semantics. |
| `spatial` | Pivot or geometry-bounds selection, described below. |

`{}` queries all scene entities, subject to response limits. Temporary editor
entities and their descendant subtrees are always excluded. Static flags do not
affect selection. Results follow current scene-list order; names need not be unique.

## Choose returned details

Every result includes `id`, `internalId`, `name`, `active`, `static`, `temporary`,
`parent` (entity ID or null), and `components`. Each component includes `id`,
`type` and `active`. There is no recursive child expansion.

The optional `include` object controls additional details:

- `components`: array of at most 32 exact component type names. Omit to report all
  component identities; `[]` omits all component entries. This controls output,
  independently of the membership filter. Entries retain entity component order,
  including multiple instances of the same type. Repeated requested types do not
  duplicate entries. An absent requested component simply has no entry.
- `properties`: boolean, default false. Adds entity properties and the selected
  components' properties using the existing [editable readback format](debug-server-editing.md#writable-state-readback).
  Unsupported component adapters return `properties: null`. No serialized `data`
  dump is included; use `/entity` for that representation. Activation filters do
  not remove component entries from a selected entity's readback.
- `worldTransform` and `localTransform`: booleans, both default false. Each adds
  `{position,rotation,scale}` using the same fresh accessors and quaternion format
  as [spatial measurements](debug-server-spatial.md). Pine adds parent/child
  positions, composes rotations and multiplies scales; parent rotation and scale
  do not transform the child's positional offset.

An abbreviated example response:

```json
{
  "sceneGeneration": 3,
  "total": 1,
  "truncated": false,
  "entities": [{
    "id": "<lamp-id>",
    "internalId": 12,
    "name": "Hall lamp",
    "active": true,
    "static": false,
    "temporary": false,
    "parent": "<group-id>",
    "components": [{"id":"<light-id>","type":"Light","active":true}],
    "worldTransform": {
      "position": {"x":12,"y":3,"z":20},
      "rotation": {"x":0,"y":0,"z":0,"w":1},
      "scale": {"x":1,"y":1,"z":1}
    }
  }]
}
```

## Find nearby objects

Use a lamp's returned world position as a radius center:

```json
{
  "filter": {
    "spatial": {
      "test": "bounds",
      "radius": {"center":{"x":12,"y":3,"z":20},"distance":5}
    }
  },
  "include": {"worldTransform":true}
}
```

`spatial.test` is required: `pivot` tests the world position and can find lights
and empty groups; `bounds` tests the union of the entity's model/terrain world
AABBs. Entities without supported geometry never match a bounds test. Children
are separate candidates; their geometry is not aggregated into their parent.

Supply exactly one shape:

- `radius`: sphere with required `center` vector and nonnegative `distance`.
  Bounds mode tests sphere–AABB intersection using the closest point on the box.
- `bounds`: world-axis-aligned box with required `min` and `max` vectors and
  `min <= max` on every axis. Bounds mode tests AABB overlap.

Vectors are strict `{x,y,z}` objects. All coordinates and distances are finite
numbers with absolute value at most `1e12`, in Pine world units. Zero radii and
zero-size boxes are allowed; touching counts as a match. Geometry coverage and
freshness follow [spatial measurements](debug-server-spatial.md#geometry-coverage-and-freshness):
selected model mesh bounds, negative/zero scale, and live terrain chunk bounds
with terrain's translation-only transform. Colliders, shader deformation and
other geometry are not measured. An enclosing-box match is not a triangle
intersection, physics collision or proof of free space.

## Limits, timing and errors

- Requests are limited to 16 KiB and eight JSON nesting levels. Unknown fields,
  query parameters, malformed JSON, invalid types/references and non-finite
  measured spatial state return HTTP 400 with `error` and `path`.
- Filtered queries accept `limit` from 1–128, default 128. `total` counts all
  matches before limits; `truncated` indicates omitted results. Selection scans
  the current scene; there is no spatial index or retained snapshot.
- Successful responses use compact JSON, capped at **1 MiB**, including escaped
  strings and metadata. Filtered queries stop at a whole-entity boundary when
  either limit is reached. If even the first result cannot fit, HTTP 413 tells
  the caller to narrow the projection. An explicit-ID batch exceeding the byte
  cap returns 413 without partial results.
- There is no pagination. Narrow the name, hierarchy or spatial filter when
  results are truncated. Current scene-list order is repeatable while the scene
  is unchanged, but is not a persistent ordering across edits or reloads.
- Selection and returned properties/transforms are sampled in one main-thread
  execution, including during playing or paused play mode. No rendering or physics
  step is awaited. The query does not change camera, selection, scene, history or
  unsaved state, and rejects mutation retry headers.
- `sceneGeneration` identifies the current scene lifetime. An optional expected
  generation in the request returns HTTP 409 if it differs. It detects scene
  replacement, including Stop restoring a play snapshot; it is **not** an edit
  revision, frame identity or a cross-request snapshot. Later edits can invalidate
  earlier measurements even if their generations agree.

## Verification

After configuring and building Editor with Ninja:

```sh
python3 Editor/src/DebugServer/Verification/verify-inspection.py --build build
```

The recipe launches a disposable project under Xvfb and shuts it down afterwards.
Native checks cover queries before rendering, unsupported adapters, component
activation, temporary descendants, byte truncation and UTF-8 replacement, fresh
terrain bounds, invalid spatial state and play/stop generation handling. HTTP
checks cover the lights/nearby-object workflow, projections, ordered batches,
hierarchy/name combinations, sphere/box boundaries, limits, validation, unchanged
editor state, immediate transform freshness and scene reload. It also runs the
existing individual/observation readback recipe to check compatibility.

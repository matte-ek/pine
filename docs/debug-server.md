# Editor debug server — HTTP reference

The Editor runs a localhost HTTP server that lets an agent or a script inspect a
running project, measure and edit the scene, capture rendered frames, and save a
Level — everything the ImGui UI does for scene authoring, without a human at the
keyboard. This page is the contract: every route, what it accepts, what it returns
and where it refuses. For how to actually build something with it, read
[the scene-authoring workflow](debug-server-workflow.md).

Implementation lives under `Editor/src/DebugServer/`. Nothing in `Engine/` depends
on it.

## Enable and connect

The server is off unless `PINE_DEBUG_SERVER` is set. The variable both enables it
and picks the port: `1` means the default **127.0.0.1:9002**, any valid port number
(1024–65535) selects that port instead, anything else falls back to the default.

```sh
PINE_DEBUG_SERVER=1 ./Editor <project-name>
curl --silent --show-error --fail-with-body http://127.0.0.1:9002/status
```

It binds loopback only and has no authentication. It is a control channel into a
running editor; keep it local.

Use `--fail-with-body` or check the status code yourself. Plain `curl -sS` exits
successfully on an HTTP error, and an unknown route can answer with an empty body.

## Execution model

Every handler runs on the **editor's main thread**, between frames. HTTP workers
only parse the request, hand a job to the debug server's own queue, and block until
a result comes back. This is why the API behaves the way it does:

- Handlers may touch engine state freely; nothing needs locking.
- Requests are serialized against each other and against the frame loop. There is
  no partial-frame state and no torn read.
- A handler that blocks the main thread blocks rendering. Hence the five-second
  deadline on every request.
- Reads that need a rendered frame (`/observe`, `/pick`) are deferred and resumed
  after rendering, before ImGui and queued writes.

Pine's own `Threading` system is deliberately not used here: `PumpMainThreadTasks()`
is only called from inside `AwaitTaskResult`/`AwaitTaskPool` and `Engine::Run()`
never pumps, so a queued main-thread task awaited from an HTTP worker would hang.

## Conventions

These apply to every route unless its entry says otherwise.

**Bodies and encoding.** POST bodies are JSON with `Content-Type: application/json`.
Query values must be URL-encoded. Unknown fields and unexpected query parameters are
rejected rather than ignored — a typo fails loudly instead of silently doing
something else. Responses are pretty-printed JSON; invalid UTF-8 in entity names or
asset paths is replaced rather than throwing.

**Vectors and rotations.** Vectors are strict `{"x":…,"y":…,"z":…}` objects with all
three coordinates required. Rotations are `{"x":…,"y":…,"z":…,"w":…}` quaternions,
normalized on application. Numbers must be finite and representable as float32.
Coordinates and distances are in Pine world units, with absolute value at most `1e12`
on the geometric routes.

**Identifiers.** Entities and components have persistent `UId`s: 1–16 hexadecimal
digits, a dash, and 16 hexadecimal digits, first group nonzero. These survive
save/reload of the objects they name and are what every route wants. `internalId`
(a pool slot) exists on reads for debugging; prefer the persistent ID. An entity ID
and a component ID are not interchangeable — `component.update` wants a component
ID, `component.add` wants an entity ID, and mixing them is a validation error.

**Temporary editor entities** (the editor's own camera and helpers) and their whole
descendant subtrees are excluded from every query and rejected as every target.
They still appear in `/entities` with `temporary: true` and in `/status.entityCount`.

**Play mode.** Every write — scene edits, imports, undo/redo, saving, loading,
terrain sculpting, game-camera selection — requires **stopped** play mode and returns
HTTP 409 otherwise. `worldPaused: true` is not the same thing and is not sufficient.
Reads stay available during play; so do the editor-camera routes. `/spatial/raycast`
and `/spatial/overlap` are reads that nonetheless require stopped mode, because they
read live GPU mesh data.

**Coordinate and hierarchy semantics.** Pine's `Transform` is not a conventional
scene graph, and the API exposes that faithfully:

- World position = parent world position **+** local position. Parent rotation and
  scale do **not** rotate or scale the child's positional offset.
- World rotation = parent world rotation × local rotation.
- World scale = parent world scale × local scale, component by component.

So scaling or rotating a group does not lay out its children the way a conventional
engine would. Use explicit positions for repeated modules. Entity `active` and
`static` flags likewise apply only to the entity itself and do not propagate to
children; an inactive parent gates its own components without disabling descendants.
Zero and negative scales are valid throughout.

**Scene generation.** `sceneGeneration` identifies the current scene *lifetime*. It
changes when the scene is replaced — loading any Level including the same one again,
and Stop restoring the pre-play snapshot. It is not a frame ID and not an edit
revision: two requests agreeing on a generation can still be separated by edits.
Routes that accept an expected `sceneGeneration` return 409 when it differs.

**Errors.** Failures return a JSON body with `error` and, where a specific input is
at fault, a slash-separated `path` into the request. Batch operations also carry the
zero-based `operation` index.

| Status | Meaning |
| --- | --- |
| 400 | Malformed or invalid request. Nothing was changed. |
| 404 | Unknown entity, component or asset ID. |
| 409 | Precondition failed: wrong play mode, hidden viewport, stale token, conflicting retry identity, capacity or work limit exceeded. Nothing was changed. |
| 413 | Response or request body exceeds its byte cap. |
| 500 | Execution failed **after** mutation started. State may have changed; the body says what completed. |
| 503 | Shutting down, or the request registry is at capacity. |
| 504 | The five-second deadline expired. For a mutation this does **not** mean it was cancelled. |

## Observation tokens

Every successful mutation returns an `observationToken`: an object holding the
server `session`, `sceneGeneration`, the debug mutation `revision`, the `frame` the
operation ran on, and a `logsSince` log cursor sampled just before it. Execution
failures that may have changed state get one too; validation failures do not.

Pass a token to `POST /observe` as `after` to capture a frame that is guaranteed to
come after that operation, with no client-side sleeping. A token is an **ordering
barrier, not a snapshot**: later edits, UI changes and mouse navigation land in the
captured frame too. It does not freeze the editor or promise historical pixels.

## Mutation retry identities

Every mutation route accepts two optional headers together:

```
X-Pine-Session: <session from GET /requests>
Idempotency-Key: <a unique key you keep for this one logical operation>
```

With both set, the result is retained and the operation is deduplicated. A retry
with the same key, session, route, decoded query and **byte-identical** body joins
the original in-flight request or returns its original status and body. It never
runs the operation twice. Without the headers a request executes normally but is not
retained or deduplicated.

- Keys accept 1–128 characters from ASCII letters, digits, `.`, `_` and `-`. Both
  headers are required when either is present; malformed, empty or repeated values
  return 400.
- Body comparison is byte-for-byte, so reformatted JSON counts as a different
  payload and returns 409. Query order and equivalent escaping do not matter;
  duplicate query names are rejected.
- Keys are shared across all mutation routes within a session. Reuse with a
  different route or payload returns 409 without touching the original record.
- Read routes reject these headers with 400, including `POST /observe` and
  `POST /pick`.
- A different server session returns 409 before admission.

Mutation responses carry a `request` object with `id`, `session`, `tracked`, `state`
and `mayHaveExecuted`.

| State | Meaning | `mayHaveExecuted` |
| --- | --- | --- |
| `pending` | Admitted; the handler has not started. | false |
| `running` | Validation or execution has started. | true |
| `succeeded` | Finished successfully. | true |
| `rejected` | Rejected before any mutation. | false |
| `failed` | Execution failed; inspect the retained response for partial changes. | true |
| `cancelled` | Cancelled, expired or shut down before execution. | false |
| `unknown` | No record. Never-admitted and expired are indistinguishable. | true |

**Deadlines.** The five-second deadline starts at admission and is rechecked before
the handler starts. If it expires first, the request becomes `cancelled` with a
retained 504 and can never execute. If the handler has already started, the caller
gets 504 with `state: "running"` and the handler **continues** — poll
`GET /requests?id=…` for the real outcome. A client disconnect is not cancellation.
Timed-out reads are discarded; reads have no scene side effects.

**Retention.** Terminal results are kept for **ten minutes after completion**, up to
**256 retained identities**; new identified work is refused with 503 at capacity, and
unexpired results are never evicted to make room. At 256 active requests all new work
is refused with 503. Records survive scene replacement, so replaying an old creation
after a level load returns its old result — with IDs that no longer exist — rather
than creating anything. After expiry or an `unknown` status, reconcile the live scene
before issuing another write.

---

# Routes

## Status and logs

### `GET /status`
Returns `project`, `activeLevel` (virtual path or null), `playState`, `worldPaused`,
`entityCount` and `deltaTime`. `playState` must be `stopped` before any scene write.

### `GET /logs`
Optional `?limit=N` and `?since=<cursor>`. Returns `messages`, `totalBuffered`,
`oldestSequence`, `latestSequence`, `nextCursor`, `hasMore` and `historyLost`.

Each entry carries a monotonic `sequence`. With `since`, entries strictly after that
cursor are returned oldest-first and `limit` pages forward without skipping; without
it, `limit` returns the most recent N. Follow `nextCursor` while `hasMore` is true;
an empty page leaves the cursor where it was. Pine retains 256 entries, so
`historyLost: true` reports that the cursor predates the retained history (surviving
entries are still returned). Cursors are valid only within one server process — one
ahead of the current history returns 409, malformed ones 400. All severities are
returned so a client can filter without breaking cursor continuity.

### `GET /stats`
Level and Game context counters, sizes, render times, and tracked profiling scopes.
Counters include `drawCalls`, `vertexCount` (vertices submitted, so index count for
an indexed draw), `visible`/`culledObjects` for the model batch, and
`visible`/`culledTerrainChunks` for terrain, which culls per chunk rather than per
component. Each profiling scope has `name` (full signature), `shortName`, `parent`
(the calling scope's name, empty at top level), `time` (last frame's total, summed
over every call that frame), `smoothedTime` and `callCount`. Treat these as
diagnostics, not as assertions about what is on screen.

## Inspect the scene

### `GET /entities`
The whole hierarchy: `entities` (roots, with full objects nested recursively under
`children`) and `count`. Components are listed by type name only.

`entities` contains **roots**, not a flat list, and `count` includes temporary editor
entities. Counting the top-level array makes a large grouped scene look like it has
two objects.

### `GET /entity`
`?id=<entity-id>` or `?internalId=<pool-slot>`. Returns identity, parent and
children, flags, tags, component IDs, the serialized `data` dump and the writable
`properties` objects. Returns 404 for a deleted ID.

`data` is the binary serializer's one-way dump. It can contain numeric enums and
opaque fields and is **not** the editing format. `properties` is; see
[Writable-state readback](#writable-state-readback).

### `POST /entities/query`
Filtered or explicitly batched entity reads without a capture or a visible viewport.
This is the route for "fetch all the lights with their properties", "read these
twelve IDs" and "what is near this point".

Either supply `entities` — 1–128 references, read in request order, duplicates
preserved — or a `filter`. An explicit batch rejects the whole request if any
reference is invalid, missing or beneath a temporary entity, cannot supply `filter`
or `limit`, and is never truncated. Batch refs and pool slots are not accepted.

```json
{
  "filter": {"component": "Light"},
  "include": {"components": ["Light"], "properties": true, "worldTransform": true},
  "limit": 128
}
```

`filter` fields combine with AND, and `{}` selects every scene entity:

| Field | Meaning |
| --- | --- |
| `component` | Exact engine type name (`Light`, `ModelRenderer`, `TerrainRenderer`, …). Tests membership, including types with no editing adapter. Explicit 2D component queries are rejected. |
| `name` | `{"value":"lamp","match":"contains"}`. `match` is `exact` (default) or `contains`; both are case-sensitive byte comparisons — no regex, wildcards or Unicode normalization. |
| `hierarchy` | `{"root":{"id":"…"},"mode":"descendants","includeRoot":false}`. `mode` is `descendants` (default) or direct `children`. `includeRoot` adds the root as a candidate, still subject to the other filters. |
| `includeInactive` | Default `true`. When false, excludes inactive entities, requires an active matching component for `component`, and excludes inactive geometry from bounds tests. |
| `spatial` | Pivot or bounds selection; see below. |

`include` chooses what comes back beyond the always-present `id`, `internalId`,
`name`, `active`, `static`, `temporary`, `parent` and `components`:

- `components`: up to 32 exact type names. Omit for all component identities, `[]`
  for none. This controls output independently of the membership filter; entries
  keep entity component order including repeated types, and an absent requested type
  simply has no entry.
- `properties` (default false): adds entity and selected-component writable
  properties. Unsupported adapters report `properties: null`. No `data` dump.
- `worldTransform`, `localTransform` (both default false): each adds
  `{position, rotation, scale}` from the same fresh accessors as `/spatial/query`.

`spatial` requires `test` — `pivot` tests the entity's world position (so it finds
lights and empty groups), `bounds` tests the union of its own model and terrain world
AABBs (so entities without geometry never match, and children are separate candidates
rather than being aggregated into their parent). Supply exactly one shape:

- `radius`: `{"center":{…},"distance":…}` with a nonnegative distance. In bounds mode
  this is sphere–AABB intersection via the closest point on the box.
- `bounds`: `{"min":{…},"max":{…}}` with `min <= max` on every axis. AABB overlap.

Zero radii and zero-size boxes are allowed; touching counts as a match.

Responses carry `sceneGeneration`, `total` (matches before limits), `truncated` and
`entities`. Filtered queries take `limit` 1–128 (default 128). Bodies are capped at
16 KiB and eight nesting levels; successful responses are capped at **1 MiB**, and a
filtered query stops at a whole-entity boundary when either limit is hit. If even the
first result will not fit, 413 tells you to narrow the projection. There is no
pagination and no spatial index — narrow the filter instead. Results follow current
scene-list order, which is repeatable while the scene is unchanged but is not a
persistent ordering. An optional expected `sceneGeneration` in the request returns
409 if it differs.

## Measure geometry

All three routes are reads: they leave selection, camera, scene, history and unsaved
state untouched, reject retry headers, sample one main-thread execution, and return
`sceneGeneration`. Re-query after changing the geometry a placement depends on.

### `POST /spatial/query`
Bounds, dimensions, transforms and orientation axes for 1–128 entities, without
moving the camera and without a visible viewport. Works in stopped mode and during
play.

```json
{"entities": [{"id": "<entity-id>"}], "includeChildren": true}
```

`includeChildren` defaults to true and measures the entity plus its non-temporary
descendants. Each result has `id`, `bounds` (`min`, `max`, `center`, `dimensions`),
`localTransform`, `worldTransform`, and world-space `forward`/`right`/`up` axes
(local −Z, +X, +Y, without scale or its mirroring). The response also has
`combinedBounds`, the union of all non-null result bounds.

`bounds` is `null` when the entity and its included descendants have no supported
geometry — a light, an empty group or an unassigned model does not pretend to have a
size. A model flattened by zero scale still has non-null bounds with a zero
dimension. Transforms always describe the requested entity itself, even when bounds
include children.

### `POST /spatial/raycast`
Nearest surface along a ray, in **stopped mode only** (409 while playing or paused).
No Collider, camera or viewport required.

```json
{
  "origin": {"x": 10, "y": 20, "z": 5},
  "direction": {"x": 0, "y": -1, "z": 0},
  "maxDistance": 50,
  "exclude": [{"id": "<prop-id>"}]
}
```

`origin`, `direction` and `maxDistance` are required; the direction is normalized by
the server, so distance is independent of its magnitude, and `maxDistance` must be
positive. The closest surface in `[0, maxDistance]` wins; a miss is a successful
`hit: null`.

`hit` carries `entity`, `component`, `asset`, `geometry` (`model-surface` or
`terrain-surface`), `meshIndex` (zero-based, null for terrain), `distance`,
`position` and `normal`. Normals are geometric face normals flipped to oppose the
ray — not interpolated shading normals or normal-map values. Faces can be hit from
either side, including from inside a closed mesh. Degenerate triangles and rays
parallel to a face do not intersect. Coincident hits keep the first entity
encountered; that tie order is not stable across scene replacements.

### `POST /spatial/overlap`
Entities whose world bounds overlap a box, in stopped mode only.

```json
{"bounds": {"min": {"x": 8, "y": 0, "z": 3}, "max": {"x": 12, "y": 4, "z": 7}}, "limit": 128}
```

`bounds` is required with `min <= max` per axis; touching faces, edges and points
count, and a zero-size box is allowed. `limit` is 1–128, default 128. Each entity is
tested once against the union of its **own** eligible model and terrain bounds,
without aggregating children. Results carry `total` and `truncated`, follow
scene-list order, and are not paginated.

### Optional fields on raycast and overlap

- `includeInactive` (default `false`): when false, the entity and component must
  themselves be active, following Pine's `IsWorldEnabled` semantics — a parent's
  flag does not implicitly disable descendants. Static flags never affect queries.
- `exclude`: up to 128 `{"id":"…"}` references, each excluding that entity **and its
  descendants** — useful for probing the floor beneath a prop without hitting the
  prop. Missing, stale, temporary and batch references reject the whole query.

### Geometry coverage and its limits

This applies to `/spatial/*`, the spatial filter on `/entities/query`, and
`/camera/frame`:

- **ModelRenderer** contributes the live model's local bounding box, or the selected
  mesh's box when `MeshIndex != -1`, with all eight corners transformed by current
  world position, rotation and scale including parents. Negative and zero scales are
  supported. Nothing reads stale renderer bounds or cached matrices, so a measurement
  is correct immediately after an edit. Raycasts additionally read current uploaded
  triangle positions and indices, including procedural meshes and `UpdateVertices`
  changes, with no frustum or back-face culling.
- **TerrainRenderer** contributes the union of live chunk bounds, or for raycasts the
  live finest-resolution height field with terrain authoring's triangle split.
  Sculpting is visible immediately, before chunk meshes rebuild. Only the entity's
  world *translation* applies — as in the terrain renderer, rotation and scale do
  not. Crack-hiding skirts and LOD simplification are excluded.
- Model and terrain are measured regardless of active/static flags and camera
  visibility (the activation filters above are the exception). **Colliders, sprites,
  particles, editor overlays, material alpha, transparency and shader deformation are
  never measured**, including for occlusion.
- Non-finite spatial state or an invalid `MeshIndex` returns 400 rather than a
  misleading number.

A bounding box is an **enclosing box**, not a surface, not a collision test and not
proof of free space. A doorway's box includes its opening; a rotated prop's world box
is larger than the prop; two meshes on one entity enclose the gap between them. Use a
raycast when you need a point on an actual surface.

Raycasts read model geometry synchronously from the GPU, once per distinct mesh per
request, with no cross-request cache. Work is capped at **64 MiB** of position/index
data and **2,097,152 triangle tests** across every eligible instance; exceeding it,
or a failed mesh readback, returns 409 with no partial hit. Exclude unrelated
hierarchies to cut the work. There is no spatial acceleration index. Intersection
uses doubles over float data, so contacts are approximate far from the origin —
allow placement clearance.

## Edit the scene

### `GET /edit/schema`
The machine-readable description of `POST /edit` for **this running build**: request
envelope, every operation's fields, reference rules, component properties and
defaults, limits, history policy and capability flags. Read it at the start of a
session and again after an Editor restart; prefer it over any hardcoded assumption
about what this build supports.

### `POST /edit`
The batch scene-editing route. Fully documented in [POST /edit](#post-edit-1) below.

### `GET /history`
`undoCount`, `redoCount`, `limit` (128) and `sceneGeneration`.

### `POST /history/undo`, `POST /history/redo`
Empty body or `{}`. Returns history counts, `applied` and an observation token. An
empty history is a successful `applied: false`. Unknown body fields are rejected.

Debug batches share the editor's 128-entry undo stack with Ctrl+Z/Ctrl+Y, and a new
edit after undo discards the redo branch. A held UI component edit is finished before
a debug batch samples its starting state.

**What a step restores:** names, entity active/static flags and tags, local
transforms, model and material references, mesh indices, light properties, component
active flags, renderer stencil settings, perspective Camera properties, the selected
game camera, and parent/child, component and scene listing order. Unchanged values
are not rewritten. Creation, duplication, component removal and entity deletion
restore their **original persistent entity and component IDs**; pool slots, raw
pointers and managed objects do not survive, so reacquire objects by ID. Batch `ref`
names and retained HTTP results are historical request results and are not rewritten.

Restoration goes through the same component adapters and public setters as editing,
plus public entity lifecycle operations — never `LoadData()` onto a live component.
Transform hierarchies and rendering caches are dirtied as needed and recreated
components get fresh caches. Undo does not restore editor selection or camera
navigation.

**When it fails.** Before restoring, history checks entity state, scene membership
and ordering, asset and property validity, and pool capacity. Direct UI or native
changes outside recorded commands can make a snapshot inapplicable. A restoration
failure returns 500 with `historyCleared: true`, `stateMayHaveChanged: true` and an
observation token, and clears both stacks. A preflight failure changes nothing; a
failure during application can leave partial restoration.

Loading or replacing a scene, including stopping play mode, invalidates history
through its scene generation. Saving does not.

### `POST /terrain/sculpt`
One brush stroke against a terrain, recorded as **one undo step** that
`/history/undo` reverses exactly. `?path=` or `?id=` names the terrain; everything
else is the JSON body. Stopped mode only.

| Field | Default | Meaning |
| --- | --- | --- |
| `mode` | `raise` | `raise`, `lower`, `smooth` (towards the neighbouring samples' average), `flatten` (towards one height) or `paint` (towards one layer). The first four move the height field; `paint` writes layer weights without moving the ground. |
| `x`, `z` | — | One terrain-local point for a single dab. Mutually exclusive with `points`. |
| `points` | — | Up to 256 `{"x":…,"z":…}` for a drag. Every point gets a full `duration`, so a longer stroke moves the ground further — as holding the brush still for more frames would. |
| `radius` | `8` | World units. The brush is round, not square; samples further out are untouched. |
| `strength` | `8` | Per second. For the height modes that is world units, and `smooth`/`flatten` move a sample *towards* their target by at most this much. For `paint` it is the share of the layer handed over, approaching full coverage rather than reaching it. |
| `falloff` | `0.5` | How much of the radius is soft edge, measured **inwards from the rim**. `0` is a hard-edged stamp with a flat top, `1` a dome peaking under the point. |
| `duration` | `0.1` | Seconds of brush time per point, standing in for the frame time a dragged stroke accumulates. |
| `height` | — | Reference height for `flatten`. Omitted, it levels to the ground under the first point. |
| `layer` | `0` | Splat channel for `paint`, `0`–`3`. Rejected outside that range whatever the mode is, rather than clamped. |

The reply carries the terrain path, resolved `mode` (plus `layer` when painting),
`requestedPoints`/`appliedPoints` and the resulting history counts. **Fewer applied
points than requested is a normal answer** — part of the stroke fell outside the
terrain — and zero means none of it landed, which is how you learn your coordinates
are not on that terrain. A stroke that changes nothing records no undo step, so it
cannot consume the next undo.

Read the result back with `GET /terrain?x=&z=`.

## Assets and terrain data

### `GET /assets`
Optional `?type=Model` (case-insensitive type name). All loaded assets sorted by
virtual path, each with `path`, `type`, `uid` and `modified`. Includes engine and
editor assets. There is no server-side name search and no pagination — fetch once,
cache it, and search locally.

### `GET /asset`
`?path=<virtual-path>` or `?id=<asset-id>`. The stored asset JSON under `content`,
plus `file`, identity and `modified`. This reads the compressed file from disk, so
it requires an existing readable file and describes the **stored** asset — if
`modified` is true the live asset differs.

For a model, `content.Data.Meshes[]` carries `BoundingBoxMin`, `BoundingBoxMax` and
material IDs. Geometry buffers are opaque size descriptors, not vertex arrays.

### `GET /terrain`
`?path=` or `?id=`, optionally `?x=&z=`. Terrain layout: chunk grid, origin, sample
field, height range, detail level count, and per chunk its coordinate, terrain-local
bounds, dirty state and the lights occupying its slots (`lights.point` /
`lights.spot`, by entity name, nearest first, empty slots omitted). `layers` is one
entry per splat channel — the material's virtual path, or `null` for an unassigned
slot — and `splatMapReady` says whether the render path has uploaded the weight field.

With `?x=&z=` it also returns the interpolated height at that terrain-local point
(`null` off the terrain) and `layerWeights`, the four stored weights at the **nearest
sample**. Nearest rather than interpolated, because what a caller asserts on is what
was painted. Heights are interpolated across the same triangle the mesh is built
from, so they are exact rather than approximate.

### `POST /assets/import`
Import one local source file through the editor's own import context: copy the source
into `content/`, write the `.passet` under `assets/`, register the asset and refresh
the asset browser. Synchronous on the main thread — a large import blocks frames.

```json
{"source": "/tmp/generated/prop.glb", "directory": "models/props", "overwrite": false}
```

- `source` is a required regular file the Editor can read. Absolute paths are
  recommended; relative ones resolve from its working directory (normally `data/`).
  It does not need to be inside the project.
- `directory` defaults to the project asset root (`""` also selects the root). Use a
  lowercase project-relative directory with no trailing slash, no `.`/`..` components
  and no symbolic links. Missing directories are created. It does not depend on the
  asset browser selection.
- The source filename supplies the asset name through the importer's existing path
  rules: this example gives virtual path `models/props/prop`, file
  `assets/models/props/prop.passet` and source copy `content/models-props-prop.glb`.
- `overwrite` defaults to false. True updates a loaded asset of the same type at the
  destination, preserving its ID. An unloaded `.passet` or an asset of another type
  remains a conflict.

HTTP 200 means every queued entry completed. `imports` lists the main asset first,
then dependencies the importer queued, each with `source`, `action`, `status`,
`type`, `id`, `path`, `file` and `sources`. The asset ID is immediately usable in a
ModelRenderer edit.

Bodies are capped at 16 KiB and eight nesting levels. Directories, multiple sources,
custom import settings and uploads are not accepted; registered file types and
default settings come from Pine's importer, and updates retain existing settings.
Finish or cancel any pending UI import dialog first.

HTTP 500 means execution failed **with possible partial effects** — source copies,
dependency assets or changes to an existing asset may already exist. There is no
rollback and no import undo step. Inspect `/logs` for importer diagnostics.

## Levels

### `GET /level/status`
`path`, `id`, `hasDestination` and `unsavedChanges`. During play `unsavedChanges` is
`null` with a reason, since runtime state is not the authored scene.

The comparison serializes the complete scene and diffs it against the active Level's
file, excluding asset header timestamps and IDs. It catches debug edits, undo/redo
and UI edits to serialized scene state — but only serializer-defined authored state,
not every runtime member and not other assets. Untitled or missing files always
report unsaved changes, and a serialization format upgrade makes an older file differ
until it is saved again.

### `POST /level/save`
Empty body or `{}`. Saves the active Level to its current project destination. An
untitled Level returns 409 and needs save-as. Does not save other modified assets.

### `POST /level/save-as`
`{"path": "levels/prototype", "overwrite": false}`. Saves the current scene to that
project-relative virtual path — lowercase, relative to the project's `assets/`, no
`.passet` extension — and makes it the active Level. Missing parent directories are
created. Absolute paths, traversal, symbolic links, noncanonical separators and
conflicts with another asset type are rejected; the destination always stays inside
the project's assets directory.

`overwrite: false` rejects an existing destination, including one created by another
writer after validation. `overwrite: true` requires the destination to be a loaded
project Level backed by a regular file, and preserves its asset ID; a new destination
gets a new ID. A `.passet` that appeared outside the asset manager must be loaded
before it can be overwritten.

**Save-as copies the current scene; it does not create an empty one.** Saving to a
new path is therefore the safe way to start a prototype from the current scene
without touching the original Level file.

Both routes serialize the live scene through Pine's Level/Blueprint serialization,
compress to a temporary sibling file, verify it, then install it at the destination —
the old file survives a write or verification failure. Success returns `path`, `id`,
`fileWritten: true` and `unsavedChanges: false`. A 500 carries `fileWritten` and
`stateMayHaveChanged`: the file can have been written even if a later asset
registration step failed. Saving is not undoable and undo never rewrites files.

### `POST /level/load`
`{"path": "levels/prototype"}`, or `?path=…`. Loads an already-loaded Level asset,
replacing the scene entities. **It does not guard against unsaved changes.**

Loading creates fresh scene IDs, clears history and invalidates every observation
token and picking capture. Reacquire IDs from `/entities` and `/entity` afterwards.
Hierarchy, writable component properties and asset references survive a save/reload
round trip; entity tags and component active flags are preserved in Blueprint capture,
and older files without a component active field load it as true.

### `GET /level/camera`, `POST /level/camera`
Which scene Camera renders the Game viewport. GET returns `target`, `component` and
`sceneGeneration`, each identity being `{"id":"<persistent-id>"}` or null. POST takes
`{"target":{"id":"<entity-id>"}}` to select that entity's **first** Camera, or
`{"target":null}` to clear it.

The target is an entity ID, never a component ID or batch ref. Selecting the first
Camera matches the Level Properties panel and Level serialization. Selection requires
valid perspective properties. Missing entities, temporary hierarchies, entities
without a Camera, unknown fields and malformed references return 400 without changing
selection or history. Bodies are capped at 4 KiB and eight nesting levels.

Creating or duplicating a Camera does not select it. Each selection or clear is one
undo step, including reselecting the current camera. Removing the selected Camera or
deleting its hierarchy clears the rendering reference; undo recreates the original
IDs and restores the selection, and redo clears it again. Selection undo/redo checks
that intervening untracked UI changes have not replaced the expected camera.

Newly captured Levels set `CameraUsesSerializedOrder: true` and store `Camera` as a
one-based index in serialized root/descendant order (zero means none), so reparenting
and editor-only entities no longer shift the selection. Older assets keep the legacy
interpretation until captured again, and an existing incorrect legacy index cannot be
reconstructed automatically.

## Cameras, capture and picking

### `GET /camera`, `POST /camera`, `POST /camera/frame`
The **Level viewport's editor camera**. These do not touch any scene Camera component
and do not select the game camera. Available during play as well as stopped.

All three return 409 in 2D/orthographic mode — select 3D in the viewport first — and
writes return 409 while mouse navigation is captured, so release the right mouse
button. Bodies are capped at 16 KiB and eight nesting levels; unknown fields are
rejected, and all validation finishes before any camera state changes.

`GET /camera` returns a `state` object (`position`, `rotation`, `fieldOfView`,
`nearPlane`, `farPlane`) plus `forward`, `up`, `viewport` (`width`, `height`,
`active`) and `mouseCaptured`. **Save the `state` object** and post it back to
restore that view; the enclosing fields are read-only.

`POST /camera` accepts any subset of the state fields — omitted fields keep their
current values. `lookAt` is a world-space target that orients the camera from its
resulting position; it cannot be combined with `rotation`. An optional `up` controls
roll, requires `lookAt`, and must be nonzero and not parallel to the view direction;
without it world +Y is used, with −Z as the fallback for top and bottom views. A
target equal to the resulting camera position is rejected. Field of view is vertical
in degrees, 1–175. Near must be at least 0.0001 and far must exceed near in float32.
Inputs producing non-finite matrices, or positions so large that float32 loses the
requested direction, are rejected.

Applying a view clears the fly camera's residual movement and synchronizes its
mouse-navigation angles including roll, so returning to mouse navigation continues
from the applied orientation.

`POST /camera/frame` fits entities in the view:

```json
{"entities": [{"id": "<entity-id>"}], "includeChildren": true, "padding": 1.2,
 "direction": {"x": -1, "y": -0.5, "z": -1}}
```

1–128 existing scene entity IDs. `includeChildren` defaults to true — framing an
empty parent then frames the models beneath it. `padding` defaults to 1.2, accepts
1–10, and multiplies the projected extents. `direction` is the direction the camera
**looks**, not its offset from the selection; omit it to keep the current rotation.
Optional `up` requires `direction`. For a top view use `{"x":0,"y":-1,"z":0}`.

Framing centers the combined world bounding box and fits its eight corners within
both fields of view, preserving the existing clipping planes; if the selection will
not fit, raise `farPlane` and try again. The Level viewport must be active with a
nonzero size, or the endpoint returns 409 because it cannot determine the aspect
ratio. ModelRenderers are included regardless of activation. An entity with no model
geometry among itself and its considered descendants contributes its **pivot**, and a
pivot-only selection is viewed from one unit away. Terrain, physics shapes and 2D
geometry are not measured — `/spatial/query` is the route that covers terrain and
reports null for missing geometry. The response is the `/camera` description plus
`framedBounds`.

### `GET /viewport.png`
Optional `?view=level|game` and `?width=N`. An immediate PNG of the active viewport,
with **no ordering guarantee** relative to an edit. Use `/observe` when the frame
must come after a change.

### `POST /observe`
Capture the next rendered **3D perspective** Level or Game view and return its PNG,
frame identity, camera metadata, selected entity state and incremental logs in one
response. The main thread never waits for another frame.

```json
{
  "after": {"session": "…", "sceneGeneration": 1, "revision": 3, "frame": 100, "logsSince": 42},
  "view": "level",
  "width": 800,
  "entities": ["<entity-id>"]
}
```

Copy an `observationToken` verbatim into `after`; never construct one. `view`
defaults to `level`. `width` accepts 1–4096 and only **downsizes**, preserving aspect
ratio — omit for native resolution. `entities` accepts up to 128 existing scene
entity IDs, default none. An optional top-level `logsSince` overrides the token's
cursor, which is how you capture a later camera-framing token while covering logs
from the earlier edit. `{}` is valid and means "a fresh view after this request was
accepted", with logs from acceptance — which can miss errors from earlier edits.
Bodies are capped at 16 KiB and eight nesting levels.

The response contains:

- `after`: the requested token, or the one established at acceptance.
- `frame`: `session`, monotonic `id`, `sceneGeneration`, `revision`.
- `viewport`: `view`, native rendered `width` and `height`.
- `camera`: component `id`, world `position` and `rotation`, vertical `fieldOfView`,
  clipping planes, and `viewMatrix`/`projectionMatrix` as arrays of four **columns**
  of four numbers.
- `image`: `contentType`, `encoding: "base64"`, output dimensions and base64 `data`.
  Decode it to a file and look at it; never print the payload.
- `entities`: the same descriptions as `GET /entity`, including `data` and writable
  `properties`.
- `picking`: `null` unless requested; see below.
- `logs`: the incremental `/logs` response.
- `timing`: explicit sampling phases for camera, image, entities and logs.

**Timing.** An observation always waits for a frame after its *acceptance*, even if
the named operation has already rendered. Camera parameters, matrices and viewport
dimensions are copied at that viewport's render callback, after camera preparation
and before the color passes. Pixels are read after rendering finishes, **before ImGui
and queued debug writes**, so later UI changes cannot contaminate the metadata.
Entity state including writable properties is sampled at the same post-render
boundary. Logs are copied under the log mutex after capture, so asynchronous messages
arriving during rendering or PNG encoding mean logs are not an atomic snapshot.

**409 explicitly rejects:** a token from another session or a replaced scene (every
scene reset invalidates old tokens, including reloading the same Level and restoring
the play-mode snapshot — and a reset while waiting also fails); a hidden or inactive
viewport, a missing camera or framebuffer, an invalid render size, or a
non-perspective view — no stale framebuffer is ever substituted; and a requested
entity that disappears while the observation is pending. A Game capture also needs a
selected scene Camera.

### `POST /pick`
Read the model surface under a pixel of a **retained capture**. First request an
observation with `"picking": true` while stopped; ordinary observations return
`picking: null` and render no picking buffers. `/viewport.png` images cannot be
queried.

```json
{"capture": "<observation.picking.capture>", "pixel": {"x": 400, "y": 225}}
```

Returns `capture`, `frame`, `pixel`, `geometry: "model-surfaces"` and `hit`, where a
hit carries `entity`, `component`, `model`, `meshIndex`, `position` and `normal`.
`hit: null` is a successful miss. Normals are geometric face normals facing the
captured view. Accepts no query parameters, at most 4 KiB of JSON and four nesting
levels.

Pixels are integer indices in the **returned PNG** with `(0,0)` top-left, X right and
Y down, sampled at the pixel centre `(x+0.5, y+0.5)`, and must be inside
`image.width`/`image.height`. When an observation is downsized, picking uses a sample
grid of that output size with the original projection, without averaging IDs, depth
or normals — and because the PNG is resized with filtering, an edge pixel can show
several surfaces' colors while its centre picks exactly one. Prefer interior pixels.

**A capture is historical.** It retains depth, face normals and mesh identities with
the observation's saved camera matrices, and picking does not rerender. Camera
navigation, resizing, moving or deleting objects, history changes and re-imports
cannot change a retained answer — and the identities it returns may no longer exist.
Acquire a fresh observation before placing against changed geometry.

Retention is bounded by **all** of: 120 seconds from creation (a pick does not
refresh it); four captures, oldest evicted first; 64 MiB of retained payload; and
2,097,152 pixels per capture with at most 4096 per dimension and 16,384 visible model
meshes. A larger request returns 409 — reduce the observation `width`. The `picking`
object advertises `capture`, `geometry`, `width`, `height`, `retentionSeconds`,
`maximumCaptures` and `maximumRetainedBytes`. A reference is valid only within its
server process and scene generation; level replacement and Stop's scene restoration
invalidate it. Unavailable, expired or evicted references return 409.

**Coverage.** Picking uses live GPU mesh buffers for **ModelRenderer triangles**,
needs no Collider, and works in stopped edit mode only (runtime captures including
paused play are rejected). It respects active entities and components, model
visibility, `MeshIndex`, actual world matrices and back-face culling, and handles
indexed and non-indexed meshes. It does **not** reproduce material alpha cutouts,
transparency, shader vertex deformation, stencil effects, normal maps, wireframe
display or post-processing. Terrain, sprites, particles and editor overlays are
excluded **including from occlusion**, so a model behind one of them can be returned
and a miss does not mean the pixel is empty. Normals are half-float octahedral and
positions are reconstructed from raster depth, so both have finite precision — allow
clearance.

## Request control

These run on HTTP workers against the synchronized request registry and do not wait
for the main thread.

### `GET /requests`
Without `?id=`: the server `session`, `timeoutSeconds` (5), `retentionSeconds` (600),
`maxRetainedRequests` (256) and `maxActiveRequests` (256). The session matches the
one in observation tokens; it changes when the Editor restarts but survives level
replacement and play-mode scene restoration.

With `?id=<key>` and the `X-Pine-Session` header: the retained record. Returns 200
with `request` and, once terminal, `result: {"status": <original HTTP status>,
"body": <original response>}` — **200 here does not mean the operation succeeded**,
only that the lookup did. Unknown IDs return 404 with `state: "unknown"`. An ID from
a different session returns 409 with `currentSession`. Neither proves the old
operation did not run.

### `POST /requests/cancel`
`?id=<key>` with `X-Pine-Session`, no body and no idempotency key. Cancelling pending
work returns its terminal snapshot and wakes any waiting caller with a retained 409;
repeating it returns the same snapshot. Cancelling completed work returns the
existing result and has no scene effect. **Running work returns 409 and continues** —
cancellation never interrupts a setter or rolls anything back.

---

# POST /edit

One request, one validated batch, **one undo step**. This is the only route that
writes scene structure.

## Envelope

```json
{"version": 1, "operations": [ … ]}
```

Limits are **128 operations, 256 KiB of JSON and 32 nesting levels** per request,
plus at most **1024 duplicated entities** across a batch. `/edit/schema` advertises
the live values; object pools impose their own separate capacity limits.

Operations execute in array order, and each one is validated against the state
**proposed** by the operations before it. The entire batch is validated before
anything mutates.

## Operations

| Operation | Target | Effect |
| --- | --- | --- |
| `entity.create` | — | Create an entity. Optional `name` (default `Entity`), `ref`, `parent` and `components`. |
| `entity.update` | entity ID | Patch `name`, `active`, `static`. |
| `entity.reparent` | entity ID | Move the entity and its hierarchy under a new parent, or to the scene root. |
| `entity.delete` | entity ID | Delete the entity and all its descendants. |
| `entity.duplicate` | entity ID | Copy the entity and its hierarchy with fresh IDs, under the same parent. |
| `entity.place` | entity ID | Position against a supplied surface plane, or align bounds with another entity. |
| `entity.aim` | entity ID | Rotate a chosen local axis toward a world point. |
| `component.add` | entity ID | Add a component with optional initial properties. |
| `component.update` | **component ID** | Patch a component's writable properties. |
| `component.remove` | **component ID** | Remove a component. |

Supported component types are **Transform, ModelRenderer, Light, perspective Camera,
primitive Collider and RigidBody**. Each may appear once per entity. Transform always
exists and can be neither added nor removed; everything else advertises `addable` and
`removable` in the schema.

## References

- **Entity and component targets accept only `{"id": "…"}`.** Not refs, not pool
  slots, not the other kind of ID.
- **`parent` additionally accepts `{"ref": "…"}`**, naming an **earlier**
  `entity.create` or `entity.duplicate` root in the same request, or `null` to detach
  to the scene root. Omitting `parent` on a create makes a root; an explicit null
  there is rejected. Reparenting requires the field, where null means detach.
- `ref` names are declared by creations and duplications, must be unique across the
  whole request, and are **parent references only** — never operation targets, never
  a way to address copied descendants, never usable in a later request. Forward,
  self, deleted and previous-request references are rejected. To touch something this
  batch created, use its returned ID in a following request.
- **Asset references** accept exactly one of `{"id": "<uid>"}` or
  `{"path": "<virtual-path>"}`. The asset must already be loaded and match the
  expected type. Responses normalize to IDs. `null` clears a nullable asset property.

## Per-operation contracts

### `entity.create`
`components` configures the automatically created Transform and adds any of the other
supported types. Omit it for a bare Transform. Parenting keeps the supplied local
transform as-is.

### `entity.update`
`properties` is required; every field within it is optional and an empty object is a
no-op. Names are nonempty strings without null characters — Unicode and duplicates
are fine. Flags accept JSON booleans only. Calls `SetName`/`SetActive`/`SetStatic`
and marks the entity dirty. Static entities remain editable through this API.

### `entity.reparent`
Preserves **local** position, rotation and scale exactly. There is no
world-preservation mode and no matrix decomposition — see the hierarchy semantics
under [Conventions](#conventions) for what that means in practice.
`/edit/schema` advertises this at `entity.reparent.transformPreservation`.

Moves the target with all descendants, preserving entity and component IDs, local
values, flags and selection references. It removes the target from the old parent's
child list and appends it to the new one's. Assigning the current parent (including
`null` on a root) is a no-op that preserves sibling order. Cycle validation follows
the proposed hierarchy after each preceding operation: self-parenting and parenting
beneath a descendant reject the whole batch, and a later detach cannot rescue an
earlier cyclic operation — detach first, then move.

### `entity.delete`
Always includes **all descendants at this point in the batch**, matching
`Entities::Delete`. To keep a child, reparent it out first; there is no implicit
promotion to the scene root. Referenced assets stay loaded.

A hierarchy containing any component other than the six supported types is
**rejected before mutation**, because there is no supported undo restoration path for
it. A hierarchy containing a temporary entity is likewise protected.

The result carries `entityId` and `removedEntities`, an array of
`{"id": …, "componentIds": [ … ]}` covering the whole removed hierarchy, captured
before destruction (array order is not destruction order).

Later operations cannot reference deleted entities or their components — deleting an
ancestor then a descendant is rejected, the reverse order is valid. A deleted
creation's `ref` becomes `null` in the response's `refs`, and its name stays reserved
for the request. Affected entities leave the editor selection before destruction, and
a drag originating in the removed hierarchy is cancelled. Deleting the selected game
Camera clears the rendering reference; undo restores it.

After deletion `/entity` returns 404 for that ID, fresh edits and framing requests
using it fail validation, and handles cannot resolve a replacement even after pool
slot reuse. Deletion does **not** replace the scene or change its generation, so
tokens and IDs for surviving entities remain valid — observe the deletion token with
surviving IDs, or omit `entities`.

### `entity.duplicate`
The only optional field is `ref`. Unknown fields are rejected, including parent, name,
offset and children options.

Copies **all descendants at this point in the batch**, reflecting preceding patches,
component additions and removals, reparenting, new descendants and earlier duplicates
within the source hierarchy. Later changes to the source do not affect the copy. The
copied root is appended under the source's current parent (or at the scene root),
child order and all local transforms are preserved — so **the copy initially overlaps
its source** — and names are copied unchanged. To rename or move it, use its returned
IDs in a subsequent request.

Entity active/static flags, tags and component active flags are preserved, as are
ModelRenderer stencil settings and Camera clear color, aspect override and dormant
orthographic size. Rendering caches are fresh and the hierarchy is marked dirty.
Model and material references keep the same loaded asset IDs; assets stay shared.

The batch is rejected if the source hierarchy contains any unsupported component, a
temporary descendant, repeated component types, or a missing or misplaced Transform.
Unsupported components are never silently omitted.

Every copied entity and component gets a **fresh persistent ID**. The result carries
`duplicatedEntities`: every copied node, root first then descendants in breadth-first
child order, each with `sourceId`, `entity` and `componentIds` mapping source
component IDs to copies. Parent links within the copy point at copied parents; the
root keeps its external parent. No reference remapping is promised for scripts or
other unsupported types.

### `entity.place`
Two mutually exclusive forms; `/edit/schema` advertises their required and forbidden
fields under `oneOf`. Both are authoring operations in stopped mode — neither runs
physics or avoids collisions.

**Surface form** — put the entity against a supplied world plane:

```json
{
  "op": "entity.place",
  "target": {"id": "<prop-entity-id>"},
  "surface": {"point": {"x": 10, "y": 2, "z": 5}, "normal": {"x": 0, "y": 1, "z": 0}},
  "anchor": {"type": "modelBounds"},
  "clearance": 0.02
}
```

`surface.normal` points **out of the surface toward the prop**, with magnitude above
`1e-12`; the server normalizes it. Raycast normals oppose the ray, so cast from the
side the prop should sit on. `clearance` is optional, nonnegative, and measured along
the normalized normal. `alignment` is optional — omit it to preserve rotation.

`anchor` is required and picks one of two policies:

- `{"type":"modelBounds"}` measures the target's **own** ModelRenderer geometry (the
  model's local box, or the selected mesh's box when `MeshIndex != -1`; multiple
  renderers combine into one local box) using the proposed rotation and scale
  including parents. It centers that transformed box over `surface.point` in the two
  tangential directions and puts its **minimum projection along the normal** at
  `clearance`, which handles off-center pivots and rotated walls without inflating to
  a world-axis box first. Local scale is preserved. Descendants, terrain, colliders
  and shader deformation contribute nothing, so a group with geometry only on its
  children needs an explicit anchor or per-model placement.
- `{"type":"localPoint","point":{…}}` uses a point in model-local coordinates
  **before scale**, transforms it by the resulting world rotation and scale, and
  translates the entity so the anchor lands at `surface.point + normal * clearance`.
  `{0,0,0}` is the pivot. This works without model geometry and claims nothing about
  the rest of the entity. `point` is required here and forbidden for `modelBounds`.

`alignment` sets both the normal-facing axis and the roll:

```json
"alignment": {"axis": "-Z", "upAxis": "+Y", "up": {"x": 0, "y": 1, "z": 0}}
```

`axis` and `upAxis` each accept `+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z` and must be
perpendicular. `axis` is rotated onto the surface normal; `upAxis` onto the projection
of the normalized world `up` onto the surface plane. `up` needs magnitude above
`1e-12` and its projection length above `1e-6` — parallel inputs are rejected rather
than picking an arbitrary roll. These are rotation axes, independent of scale and
mirroring, matching `/spatial/query` orientation vectors; contact bounds and local
anchors still use signed world scale.

**Relative form** — align world bounds with another entity, then offset:

```json
{
  "op": "entity.place",
  "target": {"id": "<prop-entity-id>"},
  "relativeTo": {"id": "<reference-entity-id>"},
  "boundsAlignment": {
    "x": {"target": "min", "reference": "max"},
    "y": {"target": "min", "reference": "min"},
    "z": {"target": "center", "reference": "center"}
  },
  "offset": {"space": "world", "value": {"x": 0.1, "y": 0, "z": 0}}
}
```

`relativeTo` is an existing entity ID that is neither the target nor its descendant
(moving the target would move the reference); an ancestor is allowed.
`boundsAlignment` needs at least one axis, each with `target` and `reference` chosen
from `min`, `center`, `max`. These are coordinates of **world-axis-aligned model
bounds**, regardless of either entity's rotation; rotation and scale are preserved and
unselected coordinates are kept before the offset. `offset` defaults to zero and needs
both `space` and `value` when supplied — `world` adds the vector directly,
`referenceLocal` rotates it by the reference's world rotation (reference scale and
mirroring never scale or reverse it) — and applies on **all** axes including ones
omitted from `boundsAlignment`.

Both entities need their own assigned ModelRenderer geometry; each renderer's local
box (selected by `MeshIndex`) is transformed before combining, matching
`/spatial/query` for model-only entities with `includeChildren: false`. Active flags
do not affect these bounds.

**Both forms:** the surface or reference is resolved from the state after preceding
operations in the batch, and is a **literal value, not a retained relationship** —
moving the wall or the reference afterwards, even later in the same batch, does not
update the placement. No ongoing attachment is created, and redo restores the recorded
transform without resampling. Placement guarantees bounding-box clearance from the
supplied infinite plane within float precision; it does not establish mesh contact,
support, or clearance from anything else. A raycast on uneven terrain gives one point
and one face normal, and that tangent plane can pass through the ground under a wide
prop — take more samples and look at the capture.

Ancestor rotations must be unit quaternions within `1e-5`; repair invalid native state
through the Transform adapter first. Placement subtracts the parent world position and
converts the chosen world rotation to a local one, following Pine's actual hierarchy
semantics. Children keep their local transforms and are not a placement constraint, so
a later operation moving a parent can move a placed child off the plane.

### `entity.aim`
Rotates an entity so an explicit local axis points from its world position toward a
world point, preserving local position and scale. It needs no model or light. For a
Pine spotlight or scene camera use `forwardAxis: "-Z"`, `upAxis: "+Y"`.

```json
{
  "op": "entity.aim",
  "target": {"id": "<spotlight-entity-id>"},
  "point": {"x": 10, "y": 2, "z": 5},
  "forwardAxis": "-Z",
  "upAxis": "+Y",
  "up": {"x": 0, "y": 1, "z": 0}
}
```

All six fields are required. The distance from the entity's world position to `point`
must exceed `1e-12`. `forwardAxis` and `upAxis` must be perpendicular signed axes; the
up axis follows the projection of world `up` onto the plane perpendicular to the view
direction, with the same `1e-12`/`1e-6` magnitude rules and the same refusal to invent
a roll. No tracking relationship is created — aim again after moving the entity or
rotating its parent. Aiming does not preserve surface contact, so for a mounted lamp,
aim a light child rather than the housing.

### `component.add`, `component.update`, `component.remove`
`component.add` takes an **entity** ID, a `type` and optional `properties` (omitted
means schema defaults), and rejects a type already present on that entity at this
point in the batch. Its result carries `entityId` and
`component: {"id": …, "type": …, "properties": { … }}`.

`component.update` takes a **component** ID and a property patch; it never adds.
`component.remove` takes a component ID and returns `entityId` and `removedComponent`;
it never cascades.

Membership and pool capacity are validated in operation order before any mutation. A
removal frees capacity for later additions, but a later removal cannot rescue an
earlier allocation that would exhaust a pool. Remove-then-add of the same type in one
batch is allowed and creates a **new component identity**. Updating or removing an ID
already removed earlier in the batch rejects the whole request. A scene may contain
multiple components of a type created outside this API: removal targets exactly the
given ID, while addition requires every existing instance to have been removed first.
There is no implicit replacement. Removed IDs stay invalid even when Pine reuses the
pool slot.

## Component properties

Omitted properties keep current values on update and take engine defaults on creation
or addition. A supplied vector **replaces all three coordinates**. Integer properties
reject fractional values. Related properties are validated together, after merging the
patch with the preceding state. `/edit/schema` is authoritative for this build.

| Component | Properties |
| --- | --- |
| Transform | `LocalPosition`, `LocalRotation`, `LocalScale` |
| ModelRenderer | `Model`, `OverrideMaterial`, `MeshIndex` |
| Light | `Type`, `Color`, `Intensity`, `Range`, `CastShadows`, `SpotlightOuterAngle`, `SpotlightInnerAngle` |
| Camera | `Type`, `FieldOfView`, `NearPlane`, `FarPlane` |
| Collider | `Type`, `Position`, `Size`, `Layer`, `LayerMask`, `IsTrigger`, `TriggerMask` |
| RigidBody | `Type`, `Mass`, `GravityEnabled`, `PositionLock`, `RotationLock`, `MaxLinearVelocity`, `MaxAngularVelocity` |

Entity update properties are advertised separately at `entity.properties`: `name`,
`active`, `static`.

**ModelRenderer.** `MeshIndex` is `-1` for all meshes or an index within the selected
model; clearing `Model` requires `MeshIndex` to be `-1` too.

**Light.** `Type` is `Directional`, `PointLight` or `SpotLight`. `Color` is linear
RGB, `Intensity` nonnegative, `Range` at least 0.01 world units. Spotlight half-angles
satisfy `0 <= inner <= outer` with outer between 1 and 89 degrees, checked as a pair
before any setter runs.

**Camera.** `Type` accepts only `Perspective`. `FieldOfView` is vertical, 1–179
degrees, default 70. `NearPlane` (default 0.01) must be positive and less than
`FarPlane` (default 150), checked after float32 conversion including underflow and
planes that round together, and the parameters must produce finite projection and
inverse projection matrices. The viewport supplies the aspect ratio for new cameras;
aspect overrides, clear color and orthographic properties are not exposed, and
position and orientation come from the entity's Transform.

Existing **orthographic** cameras can be inspected but fail this adapter's validation,
so they cannot be edited, removed, duplicated or deleted through the API — history
could not restore them through the same adapter.

**Collider and RigidBody.** Edits change authored properties; the first physics step
after Play creates actors and shapes from them, and Stop destroys them and restores
the pre-play scene. Nothing is live-editable during play, and readback during play
reports configuration rather than an inspection of the backend actor.

- A Collider without a RigidBody makes a **static** actor. A RigidBody uses the
  Collider on the **same entity** and without one stores configuration but creates no
  actor. Neither implies nor cascades to the other, and either addition order works.
- An entity's `static: true` forces a static actor regardless of RigidBody `Type`; the
  configured type is unchanged in readback and serialization.
- One of each per entity. Compound colliders and colliders on children attached to a
  parent's body are not supported.

Collider `Type` is `Box`, `Sphere` or `Capsule` (mesh and heightfield are
unsupported). `Position` is an offset added to the entity's world position which Pine
does **not** rotate or scale. `Size` is multiplied componentwise by entity world scale
at shape creation, and all three coordinates must stay positive in float32 even when
unused — there are no Radius/Height aliases, so patches and history have one
representation.

- **Box:** `Size` is **half-extents**. `{1,1,1}` is two units across at unit scale.
- **Sphere:** `Size.x` is the radius, scaled by world scale x. Y and Z are retained
  but unused; nonuniform scale does not make an ellipsoid.
- **Capsule:** `Size.x` is the radius, `Size.y` the cylindrical section's
  **half-height**, so total height at unit scale is `2 * (Size.y + Size.x)`. It aligns
  with entity Y and rotates with the entity. `Size.z` is retained but unused.

`Layer` (default 1) and `LayerMask` (default 4294967295) are unsigned 32-bit masks;
both objects must allow the other's layer for contact. `IsTrigger` makes a non-solid
shape and `TriggerMask` enables a pair when either trigger's mask allows the other's
layer. Triggers configure backend overlap filtering only — Physics3D installs no
simulation event callback, so there are no gameplay trigger callbacks.

RigidBody `Type` is `Static`, `Kinematic` or `Dynamic` (default). `Mass` is a positive
finite float32 with a finite reciprocal, in kilograms, default 1. `GravityEnabled`
defaults true. `PositionLock` and `RotationLock` use schema type `boolean3`: all three
boolean fields required when supplied, numeric 0/1 and arrays rejected, whole object
replaced. `MaxLinearVelocity` (world units/second) and `MaxAngularVelocity`
(radians/second) are nonnegative, where zero selects the backend default. Values are
stored even when the configured type does not use them.

For **dynamic** simulation use scene-root entities: the engine writes the world
physics pose back as a local Transform, so transformed parents are not handled
correctly by dynamic-body pose synchronization.

## Writable-state readback

`GET /entity`, `POST /entities/query` with `include.properties`, and `POST /observe`
all return the same `properties` objects, and `/edit/schema` advertises where.

- Entity `properties` contains exactly `name`, `active`, `static` — ready for
  `entity.update.properties`.
- Each supported component's `properties` contains everything its adapter advertises,
  ready for `component.update.properties`. Transform values are local, Light types are
  named enums, asset references are `{"id": …}` or null. The same adapter reads these
  and writes editing responses, so a read → edit → read round trip is stable.
- Unsupported components report `properties: null`, as do temporary editor entities
  and everything beneath them.
- The patch deliberately excludes non-editable state: component active flags, tags and
  hierarchy links.

```python
state = get_json('/entity?id=' + entity_id)
light = next(c for c in state['components'] if c['type'] == 'Light')
properties = dict(light['properties'])
properties['Intensity'] = 4
post_json('/edit', {'version': 1, 'operations': [{
    'op': 'component.update',
    'target': {'id': light['id']},
    'properties': properties,
}]})
```

Readback samples live getters without validation and is available during play. Values
from older assets or written by native code or the UI **can violate current edit
constraints**; reads do not repair them and resubmission still goes through normal
validation. Submitting a full property object may overwrite intervening changes —
send only what you meant to change.

## Validation and failure

The whole batch is prepared without mutating live objects: field and type checks,
target and asset resolution, related-property checks, and pool capacity. History also
validates the **original** values it will need to restore, so an invalid legacy or
native value rejects the batch with a `beforeState` validation path — repair it
through its owning subsystem first.

**Validation failure** is HTTP 400 with `phase: "validation"`, `completed: 0`,
`error`, a slash-separated `path` and, for operation-specific errors, the zero-based
`operation` index. Nothing changed, and history is untouched.

**Execution is not atomic.** A failure mid-batch returns HTTP 500 with
`phase: "execution"`, the completed results, the failing operation index and
`failedOperationMayHaveChangedState: true`. Earlier operations stay applied, later
ones are skipped, `history` reports `"cleared"` and **both undo and redo stacks are
cleared — there is no rollback**. When known, the failing operation's created entity
is `createdEntityId`; a `component.add` that attached a component before failing to
apply properties also reports `createdComponentId` and `entityId`. Inspect the live
scene before continuing.

Success is HTTP 200 with `completed` equal to the operation count, `refs` mapping
request names to entity IDs, `results` with per-operation IDs and resulting writable
properties, `history: "recorded"` and an `observationToken`. A batch with no net state
change still records a step.

---

# What the API does not do

Reading serialized state does not imply it can be written. Not currently exposed:

- Ambient, fog and skybox writes; material authoring; Blueprint spawning.
- Play, pause and stop control. (Writes require stopped mode, so drive play from the
  UI.)
- 2D authoring and 2D camera controls.
- Arbitrary component writes — only the six adapted types.
- Viewport tab switching and general UI automation. `/camera/frame` and `/observe`
  need the relevant viewport already open and visible.
- Uploads, directory imports and custom import settings.
- Live physics editing, gameplay trigger callbacks, and picking against terrain or
  material cutouts.
- Pagination, spatial indexing and cross-request caching on the query routes.

# Adding a route

Routes are registered in
[`Endpoints.cpp`](../Editor/src/DebugServer/Endpoints/Endpoints.cpp) via `AddRoute`
for reads and `AddMutationRoute` for writes — the latter adds retry tracking and an
observation token. Request-control routes are registered directly in
[`DebugServer.cpp`](../Editor/src/DebugServer/DebugServer.cpp). Each area owns a
folder under `Editor/src/DebugServer/`.

For `/edit` specifically: `Editing.cpp` prepares and executes batches, `Values/` holds
shared validation and conversion, `Schema/` describes the envelopes and reference
rules, and each `Editing/Components/<Type>/` adapter owns its advertised schema,
reads actual and default values, validates related properties, and applies state
through **public component setters**. Register a new adapter in
`Components::GetAdapters()` and keep its schema description paired with the
preparation change.

Two rules worth stating explicitly:

- **Never apply edits as JSON → binary → `LoadData()`.** That bypasses setter
  behavior for some components. Adapters exist for this reason.
- **Adding a property adapter does not enable lifecycle operations.** Addition and
  removal need `AllowAddRemove`, and duplication needs explicit entries in
  `Duplication::Supports`, `Read`, `Validate` and `ApplyComponent`. Evaluate
  dependency validation, pool accounting and both creation and destruction side
  effects before opting a type in.

When you add or change a route, update this page and the
[workflow guide](debug-server-workflow.md) if it changes how a scene gets built.

# Editor API guide

Use the Editor's localhost HTTP API to inspect a project, assemble a 3D scene,
position cameras, capture the rendered result, and save a Level. This is the
starting point for agents and scripts operating the Editor. The linked feature
docs describe the exact validation and failure contracts.

- [Connect and inspect](#connect-and-inspect-first)
- [All routes](#complete-route-reference)
- [Scene-building workflow](#build-a-scene-efficiently)
- [Blender asset sub-agents](#blender-asset-sub-agents)
- [Limits and troubleshooting](#limits-and-troubleshooting)

## Connect and inspect first

Reuse a running Editor when one is available. The server is disabled unless
`PINE_DEBUG_SERVER` is set; `PINE_DEBUG_SERVER=1` enables **127.0.0.1:9002**.
A value such as `19023` selects another port. When launching an Editor, run from
`data/` and pass the project name, as described in [project setup](data-and-projects.md).
This is an unauthenticated loopback service; keep it local.

```sh
curl --silent --show-error --fail-with-body http://127.0.0.1:9002/status
```

The health/state route is `/status`. Check HTTP status as well as the response
body: plain `curl -sS` can exit successfully on an HTTP error, and an unknown route
can return an empty body. If a sandbox cannot reach the user's running Editor,
check whether the client and Editor share the same network environment before
assuming the Editor has stopped.

Read these at the start of a session; independent reads can be requested together:

| Route | What to establish |
| --- | --- |
| `/status` | Expected project and active Level; `playState` must be `stopped` for scene writes. `worldPaused: true` alone is insufficient. |
| `/level/status` | Current destination and unsaved changes before loading or saving anything. |
| `/entities` | Existing scene hierarchy, including temporary editor entities. |
| `/camera` | Current Level view; retain its `state` object if the view should be restored. |
| `/edit/schema` | Supported operations, component properties, defaults, limits and reference forms in this running build. |
| `/requests` | Server session and retry/deadline limits. |
| `/logs?limit=1` | A starting `nextCursor` for subsequent incremental logs. |

Save full responses to local files and print summaries. Refresh the schema and
session after an Editor restart; refresh scene IDs after a Level replacement.

## Complete route reference

Query values must be URL-encoded. POST bodies are JSON with
`Content-Type: application/json`, except request cancellation, which has no body.
Examples below use placeholders where a live ID or project asset is required.

### Inspection and asset discovery

| Method and route | Input | Output / purpose |
| --- | --- | --- |
| `GET /status` | None | Project, active Level, play state, world pause, entity count and frame delta. |
| `GET /entities` | None | Recursive root `entities` tree and total `count`; components are type names only. |
| `GET /entity` | `?id=<entity-id>` or `?internalId=<pool-slot>` | Entity identity, parent/children, flags, tags, component IDs, serialized `data` and writable `properties`. Prefer persistent IDs over pool slots. |
| `GET /assets` | Optional `?type=Model` (case-insensitive type name) | Loaded assets sorted by virtual path: `path`, `type`, `uid`, `modified`. Includes engine/editor assets. No server-side name search or pagination. |
| `GET /asset` | `?path=<virtual-path>` or `?id=<asset-id>` | Stored asset JSON under `content`, plus `file`, identity and `modified`. Reads the compressed file; requires an existing readable file. |
| `GET /terrain` | `?path=<virtual-path>` or `?id=<asset-id>`, optional `?x=&z=` | Terrain layout (chunk grid, origin, sample field, height range, detail level count) and per-chunk coordinate, terrain-local bounds, dirty state and the lights occupying the chunk's slots (`lights.point` / `lights.spot`, by entity name, nearest first, empty slots omitted). `layers` is one entry per splat channel, the material's virtual path or `null` for an unassigned slot, and `splatMapReady` says whether the render path has uploaded the weight field yet. With `?x=&z=` also the interpolated height at that terrain-local point, `null` when the point is off the terrain, and `layerWeights` — the four stored weights at the nearest `sample` to it, nearest rather than interpolated because what a caller asserts on is what was painted. |
| `GET /logs` | Optional `?limit=N`, `?since=<cursor>` | Messages, sequence cursors, `hasMore` and `historyLost`. See [log paging](debug-server-observation.md#incremental-logs). |
| `GET /stats` | None | Level/Game context counters, sizes, render times and tracked profiling scopes. Counters include `drawCalls` and `vertexCount` (vertices submitted, so index count per indexed draw), `visible/culledObjects` for the model batch, and `visible/culledTerrainChunks` for terrain, which culls per chunk rather than per component. Each scope carries `name` (the full signature), `shortName`, `parent` (the calling scope's `name`, empty at the top level), `time` (the last frame's total, summed over every call of that frame), `smoothedTime` and `callCount`. |
| `GET /edit/schema` | None | Versioned edit envelopes, operations, components, references, limits, history and scene-camera discovery. |

### Scene and asset writes

| Method and route | Body | Effect |
| --- | --- | --- |
| `POST /edit` | `{"version":1,"operations":[...]}` | Apply a validated batch; return `refs`, operation results, IDs, history status and observation token. |
| `POST /assets/import` | `{"source":"/absolute/path/prop.glb","directory":"models/props","overwrite":false}` | Import one Editor-accessible file and dependencies; copy sources into the project and load assets. No upload or import-settings API. [Import details](debug-server-import.md). |
| `GET /history` | — | Undo/redo counts, limit and scene generation. |
| `POST /history/undo` | `{}` or empty | Undo one recorded step; `applied: false` if empty. |
| `POST /history/redo` | `{}` or empty | Redo one recorded step; `applied: false` if empty. |
| `GET /level/status` | — | Level `path`, `id`, `hasDestination`, `unsavedChanges`; unsaved state is unavailable during play. |
| `POST /level/save` | `{}` or empty | Save the active Level at its current project destination. |
| `POST /level/save-as` | `{"path":"levels/prototype","overwrite":false}` | Save the current scene to that project-relative virtual path and make it active. Omit `.passet`; use lowercase paths. |
| `POST /level/load` | `{"path":"levels/prototype"}`; alternatively `?path=...` | Load an already loaded Level asset, replacing scene entities. Does **not** guard against unsaved changes. |
| `POST /terrain/sculpt` | `?path=` or `?id=` names the terrain; body `{"mode":"raise","x":32,"z":32,"radius":10,"strength":20,"falloff":1,"duration":0.5}` | Apply one brush stroke to a terrain as a single undo step — moving its height field, or painting a layer onto it with `"mode":"paint"`. See [sculpting](#sculpting-a-terrain). |

### Sculpting a terrain

`POST /terrain/sculpt` drives the same brush the editor's terrain tools use, and records the result
as **one undo step** that `/history/undo` reverses exactly. The mode decides which of the terrain's
two fields the stroke writes: four of them move the height field, and `paint` writes layer weights
without moving the ground at all.
The terrain is named by `?path=` or `?id=`, as on `GET /terrain`; everything else is the JSON body.

| Field | Default | Meaning |
| --- | --- | --- |
| `mode` | `raise` | `raise`, `lower`, `smooth` (towards the average of the neighbouring samples), `flatten` (towards one height) or `paint` (towards one layer). |
| `x`, `z` | — | One terrain-local point, for a single dab. Mutually exclusive with `points`. |
| `points` | — | `[{"x":…,"z":…}, …]`, up to 256, for a drag. Every point gets a full `duration`, so a longer stroke moves the ground further — exactly as holding the brush still for more frames would. |
| `radius` | `8` | World units. Samples further out are untouched; the brush is round, not square. |
| `strength` | `8` | Per second. For the height modes that is world units, and it means the same in all four: `smooth` and `flatten` move a sample *towards* their target by at most this much. For `paint` it is the share of the layer handed over, which approaches full coverage rather than reaching it. |
| `falloff` | `0.5` | How much of the radius is soft edge, measured **inwards from the rim**. `0` is a hard-edged stamp with a flat top, `1` a dome peaking under the point. |
| `duration` | `0.1` | Seconds of brush time per point, standing in for the frame time a dragged stroke accumulates. |
| `height` | — | The reference height for `flatten`. Left out, the stroke levels to the ground under its first point. |
| `layer` | `0` | The splat channel `paint` writes into, `0` to `3`. Rejected outside that range whatever the mode is, rather than clamped into it. |

The reply carries the terrain path, the resolved `mode` (plus `layer`, when painting),
`requestedPoints`/`appliedPoints` and the resulting `history` counts. **Fewer applied points than
requested is a normal answer** — it means part of the stroke fell outside the terrain — and zero
means none of it landed, which is how a caller learns its coordinates are not on that terrain. A
stroke that changes nothing records no undo step, so it cannot consume the next undo.

Sculpting requires stopped play mode, for the reason undo does. Read the result back through
`GET /terrain?x=&z=`: the heights it reports are interpolated across the same triangle the mesh is
built from, so they are exact rather than approximate, and `layerWeights` carries what a paint
stroke stored at the nearest sample.

Scene writes, imports, undo/redo, saving and loading require stopped play mode.
Saving does not save other modified assets. Import is not undoable. See
[editing](debug-server-editing.md) and [history/persistence](debug-server-history.md).

### Cameras and rendered feedback

| Method and route | Input | Effect |
| --- | --- | --- |
| `GET /camera` | None | Level editor camera `state`, forward/up vectors, viewport size/activity and mouse capture. |
| `POST /camera` | Partial camera state or `{"position":{"x":10,"y":6,"z":12},"lookAt":{"x":0,"y":2,"z":0}}` | Move/orient the editor view. Optional `fieldOfView`, `nearPlane`, `farPlane`; `rotation` and `lookAt` are mutually exclusive. |
| `POST /camera/frame` | `{"entities":[{"id":"<entity-id>"}],"includeChildren":true,"padding":1.2}` | Fit model bounds or pivots to the Level view; optional look `direction` and `up`. Returns measured `framedBounds`. |
| `GET /level/camera` | None | Selected scene camera entity `target`, `component`, and scene generation. |
| `POST /level/camera` | `{"target":{"id":"<entity-id>"}}` or `{"target":null}` | Select/clear the scene camera used by Game view. Recorded in history and Level persistence. |
| `GET /viewport.png` | Optional `?view=level&width=800`; view can be `game` | Immediate PNG from the active viewport. No ordering guarantee relative to an edit. |
| `POST /observe` | `{}` or `{"after":<observationToken>,"view":"level","width":800,"entities":["<entity-id>"]}` | Capture a subsequent rendered frame with base64 PNG, camera metadata, requested entity state and incremental logs. |

Editor-camera controls and `/observe` require 3D perspective mode. The requested
viewport must be rendered for framing/capture: open its tab in the UI. A scene
Camera does not automatically become the Game camera; explicitly select it.
Editor-camera movement does not change or save a scene Camera. Details:
[editor camera](debug-server-camera.md), [scene camera](debug-server-scene-camera.md),
[observations](debug-server-observation.md).

### Request recovery

| Method and route | Headers | Purpose |
| --- | --- | --- |
| `GET /requests` | None | Current session and request limits. |
| `GET /requests?id=<key>` | `X-Pine-Session` | Query a retained mutation. Terminal results include original HTTP status and body. |
| `POST /requests/cancel?id=<key>` | `X-Pine-Session`; no body or idempotency key | Cancel pending work. Running work cannot be interrupted. |

All scene/camera mutations, imports, saves and undo/redo support the two retry
headers described below. `POST /observe` is a read and rejects retry headers.

## Build a scene efficiently

### 1. Choose a destination without losing the starting scene

Inspect `/level/status` before replacing the scene. For a prototype based on the
current scene, save-as to a new path with `overwrite: false` before adding content.
This preserves the original Level file and includes the current unsaved scene in
the new Level. **Save-as copies the current scene; it does not create an empty one.**
Do not load another Level over unsaved user work without resolving what to preserve.

### 2. Discover a small asset palette

Fetch `/assets?type=Model` once, cache it, then search locally for names such as
`warehouse`, `shelf`, `crate`, `barrel`, `lamp`, and `floor`. Request Material or
Level catalogs separately when needed. Refresh after imports or other asset changes.

```sh
curl --silent --show-error --fail-with-body \
  'http://127.0.0.1:9002/assets?type=Model' -o models.json
```

```python
import json

assets = json.load(open('models.json'))['assets']
terms = ('warehouse', 'shelf', 'crate', 'barrel', 'lamp', 'floor')
for asset in assets:
    if any(term in asset['path'] for term in terms):
        print(asset['path'])
```

The warehouse test had **1,043 loaded models** and many more material entries.
Dumping the full catalog into tool output hid useful matches. Keep full data on
disk; show only candidate paths and selected details. Use returned virtual paths
or IDs exactly, even if filesystem spelling differs. For paths containing spaces
or `&`, let the HTTP client encode the query:

```sh
curl --silent --show-error --fail-with-body --get \
  --data-urlencode 'path=psx mega pack 2/props/wooden_crate_1' \
  http://127.0.0.1:9002/asset -o model.json
```

### 3. Measure before repeating a prop

`/asset` model dumps include `content.Data.Meshes[]` with `BoundingBoxMin`,
`BoundingBoxMax` and material IDs. Geometry buffers are opaque size descriptors,
not vertex arrays. Read candidate assets independently and summarize their bounds:

```python
import json

model = json.load(open('model.json'))
meshes = model['content']['Data']['Meshes']
minimum = {axis: min(mesh['BoundingBoxMin'][axis] for mesh in meshes) for axis in 'xyz'}
maximum = {axis: max(mesh['BoundingBoxMax'][axis] for mesh in meshes) for axis in 'xyz'}
size = {axis: maximum[axis] - minimum[axis] for axis in 'xyz'}
print('minimum:', minimum, 'maximum:', maximum, 'size:', size)
```

For an unrotated, root-level model with positive Y scale, place its base on a floor
at `floorY` using `LocalPosition.y = floorY - minimum.y * scaleY`.
Do not assume the pivot is at the base: the ceiling fixture in the warehouse test
extended **below** its pivot, while the paving module extended upward by 0.4 units.
The built-in cube spans `[-1, 1]` on each axis, so its scale is half its final size.

These asset bounds describe the **stored** model. If `modified` is true, the live
model may differ. `/camera/frame` measures live transformed geometry and returns
`framedBounds`, but also moves the view. Save/restore `/camera.state` when using it
as a measurement tool. There is no standalone live-bounds route yet. For rotated
objects, transform all eight bounding-box corners; scaling the dimensions alone
does not give the world-axis bounds. Respect `MeshIndex` when measuring one mesh.

Place one instance and inspect it before repeating it. Bounds alone cannot reveal
door openings, shelf heights, the visible front face, or a column blocking an aisle.

### 4. Use purposeful batches and retain their IDs

Organize the scene under identity-transform parents such as Structure, Storage,
Yard and Lighting. Create parents before their children. Use one batch per
meaningful undo step: structure, storage layout, contents, lighting adjustment.
The current limit is **128 operations, 256 KiB, 32 JSON nesting levels** per edit.
Read the running schema for exact limits; object pools impose separate capacity limits.

```json
{
  "version": 1,
  "operations": [
    {"op":"entity.create","ref":"storage","name":"Storage"},
    {
      "op":"entity.create","ref":"crate","name":"Dispatch crate 01",
      "parent":{"ref":"storage"},
      "components":[
        {"type":"Transform","properties":{"LocalPosition":{"x":2,"y":0,"z":3}}},
        {"type":"ModelRenderer","properties":{"Model":{"path":"psx mega pack 2/props/wooden_crate_1"}}}
      ]
    }
  ]
}
```

Substitute a loaded model path. Store the response's `refs` and component IDs;
print `completed`, `history`, `refs` and any error, rather than every returned
property. Keep a local map from your layout names to IDs and component types.

| Edit operation | Target / important behavior |
| --- | --- |
| `entity.create` | Optional name, ref, parent and components. Transform exists automatically. |
| `entity.update` | Entity ID; patch `name`, `active`, `static`. |
| `entity.reparent` | Entity ID; parent is existing ID, earlier batch ref, or null. Preserves local transforms. |
| `entity.delete` | Entity ID; deletes its entire supported hierarchy. |
| `entity.duplicate` | Entity ID; copies its supported hierarchy with fresh IDs under the same parent. Use a subsequent batch to rename/move returned copies. |
| `component.update` | **Component ID**; patch its writable properties. |
| `component.add` | **Entity ID**, component type and optional properties. |
| `component.remove` | **Component ID**; required Transform cannot be removed. |

Batch refs only address parents of later creations/reparents within the same
request. They are not general targets for component updates or later requests.
Use returned IDs for those. Read `components[].properties` from `/entity` for
editable state; serialized `data` uses a different representation and can contain
numeric enums. Prefer patches containing only the fields you intend to change.

Supported components are Transform, ModelRenderer, Light, perspective Camera,
primitive Collider and RigidBody. Consult `/edit/schema.components` for all
properties/defaults and [component reference](debug-server-editing.md#property-formats).

**Pine hierarchy semantics matter:** parent position adds to child position,
but parent rotation/scale do not rotate/scale the child's positional offset.
Rotations compose and scales multiply. A scaled group therefore does not behave
like a conventional scene-graph layout transform. Use explicit positions/scales
for repeated modules. Entity active/static flags also do not propagate to children.

### 5. Make mutation retries safe

Before sending a mutation, persist its route, exact body bytes, server session and
a unique `Idempotency-Key`. Reuse those exact values if the reply is lost.
Generate a new key only for a new logical operation, never automatically on retry.

```sh
curl --silent --show-error --fail-with-body \
  -H 'Content-Type: application/json' \
  -H 'X-Pine-Session: <session returned by GET /requests>' \
  -H 'Idempotency-Key: <unique key retained for this operation>' \
  --data-binary @edit.json http://127.0.0.1:9002/edit -o edit-result.json
```

Execute dependent mutations sequentially. The engine processes them on its main
thread; racing edits, camera moves and saves makes ordering harder to reason about.
Batching reduces request overhead and the number of retained identities.

Whole-batch validation happens before mutation, but execution is **not atomic**:
an execution failure can leave completed operations applied and clear history.
Inspect `completed`, partial results, failure flags and logs before continuing.
A five-second timeout can mean a write is still running. Poll
`GET /requests?id=<key>` with the original session header, then inspect its nested
`result.status` and `result.body`. A 200 status lookup does not mean the write succeeded.

Retained outcomes last ten minutes after completion, with at most 256 retained
identities. After expiry, restart or an unknown outcome, reconcile the live scene
before sending another creation. See [request recovery](debug-server-requests.md).

### 6. Inspect the rendered result after each stage

Use **edit → camera/frame → observe**, passing the latest mutation's
`observationToken` as `after`. Preserve the edit token's `logsSince` separately
when you also want logs covering the edit. No client sleep is needed.

For example, after the crate batch above, create a framing request from its returned
parent ID, then send it with a **new** mutation identity:

```python
import json

edit = json.load(open('edit-result.json'))
frame = {'entities': [{'id': edit['refs']['storage']}], 'padding': 1.2}
with open('frame.json', 'w') as output:
    json.dump(frame, output)
```

```sh
curl --silent --show-error --fail-with-body \
  -H 'Content-Type: application/json' \
  -H 'X-Pine-Session: <session returned by GET /requests>' \
  -H 'Idempotency-Key: <new unique key retained for this camera operation>' \
  --data-binary @frame.json http://127.0.0.1:9002/camera/frame -o camera-result.json
```

Build the observation request from that response:

```python
import json

edit = json.load(open('edit-result.json'))
camera = json.load(open('camera-result.json'))
body = {
    'after': camera['observationToken'],
    'logsSince': edit['observationToken']['logsSince'],
    'view': 'level',
    'width': 1000,
    'entities': [edit['refs']['crate']],
}
with open('observe.json', 'w') as output:
    json.dump(body, output)
```

```sh
curl --silent --show-error --fail-with-body -H 'Content-Type: application/json' \
  --data-binary @observe.json http://127.0.0.1:9002/observe -o observation.json
```

```python
import base64
import json
from pathlib import Path

observation = json.load(open('observation.json'))
Path('viewport.png').write_bytes(base64.b64decode(observation['image']['data']))
print(observation['frame'], observation['logs'])
```

Inspect the PNG; do not print its base64 payload. Width only downsizes the native
viewport, preserving aspect ratio. An observation accepts at most 128 entity IDs;
request only the objects needed for the current check. `{}` requests a fresh view,
but its default log cursor starts at acceptance and can miss errors from earlier edits.
Tokens order captures after mutations; they do not freeze out later UI or API changes.

Use an elevated overview to check layout and a walking-height view to check
clearance, shelf contents, wall gaps and lighting. Framing a whole building does
not guarantee a useful interior view. The warehouse's central column required an
offset aisle camera rather than one at the building center.

Tune lights at the actual scene scale. Pine uses windowed inverse-square falloff;
range sets the cutoff, while intensity controls brightness. In the warehouse,
point lights 4–5 units above the floor were dim at intensity 9; warm lights at 65
plus cooler fills at 22 gave a useful interior. These are scene-specific values,
not engine defaults. See [lighting](rendering.md#lighting) for slot limits and
direction conventions. Enlarge the edit-view far plane for larger scenes.

### 7. Save and check the result

1. Save via `/level/save` (or save-as for a new destination). Check `fileWritten`
   and `/level/status.unsavedChanges == false` while stopped.
2. Capture a useful view and record the scene hierarchy before a reload check.
3. If a reload is appropriate, load the saved path. Loading clears history and
   replaces entity/component IDs; reacquire them from `/entities` and `/entity`.
4. Compare hierarchy, names, components, intended properties and asset references;
   do not compare old and new scene IDs. Check the selected Game camera separately.
5. Capture again and inspect logs. Leave a useful editor view for the user.

`/entities.entities` contains **roots**, with full objects recursively nested in
`children`. Its `count` and `/status.entityCount` include temporary editor entities.
Counting only the top-level array can make a large grouped scene appear to have
just two objects. For example:

```python
def scene_entities(roots):
    for entity in roots:
        if entity['temporary']:
            continue
        yield entity
        yield from scene_entities(entity['children'])
```

The first warehouse test saved/reloaded **167 scene entities**: 154 ModelRenderers,
7 Lights, 56 Colliders and 1 scene Camera, plus organizational parents. Hierarchy
names, component membership, flags and camera selection survived; the rendered
interior was inspected after reload and no new errors were logged. This was an
authoring/persistence check, not a gameplay or full collision validation.

## Blender asset sub-agents

When a required prop is absent from the loaded model catalog, an asset sub-agent
can build a GLB with Blender/Python while the level agent continues other work.
Keep the level agent responsible for Editor mutations, imports and final placement;
give the asset agent its own authoring directory and a bounded modeling task.

The brief should specify:

- Purpose, intended placement and nearby reference assets or scene captures.
- Dimensions in Pine units, ground/contact pivot, forward direction and final
  Y-up orientation. Blender normally authors Z-up; verify the exported GLB axes.
- Polygon/detail budget, material style and whether the prop is static.
- Existing texture candidates: previews, accessible source paths, Pine paths/IDs
  when known, and intended atlas regions. Extra textures outside the project can
  also be supplied by local path; inspect them before choosing a mapping.
- Required deliverables: self-contained `.glb`, editable `.blend`, reproducible
  Python script, preview PNGs, and a manifest of bounds, triangle count, materials
  and texture provenance.

Project textures may be embedded in source GLBs under `content/`, rather than
stored as separate PNGs. Extract their image bytes for reuse when needed and
record the source GLB and image name. Do not change the original texture or source
asset. Keep authoring files in a project directory such as
`data/projects/<project>/authoring/<prop>/`; `/assets/import` copies the exported
GLB into `content/`, but does not archive the Blender file or generation script.

After delivery, inspect the preview and export metadata, then call
`POST /assets/import` with the absolute GLB path and a destination such as
`models/generated`. Import with `overwrite: false` initially. Use the returned
Model ID in a ModelRenderer, inspect `/asset` bounds, and capture the prop in Pine
beside nearby assets. A Blender preview alone does not validate Pine materials,
scale or orientation. Request a revision from the asset agent when necessary;
re-import the same destination with `overwrite: true` and a new mutation key to
preserve the model identity. Inspect partial import results before retrying.

For the initial workflow, **duplicate imported materials/textures are accepted**.
Reusing source pixels does not guarantee reuse of the existing Pine asset ID.
Resource matching and reference preservation are recorded as future work in the
[debug-server TODO](debug-server-todo.md#later-driven-by-actual-scene-work).

The first trial produced `models/generated/pallet_jack` in project `gm`: a static
1,152-triangle prop, 0.65 units wide, 1.20 high and 1.585 long. The asset agent
reused rusted steel from an existing shelf GLB and a newly imported rubber PNG.
Pine imported both embedded textures; its model bounds matched the Blender export.
The level agent placed it in `levels/warehouse`, inspected rendered views, then
saved/reloaded the scene with the new prop intact and no import errors. Sources,
previews, texture provenance and verification results are retained under
`data/projects/gm/authoring/pallet-jack/`. The trial validates static visual asset
authoring; it does not add interaction, animation or collision to the prop.

## Limits and troubleshooting

| Symptom / need | Check or next step |
| --- | --- |
| Connection refused | Correct port, Editor process, `PINE_DEBUG_SERVER`, and client network environment. |
| HTTP 400 on edits | Error `path`/operation index, running schema, exact property case, entity versus component ID, vector coordinates, and batch limits. |
| HTTP 409 during a write | Stopped play state; import dialog; destination conflicts; release mouse capture for editor-camera writes. Read the actual error. |
| HTTP 409 on capture | Selected/visible viewport, perspective camera, valid scene token and still-existing entities. Game view also needs a selected scene Camera. |
| HTTP 500 or 504 | Inspect retained result and partial-execution flags before another write. A timeout is not cancellation of running work. |
| Dark/empty model | Check Model/MeshIndex, asset bounds and scale, camera/clipping, material references, active flags, light placement/intensity and rendered logs. |
| Statistics disagree with the picture | Treat counters as diagnostics. The warehouse run reported zero `lightCount` and `vertexCount` despite visible lit geometry; this observation was not diagnosed or fixed. |
| Physics appears unchanged while stopped | Actors/shapes are created on Play. Box Collider Size is half-extents, scaled with the entity; Position is an unscaled, unrotated world-axis offset. See [physics authoring](debug-server-physics.md). |
| Need ambient/fog/skybox writes, Blueprint spawning, play/pause/stop, material authoring, picking or placement helpers | These are not currently exposed. Use the UI where available; track API work in [the TODO](debug-server-todo.md). |

The API also does not yet expose 2D authoring, viewport-tab switching, general UI
automation or arbitrary component writes. Reading serialized state does not imply
that the same state can be written through `/edit`.

## Maintaining this guide

Route registrations live in
[`Endpoints.cpp`](../Editor/src/DebugServer/Endpoints/Endpoints.cpp); request-control
routes are registered in [`DebugServer.cpp`](../Editor/src/DebugServer/DebugServer.cpp).
When adding a route, update the table here and its linked protocol documentation.
Keep writable property details in adapters and `/edit/schema`, with examples in
the feature docs. The [original debug-server plan](plans/debug-server.md) records
design history; this guide describes the operating workflow.

# Spatial measurements

`POST /spatial/query` measures scene entities without moving a camera or changing
scene state. It works without an active viewport, including in stopped edit mode.
Use it before spacing, resizing or placing props from an authoring script.

## Request

```json
{
  "entities": [{"id": "<entity-id>"}, {"id": "<another-entity-id>"}],
  "includeChildren": true
}
```

- `entities`: 1–128 existing scene entity references, using the same `{ "id": ... }`
  form as camera framing. Batch refs and temporary editor entities (including their
  descendants) are rejected. Results follow request order; duplicates are allowed.
- `includeChildren`: optional boolean, default `true`. Each result measures the
  entity and its non-temporary descendants. Set `false` to measure only the entity.
- Maximum body size: 16 KiB; maximum JSON nesting: eight levels. Unknown fields,
  query parameters, malformed JSON and invalid references return HTTP 400 with
  `error` and `path`. A bad reference rejects the whole request.
- This POST is a **read**. It does not add history, dirty the level, return an
  observation token, or accept mutation retry headers.

## Response

```json
{
  "sceneGeneration": 3,
  "includeChildren": true,
  "entities": [
    {
      "id": "<entity-id>",
      "bounds": {
        "min": {"x": -1, "y": 0, "z": -1},
        "max": {"x": 1, "y": 4, "z": 1},
        "center": {"x": 0, "y": 2, "z": 0},
        "dimensions": {"x": 2, "y": 4, "z": 2}
      },
      "localTransform": {
        "position": {"x": 0, "y": 2, "z": 0},
        "rotation": {"x": 0, "y": 0, "z": 0, "w": 1},
        "scale": {"x": 1, "y": 2, "z": 1}
      },
      "worldTransform": {
        "position": {"x": 0, "y": 2, "z": 0},
        "rotation": {"x": 0, "y": 0, "z": 0, "w": 1},
        "scale": {"x": 1, "y": 2, "z": 1}
      },
      "forward": {"x": 0, "y": 0, "z": -1},
      "right": {"x": 1, "y": 0, "z": 0},
      "up": {"x": 0, "y": 1, "z": 0}
    }
  ],
  "combinedBounds": {
    "min": {"x": -1, "y": 0, "z": -1},
    "max": {"x": 1, "y": 4, "z": 1},
    "center": {"x": 0, "y": 2, "z": 0},
    "dimensions": {"x": 2, "y": 4, "z": 2}
  }
}
```

The example response represents one measured cube. `combinedBounds` is the union
of all non-null result bounds; overlapping selections do not enlarge that union.
`bounds` is `null` when an entity (and its included descendants) has no supported
geometry. If all results lack geometry, `combinedBounds` is also `null`. A light,
empty group or unassigned model therefore does not pretend to have a measured size.
A model flattened by zero scale still has non-null bounds with a zero dimension.

All bounds are **world-axis-aligned bounding boxes** in engine world units.
Dimensions are `max - min`, and center is `(min + max) / 2`. These are enclosing
boxes, not exact surface intersections, oriented dimensions, or usable interior
space: a rotated prop can have a wider world box, and a doorway's box includes its
opening. Bounds center can differ from the entity pivot.

Transforms belong to each requested entity, even when bounds include children.
Rotations are `{x,y,z,w}` quaternions; scale is dimensionless. `forward`, `right`
and `up` are world-space rotation axes (local −Z, +X, +Y), without scale or its
mirroring. Pine adds parent and child positions, composes rotations and multiplies
scales; parent rotation/scale do not transform the child's positional offset.

## Geometry coverage and freshness

- **ModelRenderer:** the live model's local bounding box, or the selected mesh's
  box when `MeshIndex != -1`. Transform all eight corners using current world
  position, rotation and scale. Negative and zero scales are supported. This
  shares the model calculation with `/camera/frame`; it does not read stale
  renderer bounds or cached transform matrices. It does not measure individual
  triangles or shader deformation.
- **TerrainRenderer:** the union of live terrain chunk bounds, translated by the
  entity's current world position. Height edits update these bounds immediately.
  As in the terrain renderer, entity rotation and scale do not affect terrain
  geometry. Bounds cover the height field, excluding crack-hiding skirts.
- Model and terrain components are measured regardless of active/static flags,
  component activation or camera visibility. Temporary descendant subtrees are
  excluded. Colliders, sprites, particles and other geometry are not measured.
- Non-finite spatial data or an invalid selected mesh index returns HTTP 400
  rather than a misleading measurement.

All results are sampled in one main-thread handler execution. `sceneGeneration`
identifies the current scene lifetime using the same value as history and
observations. It is not a frame ID or an edit revision. During play, this is the
state when the request executes; it does not wait for physics or a capture. Later
edits can invalidate a measurement, so query again after changing the geometry
or transforms on which placement depends.

Unlike this query, `/camera/frame` still measures models only, substitutes pivots
for selections without model geometry, and moves the editor view.

## Python example

```python
import json
import urllib.request

payload = {
    'entities': [{'id': entity_id} for entity_id in prop_ids],
    'includeChildren': True,
}
request = urllib.request.Request(
    'http://127.0.0.1:9002/spatial/query',
    data=json.dumps(payload).encode(),
    headers={'Content-Type': 'application/json'},
)
with urllib.request.urlopen(request) as response:
    measurements = json.load(response)

for entity in measurements['entities']:
    bounds = entity['bounds']
    if bounds is not None:
        print(entity['id'], bounds['dimensions'], 'base Y:', bounds['min']['y'])
```

For a root-level prop resting on a horizontal floor, the required vertical
translation is `floor_y - bounds['min']['y']`. Add that delta to its current
position through `/edit`. This measures the enclosing box; uneven-ground contact
can be inspected with the raycast below. Automatic placement remains future work.

## Raycasts and bounds overlaps

`POST /spatial/raycast` and `POST /spatial/overlap` inspect the current scene in
**stopped edit mode**, without a camera, capture, visible viewport or Collider.
Playing and paused play mode return HTTP 409. Both operations are reads: they
leave selection, scene, camera, history and unsaved state unchanged, and reject
mutation retry headers. They sample one main-thread execution and return
`sceneGeneration`, not a rendered frame or an edit revision. Query again after
changing the geometry or transforms used for placement.

Both accept these optional fields:

- `includeInactive`: boolean, default `false`. Include inactive entities and
  components when true. Otherwise follow Pine's `IsWorldEnabled` semantics:
  the entity and component themselves must be active; a parent's active flag
  does not implicitly disable descendants. Static flags do not affect queries.
- `exclude`: array of at most 128 existing scene references, each `{"id":"..."}`.
  Excludes each entity **and its descendants**, useful for probing beneath a prop
  without hitting the prop itself. Missing, stale, temporary and batch references
  reject the entire query. Temporary editor subtrees are always excluded.

Vectors use strict `{x,y,z}` objects. Coordinates, directions and distances must
be finite numbers with absolute value at most `1e12`. Coordinates and distances
use Pine world units. Both routes reject query parameters, unknown fields,
malformed JSON, bodies over 16 KiB and nesting beyond eight levels with HTTP 400
and `error`/`path`. Invalid eligible scene geometry also returns HTTP 400 instead
of silently ignoring it. No result is retained for later queries.

### Cast a ray

```json
{
  "origin": {"x": 10, "y": 20, "z": 5},
  "direction": {"x": 0, "y": -1, "z": 0},
  "maxDistance": 50,
  "exclude": [{"id": "<prop-id>"}]
}
```

`origin`, `direction` and `maxDistance` are required. The nonzero direction is
normalized by the server; distance is independent of its supplied magnitude.
`maxDistance` must be positive. The closest surface in the inclusive interval
`[0, maxDistance]` is returned; a miss succeeds with `hit: null`.

```json
{
  "sceneGeneration": 3,
  "geometry": "model-surfaces-and-terrain",
  "hit": {
    "entity": "<entity-id>",
    "component": "<component-id>",
    "asset": "<model-or-terrain-asset-id>",
    "geometry": "model-surface",
    "meshIndex": 0,
    "distance": 18,
    "position": {"x": 10, "y": 2, "z": 5},
    "normal": {"x": 0, "y": 1, "z": 0}
  }
}
```

The example illustrates the response shape. `meshIndex` is the zero-based model
mesh index; terrain hits use `geometry: "terrain-surface"` and `meshIndex: null`.
Positions and unit normals are in world space. Normals describe the geometric
face, flipped to oppose the ray, rather than interpolated shading or normal maps.
Faces can be hit from either side, including from inside a closed mesh. Degenerate
triangles and rays parallel to a face have no point intersection. Coincident hits
keep the first encountered entity/component/mesh; do not rely on a persistent tie
order across scene replacements.

Geometry coverage:

- **ModelRenderer:** current uploaded triangle positions and indices, including
  procedural meshes and `UpdateVertices` changes. Respects `MeshIndex`. Uses fresh
  world position/rotation/scale, including parent transforms and negative/zero
  scales, without consulting cached renderer matrices or bounds. No frustum or
  back-face culling is applied.
- **TerrainRenderer:** the live finest-resolution height field, using the same
  triangle split as terrain authoring. Sculpting is visible immediately, even
  before chunk meshes rebuild. Applies entity world translation only, matching
  terrain rendering. Excludes LOD simplification and crack-hiding skirts.
- These are solid geometric surfaces. Material alpha, transparency, shader
  deformation, sprites, particles, colliders and other geometry are excluded,
  including from occlusion. A miss does not prove that a rendered pixel is empty.

Model geometry is synchronously read from the GPU once per distinct mesh per
request. There is no cross-request cache. Work is limited to **64 MiB** of model
position/index data and **2,097,152 model triangle tests**, counting every eligible
instance. An exceeded limit or unavailable mesh readback returns HTTP 409 without
a partial hit. Exclude unrelated hierarchies to reduce work. This first version
scans eligible model triangles; it does not have a spatial acceleration index.
Terrain uses its existing grid traversal and float precision; model intersection
uses doubles over float vertex/transform data. Contact coordinates are approximate,
especially far from the origin, so allow suitable placement clearance.

### Find overlapping bounds

```json
{
  "bounds": {
    "min": {"x": 8, "y": 0, "z": 3},
    "max": {"x": 12, "y": 4, "z": 7}
  },
  "limit": 128
}
```

`bounds` is required, with `min <= max` on every axis. Touching faces, edges and
points count as overlaps; a zero-size query box is allowed. `limit` is an optional
integer from 1–128, default 128.

```json
{
  "sceneGeneration": 3,
  "geometry": "world-bounds",
  "entities": [
    {
      "id": "<entity-id>",
      "bounds": {
        "min": {"x": 9, "y": 0, "z": 4},
        "max": {"x": 11, "y": 2, "z": 6}
      }
    }
  ],
  "total": 1,
  "truncated": false
}
```

Each entity is tested once against the union of its eligible model and terrain
component bounds, **without aggregating children**. Children are queried as
separate entities. Empty groups and entities without supported geometry have no
bound and cannot match. Model and terrain bounds follow the measurement rules
above, with the additional activation filter. Results follow current scene-list
order. `total` counts all matches; `truncated` indicates that `limit` omitted some.
There is no pagination in this operation; narrow the box or use exclusions when
you need all matches.

An overlap is **only an enclosing-box match**, not a triangle intersection,
collision, or proof of free space. A doorway's bounds include its opening, and
two separated meshes on one entity can enclose empty space between them. Use a
raycast for a point on an actual surface. Exact volume/mesh overlap is not offered.

## Verification

After configuring and building Editor with Ninja:

```sh
python3 Editor/src/DebugServer/Verification/verify-spatial.py --build build
```

The recipe uses a disposable project and Xvfb. Native checks cover a fresh terrain
height edit, terrain transform semantics, measurement without a viewport,
negative/zero scale, selected mesh bounds and invalid spatial state. HTTP checks
cover batching, hierarchy transforms, dimensions and axes, null geometry, limits,
validation, temporary/stale references, unchanged camera/history/level state,
immediate reads after edits, and the existing reparent/framing verification recipe.
The same recipe checks stopped-mode raycasts and bounds overlaps, doorway holes,
terrain face normals and fresh sculpting, rotated/mirrored parent transforms,
indexed/non-indexed and selected meshes, vertex readback updates and graphics
binding preservation, exclusions and activation, distance limits, nearest hits,
touching boxes, explicit truncation, geometry work/memory limits, invalid geometry
and play/pause rejection.

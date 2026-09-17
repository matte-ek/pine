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
and surface alignment still need the planned raycast and placement operations.

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

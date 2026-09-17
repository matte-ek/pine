# Pick model surfaces from captures

Request `POST /observe` with `"picking": true` while the editor is **stopped**.
The observation returns its usual PNG and frame/camera metadata, plus a `picking`
object containing a retained `capture` reference. Send that reference and a pixel
to `POST /pick` to read the nearest model mesh surface at that pixel.

```json
{"view":"level","width":800,"picking":true}
```

The existing `after` token can be included to wait for an edit or camera change.
Picking is opt-in: ordinary observations return `picking: null` and do not render
or retain picking buffers. Immediate `/viewport.png` images cannot be queried.
Level and Game observations support picking; the viewport must be active and have
a perspective camera when captured. Later picks do not require a visible viewport.

## Query a pixel

Copy `observation.picking.capture` into this request:

```json
{
  "capture": "<capture-id>",
  "pixel": {"x": 400, "y": 225}
}
```

`POST /pick` returns:

```json
{
  "capture": "<capture-id>",
  "frame": {"session":"<session>","id":123,"sceneGeneration":1,"revision":4},
  "pixel": {"x":400,"y":225},
  "geometry": "model-surfaces",
  "hit": {
    "entity": "<entity-id>",
    "component": "<model-renderer-id>",
    "model": "<model-asset-id>",
    "meshIndex": 0,
    "position": {"x":0.1,"y":1.2,"z":3.4},
    "normal": {"x":0,"y":0,"z":1}
  }
}
```

The numbers above illustrate the response shape. `hit: null` is a successful miss.
`meshIndex` is zero-based within the captured Model asset. Positions are in Pine
world units. Normals are normalized **geometric face normals facing the captured
view**, not interpolated vertex normals or normal-map values.

Pixels are integer indices in the **returned PNG**, with `(0, 0)` at the top left,
X increasing right and Y increasing down. The query samples the pixel centre:
`(x + 0.5, y + 0.5)`. Coordinates must be inside `image.width`/`image.height`.
When an observation is downsized, picking uses a sample grid of that output size
and the original capture's projection. It does not average IDs, depth or normals.
The PNG uses filtered resizing, so an edge pixel can contain several surfaces'
colors even though its centre picks just one surface. Prefer interior pixels.

Both endpoints are reads: they leave selection, camera, scene, history and unsaved
state unchanged. They do not accept mutation retry headers. `/pick` accepts no
query parameters, at most 4 KiB of JSON, and four nesting levels. Malformed IDs,
unknown fields and invalid pixels return HTTP 400 with `error` and `path`.

## Capture lifetime and historical state

The capture retains pixel depth, face normals and mesh identities, using the
observation's saved camera matrices. Picking does not rerender the current scene.
Camera navigation, resizing, moving or deleting an object, history changes and
asset re-imports therefore cannot change a retained answer. Returned identities
and geometry describe the **captured frame**; they may no longer exist or match
the current scene. Acquire a new observation before placing against changed geometry.

Retention is bounded by all of the following:

- 120 seconds from capture creation, without refresh on a pick.
- Four captures, oldest first when another is created.
- 64 MiB of retained pixel and mesh-identity payload, evicting oldest captures
  earlier if necessary. Temporary render/readback allocations are additional.
- 2,097,152 pixels per capture, at most 4096 per dimension, and 16,384 visible
  model meshes. A larger request returns HTTP 409; reduce observation `width`.

The `picking` object advertises `capture`, `geometry`, output `width`/`height`,
`retentionSeconds`, `maximumCaptures` and `maximumRetainedBytes`. A reference is
valid only in its server process and scene generation. Level replacement and
Stop's scene restoration invalidate it, even when reloading the same level.
Unavailable, expired or evicted references return HTTP 409 and require a new
observation. Shutdown releases the retained data and shader resources.

## Geometry coverage

This first implementation picks **ModelRenderer triangles** using live GPU mesh
buffers. It needs no Collider and works in stopped edit mode. It respects active
entities/components, the capture's model visibility, `MeshIndex`, actual world
transformation matrices and back-face culling. Temporary editor entities and their
descendants are excluded. Indexed and non-indexed meshes are supported.

The `model-surfaces` contract treats model meshes as solid geometric surfaces.
It does not reproduce material alpha cutouts, transparency, custom shader vertex
deformation, stencil effects, normal maps, wireframe display or post-processing.
Terrain, sprites, particles, editor overlays and other non-model geometry are
excluded, including from occlusion: a model behind one of them can be returned.
A miss means no supported model surface, not necessarily an empty image pixel.
Use this operation for solid model walls and props; broader visibility matching
and terrain picking are follow-up work.

Capture runs an isolated geometry pass at the observation's post-render boundary,
before UI and queued writes, with the camera matrices saved for that viewport.
Runtime captures are rejected, including paused play mode. The normal is stored
with half-float octahedral encoding and position reconstructed from raster depth,
so both have finite precision; use an appropriate placement clearance.

## Verification

Configure/build Editor, then run the existing-style native and HTTP recipe:

```sh
cmake -S . -B build
cmake --build build --target Editor -j4
python3 Editor/src/DebugServer/Verification/verify-picking.py --build build
```

The recipe compiles an Editor probe using the build's flags, runs under Xvfb in a
disposable project and shuts it down afterwards. It checks triangle holes rather
than bounding boxes, indexed/non-indexed meshes, selected meshes, component/entity
activation, large mesh IDs, memory/count eviction, invalid inputs and cameras,
rotated and parented geometry, normals on both sides, top-left pixel coordinates,
resizing, distinct Level/Game cameras, nearest-surface occlusion, unchanged historical hits after movement and
deletion, scene replacement and play-mode rejection. With film grain disabled in
the fixture, it compares PNGs to ensure picking does not alter subsequent rendering.
It also runs the existing observation recipe. Captures and hit metadata go to
`/tmp/pine-picking-results` by default.

Add `--check-expiry` to also wait for the advertised 120-second lifetime and verify
that a pick halfway through does not refresh it.

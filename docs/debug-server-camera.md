# Editor-camera controls

The debug server provides `GET /camera`, `POST /camera`, and `POST /camera/frame`
for the **Level viewport's editor camera**. These endpoints use the existing
`PINE_DEBUG_SERVER` localhost server and main-thread queue. They do not modify a
scene Camera component or select the game's active camera.

Only 3D perspective mode is supported. All three endpoints return HTTP 409 if the
Level viewport is in 2D/orthographic mode; select 3D in the viewport first. Camera
controls are available during play mode as well as while stopped. Writes return
409 while mouse navigation is captured, so release the right mouse button first.

## Read and restore

```sh
curl -sS http://127.0.0.1:9002/camera
```

The response has this shape:

```json
{
  "state": {
    "position": { "x": 0, "y": 0, "z": 0 },
    "rotation": { "x": 0, "y": 0, "z": 0, "w": 1 },
    "fieldOfView": 70,
    "nearPlane": 0.01,
    "farPlane": 150
  },
  "forward": { "x": 0, "y": 0, "z": -1 },
  "up": { "x": 0, "y": 1, "z": 0 },
  "viewport": { "width": 800, "height": 450, "active": true },
  "mouseCaptured": false
}
```

Save the **`state` object** and send it as the body of `POST /camera` to restore
that view. The enclosing response also contains observation-only fields and is
not itself a write request. Any subset of the state fields can be posted; omitted
fields retain their current values.

Coordinates use Pine world units, +Y up and local -Z forward. Rotations use
`{x,y,z,w}` quaternions and are normalized before application. Vectors require all
three coordinates. Field of view is vertical, in degrees, between 1 and 175.
Clipping planes must be finite, near must be at least 0.0001, and far must exceed
near in float32. Inputs that produce non-finite camera matrices, or positions so
large that float32 loses the requested viewing direction, are rejected.

## Look at a point

```json
{
  "position": { "x": 5, "y": 3, "z": 8 },
  "lookAt": { "x": 0, "y": 1, "z": 0 }
}
```

Send this to `POST /camera`. `lookAt` is a world-space target; it can be used alone
to orient the camera from its current position, or alongside other state fields.
It cannot be combined with `rotation`.

An optional `up: {"x":0,"y":1,"z":0}` controls roll. It requires `lookAt` and must
be nonzero and nonparallel to the viewing direction. Without `up`, world +Y is
used, with -Z as the fallback for top/bottom views. A target equal to the resulting
camera position is rejected.

Applying a view clears the fly camera's residual movement and synchronizes its
mouse-navigation angles, including roll. Returning to mouse navigation therefore
starts from the applied orientation.

## Frame entities

Send this to `POST /camera/frame`:

```json
{
  "entities": [ { "id": "<entity-uid>" } ],
  "includeChildren": true,
  "padding": 1.2,
  "direction": { "x": -1, "y": -0.5, "z": -1 }
}
```

- `entities` contains 1–128 existing scene entity IDs from `/entities` or `/edit`.
  Temporary editor entities are excluded.
- `includeChildren` defaults to true. Framing an empty parent then frames the
  models beneath it. Set false to consider only the listed entities themselves.
- `padding` defaults to 1.2 and accepts 1–10. It multiplies projected horizontal
  and vertical extents, leaving a margin around the selection.
- Omit `direction` to keep the current rotation. When supplied, it is the direction
  the camera **looks**, not its offset from the selection. It is normalized.
- Optional `up` requires `direction` and follows the same rules as look-at.
  For a top view, use `direction: {"x":0,"y":-1,"z":0}`.

Framing moves the camera to center the combined world-space bounding box and fit
its eight corners within both horizontal and vertical field of view. The Level
viewport must be active and have a nonzero size; otherwise the endpoint returns
409 because it cannot determine the framing aspect ratio. Existing clipping
planes are preserved. If the selection cannot fit, increase `farPlane` with
`POST /camera` and try again.

Bounds come from ModelRenderer assets, respect MeshIndex, and are transformed using
Pine's current world position/rotation/scale accessors. They are computed on demand,
so framing works immediately after an edit without waiting for cached render bounds.
ModelRenderers are included regardless of activation/visibility. If a selected
entity and its considered descendants have no supported model geometry, its own
pivot is used. Terrain, physics shapes and 2D geometry are not measured by this API.

The response contains the same camera description as `/camera`, plus
`framedBounds: {"min":{"x":0,"y":0,"z":0},"max":{"x":1,"y":1,"z":1}}` describing
the bounds actually used. A pivot-only selection is viewed from one unit away when
the clipping range allows it.

## Errors and capture timing

Invalid writes return HTTP 400 with `error` and a slash-separated `path`. All
validation and framing calculations finish before any camera state changes.
Requests are limited to 16 KiB and eight levels of JSON nesting. Unknown fields
are rejected.

Responses confirm camera state, not completion of a render. Successful writes also
return an `observationToken`; pass it as `after` to `POST /observe` to capture a
subsequent rendered view with matching camera metadata, without client sleeps.
See [rendered observations](debug-server-observation.md).

Writes support opt-in retry identities and retained results. Replaying a camera
request returns the original result without moving the current camera again. See
[request retries and completion](debug-server-requests.md).

## Implementation

Protocol handling and framing live in `Editor/src/DebugServer/Camera/`. The only
integration outside the debug server is `Editor::LevelEntity::SetView()` in
`Other/EditorEntity/`: it owns the synchronization with the existing fly-camera
controller. The engine and scene-writing adapters are unchanged.

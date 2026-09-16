# Scene cameras through the debug server

The `Camera` component adapter configures a scene camera through `POST /edit`.
The separate `/level/camera` operation chooses which camera renders the Game
viewport. These operations require stopped play mode and do not save implicitly.
The editor's Level viewport camera remains controlled through [`/camera`](debug-server-camera.md).

## Camera properties

`/edit/schema.components.Camera` advertises creation defaults, properties and
constraints. Camera supports creation, addition, update, removal, duplication,
readback, and undo/redo through the existing [editing contract](debug-server-editing.md).

| Property | Default | Accepted values |
|---|---|---|
| `Type` | `Perspective` | Only the string `Perspective` |
| `FieldOfView` | 70 | Vertical field of view, 1–179 degrees |
| `NearPlane` | 0.01 | Positive world distance, less than `FarPlane` |
| `FarPlane` | 150 | World distance greater than `NearPlane` |

Numbers must be finite and representable as float32. Clipping constraints are
checked after conversion to float32, including underflow and planes that round
to the same value. Parameters must produce finite projection and inverse projection matrices.
Related properties are validated together after merging each
patch with the preceding state. Omitted creation properties use engine defaults;
omitted update properties retain their values. The viewport supplies aspect ratio
for newly created cameras; aspect overrides, clear color and orthographic properties
are not exposed. Camera position and orientation use its entity's Transform.

Existing orthographic cameras can be inspected, but their properties fail this
perspective adapter's validation. They cannot be edited, removed, duplicated or
deleted through it, because history could not restore them through the same adapter.
As with other components, readback reports live values without claiming they are
valid editable values.

```json
{
  "version": 1,
  "operations": [{
    "op": "entity.create",
    "name": "Game camera",
    "components": [
      {"type": "Transform", "properties": {"LocalPosition": {"x": 0, "y": 0, "z": 6}}},
      {"type": "Camera", "properties": {"FieldOfView": 55, "NearPlane": 0.1, "FarPlane": 200}}
    ]
  }]
}
```

Creating, adding or duplicating a Camera does not select it. A duplicate gets new
entity/component IDs and retains the source's writable perspective properties.
Duplication and history also preserve existing clear color, aspect override, and
dormant orthographic size through public accessors; these are not new writable
properties. Level persistence still follows Camera's serializer: clear color and
aspect override are runtime settings and are not stored in the Level file.

## Select the game camera

| Route | Body | Result |
|---|---|---|
| `GET /level/camera` | — | Selected entity `target`, Camera `component`, `sceneGeneration` |
| `POST /level/camera` | `{"target":{"id":"<entity-id>"}}` | Select that entity's first Camera |
| `POST /level/camera` | `{"target":null}` | Clear the selection |

Both identity fields in the response are `{"id":"<persistent-id>"}`, or null
when no game camera is selected. The target is an existing scene **entity ID**,
not a component ID or a batch ref. Selecting the first Camera matches the editor's
Level Properties panel and Level serialization. Selection requires valid perspective
properties. Missing entities, temporary hierarchies, entities without a Camera,
unknown fields, and malformed references return 400 without changing selection or
history. Bodies are limited to 4 KiB and eight JSON nesting levels. Play mode returns
409. Reads remain available during play. Selection does not change active flags.

`/edit/schema.levelCamera` describes the routes and request envelope. The POST is
registered as a mutation and uses the standard [retry identity](debug-server-requests.md)
and [observation token](debug-server-observation.md) contracts. Each successful
selection or clear is one undo step, including selecting the current camera again.
History-recording failure returns 500 with `history: "cleared"` and
`stateMayHaveChanged: true`.

Removing the selected Camera or deleting its hierarchy clears rendering references.
Undo recreates the original persistent IDs and restores the selection; redo clears
it again. Selection undo/redo checks that intervening untracked UI changes have not
replaced the expected camera. Unrelated edit history does not rewrite selection.

## Observe and persist

Open the **Game** viewport tab, then send the selection or edit response's token:

```json
{"view":"game", "after":{"session":"...", "sceneGeneration":1, "revision":2, "frame":100, "logsSince":42}, "width":640}
```

Use the actual `observationToken` object returned by the server for `after`.
`POST /observe` captures the selected camera's rendered image and matching camera
ID, field of view, clipping planes and matrices. A hidden viewport or cleared
camera returns 409. Selection is permitted while the viewport is hidden; capture
requires it to be visible. No client sleep is needed between selection and capture.

Explicit [save/save-as](debug-server-history.md) persists the selected camera and
its properties. Newly captured Levels mark `CameraUsesSerializedOrder: true` and store `Camera`
as a one-based index in serialized root/descendant order (zero means no camera),
so reparenting and editor-only entities do not shift the selection. Older assets
without that flag retain the legacy index interpretation until captured again. Existing incorrect legacy indices
cannot reconstruct the original intent automatically. Reload clears the previous
rendering reference even when the new level has no camera.

## Verification

Build the Editor and use the [disposable-project launch recipe](debug-server-observation.md#verification)
with port 19033 and the **Game** tab selected, then run:

```sh
python3 Editor/src/DebugServer/Verification/verify-scene-camera.py \
  --url http://127.0.0.1:19033 --output /tmp/pine-scene-camera-results
```

The recipe verifies discovery, defaults, creation/addition, invalid values,
composed patches, selection/readback, retry identities, history, duplication,
selected-camera removal/deletion, rendered projection and clipping changes,
unsaved-state detection, and save/reload of a parented camera whose live scene
order differs from serialized order. It saves Game captures for visual inspection.
Restart the same disposable project and repeat with `--reload-only` and the same
output directory to verify both selected and cleared camera files from disk.

Verified locally: Editor, GameHost and EngineCli builds; the complete Camera HTTP
recipe including restart; the existing history, schema, component, duplication,
readback and observation recipes; and visual inspection of zoom, clipping and
matching saved/restarted captures. A temporary native fixture also verified
orthographic/protected-entity rejection, active flags, untracked selection
conflicts, unrelated undo, extra Camera settings during duplication/restoration,
multiple rendering-reference cleanup, multiple temporary entities, play/pause
rejection, play-stop selection restoration, and legacy Level loading/reserialization.
The fixture's deliberate history conflict correctly cleared history and logged an
error. Temporary Editors and virtual displays were stopped after verification.

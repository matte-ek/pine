# Building a scene through the debug server

How to drive a running Editor to assemble, inspect and save a 3D scene. Every route,
field and limit mentioned here is specified in the
[HTTP reference](debug-server.md); this page is about the order to do things in and
the mistakes that cost time.

The shape of the loop is always the same:

> **measure → edit → frame → observe → look at the PNG**

Never assume an edit did what you meant. Capture it and look.

## Orient yourself first

Reuse a running Editor when there is one. Otherwise launch it yourself — this repo
explicitly expects agents to run the Editor (see
[actions to leave to the user](../agent-guidelines/leave-to-the-user.md)) — with
`PINE_DEBUG_SERVER` set, from `data/`, passing the bare project name.

These reads establish everything you need, and are independent of each other:

| Route | What it tells you |
| --- | --- |
| `/status` | Project and active Level. **`playState` must be `stopped`** before any write. |
| `/level/status` | Current save destination and unsaved changes, before you load or save anything. |
| `/entities` | The existing hierarchy, including temporary editor entities. |
| `/camera` | The current view. Keep its `state` object if you should restore it. |
| `/edit/schema` | What this build actually supports: operations, properties, defaults, limits. |
| `/requests` | Server session and the retry/deadline limits. |
| `/logs?limit=1` | A starting `nextCursor` for incremental logs. |

Save full responses to files and print summaries. Refresh the schema and session
after an Editor restart, and every scene ID after a Level replacement.

## 1. Choose a destination without destroying the starting scene

Check `/level/status` before replacing anything. To prototype from the current scene,
`/level/save-as` to a new path with `overwrite: false` **before** adding content: that
preserves the original Level file and carries the current unsaved scene into the new
one. Save-as copies the current scene; it does not create an empty one.

Do not `/level/load` over unsaved user work without deciding what to preserve —
loading does not guard against it.

## 2. Build a small asset palette

Fetch `/assets?type=Model` once, cache it, and search locally. A real project has
thousands of entries, and dumping the catalog into tool output buries the matches.

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

Use the returned virtual paths or IDs exactly, even where the filesystem spelling
differs. Let your HTTP client encode paths containing spaces or `&`:

```sh
curl --silent --show-error --fail-with-body --get \
  --data-urlencode 'path=psx mega pack 2/props/wooden_crate_1' \
  http://127.0.0.1:9002/asset -o model.json
```

Refresh the catalog after imports.

**Names and bounds will not tell you what an asset looks like.** There are no
thumbnails, and a catalog of a thousand models tends to have cryptic paths, so
shortlisting by name gets you candidates and nothing more. When the choice matters —
which wall, which roof, which of six lamp variants — build a throwaway **palette
scene**: create the candidates in a row with known spacing, frame them, capture once,
look, then delete the batch. One extra edit and one capture settles a decision that
guessing from names will get wrong.

## 3. Measure before you repeat a prop

`GET /asset` gives the **stored** model's mesh bounds:

```python
import json

model = json.load(open('model.json'))
meshes = model['content']['Data']['Meshes']
minimum = {axis: min(mesh['BoundingBoxMin'][axis] for mesh in meshes) for axis in 'xyz'}
maximum = {axis: max(mesh['BoundingBoxMax'][axis] for mesh in meshes) for axis in 'xyz'}
print('size:', {axis: maximum[axis] - minimum[axis] for axis in 'xyz'})
```

**Do not assume the pivot is at the base.** A ceiling fixture can extend below its
pivot and a paving module above it. For an unrotated root-level model with positive Y
scale, its base sits on `floorY` at `LocalPosition.y = floorY - minimum.y * scaleY`.
The built-in cube spans `[-1, 1]` per axis, so its scale is half its final size.

For anything already placed, use `POST /spatial/query` instead: it measures live world
bounds, dimensions and transforms for up to 128 entities, accounts for rotation, scale,
parent transforms and `MeshIndex`, covers terrain, and needs no viewport.

```python
payload = {'entities': [{'id': i} for i in prop_ids], 'includeChildren': True}
for entity in post_json('/spatial/query', payload)['entities']:
    if entity['bounds'] is not None:
        print(entity['id'], entity['bounds']['dimensions'],
              'base Y:', entity['bounds']['min']['y'])
```

**Place one instance and look at it before repeating it.** Bounds cannot tell you
where a door opening is, how high a shelf sits, which face is the front, or that a
column blocks the aisle.

**Do not scale an asset on one axis to make it fit.** UVs scale with the geometry, so
stretching a wall along X smears its texture — obvious on anything tiling like brick,
planks or panelling. Nothing will stop you:
the API accepts any `LocalScale`, including negative and zero. And you cannot repair
it through this API: `ModelRenderer` exposes only `Model`, `OverrideMaterial` and
`MeshIndex`, and material authoring is not exposed at all. Materials do carry a single
`TextureScale` factor, but it is uniform, it lives on the shared Material asset rather
than the renderer, and it is reachable only through the editor's asset properties panel
— so it cannot rescue one stretched entity. Repeat the module along the axis instead,
pick a variant with the proportion you need, or import one. Uniform scale is fine; it
keeps the asset's proportions.

Watch for this coming from a **parent**: scales multiply down the hierarchy, so one
non-uniform group scale quietly stretches everything beneath it. Keep layout parents
at identity scale.

## 4. Edit in purposeful batches

Organize the scene under identity-transform parents — Structure, Storage, Yard,
Lighting — and create parents before their children. Use one batch per meaningful undo
step, not one per entity: a batch is one undo step, and batching cuts both request
overhead and the number of retained request identities.

```json
{
  "version": 1,
  "operations": [
    {"op": "entity.create", "ref": "storage", "name": "Storage"},
    {
      "op": "entity.create", "ref": "crate", "name": "Dispatch crate 01",
      "parent": {"ref": "storage"},
      "components": [
        {"type": "Transform", "properties": {"LocalPosition": {"x": 2, "y": 0, "z": 3}}},
        {"type": "ModelRenderer", "properties": {"Model": {"path": "psx mega pack 2/props/wooden_crate_1"}}}
      ]
    }
  ]
}
```

Keep a local map from your layout names to the returned IDs and component types. Print
`completed`, `history`, `refs` and any error — not every returned property.

Four things trip people up here:

- **`ref` names are parent references only.** They address parents of later creations
  and reparents *within the same request*. To update something you just created, use
  its returned ID in the next request.
- **Component operations take component IDs**, not entity IDs. `component.add` is the
  exception — it takes the entity.
- **`properties` is the editing format, `data` is not.** The serialized `data` dump
  from `/entity` contains numeric enums and opaque fields. Send back `properties`.
- **Pine's hierarchy is additive, not a scene graph.** Parent position adds to child
  position, but parent rotation and scale do **not** rotate or scale the child's
  positional offset. Rotations compose and scales multiply. So a scaled group does not
  lay out like you expect — use explicit positions and scales for repeated modules.
  Entity `active`/`static` flags likewise do not propagate to children.

Execute dependent mutations sequentially. Everything runs on the editor's main thread,
and racing edits against camera moves and saves only makes ordering harder to reason
about.

## 5. Place things accurately

For contact with a real surface, get a point and a normal first:

- `POST /spatial/raycast` — works in stopped mode with no Collider and no viewport.
  Cast from the side the prop should sit on, because the returned normal opposes the
  ray and `entity.place` wants a normal pointing toward the prop.
- `POST /pick` — the surface under a pixel of a capture, for "put it *there*, where I
  can see it". Request `/observe` with `"picking": true` first.

Then use `entity.place`:

- **Surface form** puts the model's bounds (or an explicit local anchor) against that
  plane, with optional `clearance` and `alignment` for wall mounting.
- **Relative form** aligns world bounds min/center/max with another entity per axis and
  adds a world or reference-local offset — the way to get a measured gap between props
  without assuming either pivot is centered.

Use `entity.aim` to point a spotlight or a camera at a picked world point, with
explicit forward and up axes.

What placement will not do: settle an object onto uneven ground, avoid collisions, or
guarantee anything about a finite wall's edges and openings. A raycast gives one point
and one face normal, and on uneven terrain that tangent plane can cut through the
ground under a wide prop. Take more samples and look at the result.

To give props collision, measure them with `/spatial/query` and add a box Collider per
entity from the returned dimensions — remembering that `Size` is **half-extents**, so
it is `dimensions / 2`. This is quick and conservative, and it **overestimates rotated
props**: world-axis bounds of a prop sitting at 45° enclose a good deal of empty space.
Accept that for scenery, and fit those by hand where the volume has to be tight. A
stopped-mode raycast can check a sightline down a street or corridor, but it tests one
line — it is not proof that a player fits through.

## 6. Observe after each stage

The pattern is **edit → camera/frame → observe**, passing the latest mutation's
`observationToken` as `after`. No client sleep is ever needed.

```python
edit = json.load(open('edit-result.json'))
camera = post_json('/camera/frame', {'entities': [{'id': edit['refs']['storage']}], 'padding': 1.2})

observation = post_json('/observe', {
    'after': camera['observationToken'],
    'logsSince': edit['observationToken']['logsSince'],   # cover logs from the edit itself
    'view': 'level',
    'width': 1000,
    'entities': [edit['refs']['crate']],
})

Path('viewport.png').write_bytes(base64.b64decode(observation['image']['data']))
print(observation['frame'], observation['logs'])
```

Preserve the **edit** token's `logsSince` separately when you frame afterwards —
otherwise the capture's log window starts after the edit and hides its errors. `{}`
requests a fresh view but has the same problem.

Inspect the PNG; never print its base64 payload. `width` only downsizes.

**Resist reaching for `/viewport.png` because it is one call.** It has no ordering
guarantee relative to your edit, so a quick intermediate screenshot can show you the
frame *before* the change you are checking. That matters most in exactly the case you
will use it for most — comparing lighting before and after a tweak. Use ordered
observations throughout and the comparison means something.

Use two views, not one: an elevated overview for layout, and a walking-height view for
clearance, shelf contents, wall gaps and lighting. Framing a whole building tells you
nothing about its interior, and a central column may force an offset aisle camera.

## 7. Tune lighting at the actual scene scale

Pine uses windowed inverse-square falloff: `Range` sets the cutoff, `Intensity`
controls brightness. Engine defaults are not useful values for an interior. In one
warehouse scene, point lights 4–5 units above the floor were still dim at intensity 9;
warm lights at 65 with cooler fills at 22 gave a usable interior. Those are
scene-specific numbers — the transferable part is that you should expect to iterate,
and that you will need a bigger far plane for a large scene. See
[lighting](rendering.md#lighting) for slot limits and direction conventions.

**Check `Type` and emitter placement before concluding the renderer is wrong.** Odd
shadows and upward light spill from wall and street lamps are usually a shaded fixture
that was given an omnidirectional `PointLight` where it should have a `SpotLight` aimed
downward — an authoring mistake that looks like a rendering bug. Set the type, aim it
with `entity.aim`, and sink the emitter just below its housing so the housing does not
sit in its own cone. Verify with ordered before/after captures **from the same camera
pose**; two captures from different angles cannot tell you whether anything improved.

## 8. Save and verify

1. `/level/save` (or save-as for a new destination). Check `fileWritten` and that
   `/level/status.unsavedChanges` is false, while stopped.
2. Capture a useful view and record the hierarchy before any reload check.
3. If reloading is appropriate, load the saved path. **Loading clears history and
   replaces every entity and component ID** — reacquire them from `/entities` and
   `/entity`.
4. Compare hierarchy, names, components, intended properties and asset references. Do
   not compare old and new scene IDs. Check the selected Game camera separately.
5. Capture again, check the logs, and leave the user a useful editor view.

When counting entities, remember `/entities.entities` holds **roots** with children
nested recursively, and both its `count` and `/status.entityCount` include temporary
editor entities:

```python
def scene_entities(roots):
    for entity in roots:
        if entity['temporary']:
            continue
        yield entity
        yield from scene_entities(entity['children'])
```

## Importing a model that does not exist yet

When a prop is missing from the catalog, author it externally (Blender and a Python
script work well) and bring it in with `POST /assets/import`, giving an absolute path
and a destination such as `models/generated`. Import with `overwrite: false` first; to
revise the same asset, re-import the same destination with `overwrite: true` and a new
mutation key, which preserves its ID.

If you delegate the modeling to a sub-agent, keep Editor mutations, imports and final
placement with the agent driving the Editor, and give the modeling agent a bounded
brief: purpose and intended placement, dimensions in Pine units, the ground/contact
pivot, forward direction and final Y-up orientation (Blender authors Z-up — verify the
exported GLB axes), a polygon budget and material style, which existing textures to
reuse, and the deliverables — a self-contained `.glb`, the editable `.blend`, a
reproducible script, preview PNGs, and a manifest of bounds, triangle count, materials
and texture provenance.

Then check it **in Pine**: use the returned Model ID in a ModelRenderer, read `/asset`
bounds, and capture the prop next to nearby assets. A Blender preview does not validate
Pine materials, scale or orientation. Keep authoring files in the project, e.g.
`data/projects/<project>/authoring/<prop>/` — import copies the GLB into `content/` but
does not archive the source `.blend` or the script.

Project textures are often embedded inside source GLBs under `content/` rather than
stored as separate PNGs; extract the image bytes when reusing them and record where
they came from. Reusing source pixels does not reuse the existing Pine asset ID, so
expect duplicate imported materials and textures for now.

## Recovering a lost reply

Before sending a mutation, persist its route, exact body bytes, the server session and
a unique `Idempotency-Key`. Reuse those exact values if the reply never arrives.
Generate a new key only for a new logical operation — never automatically on retry.

```sh
curl --silent --show-error --fail-with-body \
  -H 'Content-Type: application/json' \
  -H 'X-Pine-Session: <session from GET /requests>' \
  -H 'Idempotency-Key: <unique key retained for this operation>' \
  --data-binary @edit.json http://127.0.0.1:9002/edit -o edit-result.json
```

**A 504 is not a cancellation.** If the handler already started, it keeps running.
Poll `GET /requests?id=<key>` with the original session header and read the nested
`result.status` and `result.body` — a 200 on the *lookup* says nothing about whether
the write succeeded.

Retained outcomes last ten minutes after completion, capped at 256 identities. After
expiry, a restart, or an `unknown` status, reconcile the live scene before sending
another creation. Records survive scene replacement, so replaying an old creation
after a level load returns its old result with IDs that no longer exist.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Connection refused | Port, Editor process, `PINE_DEBUG_SERVER`, and whether your sandbox shares a network with the Editor. |
| 400 on an edit | The error's `path` and `operation` index, the running schema, exact property case, entity vs. component ID, all three vector coordinates, and the batch limits. |
| 409 on a write | Play state is not `stopped`; an import dialog is open; a destination conflict; mouse capture held for an editor-camera write. Read the actual error. |
| 409 on a capture | Viewport selected and visible, perspective camera, valid scene token, entities still alive. A Game capture also needs a selected scene Camera. |
| 409 on a pick | Capture expired, was evicted, or belongs to a replaced scene. Take a new `/observe` with `picking: true` while stopped. |
| 500 or 504 | Inspect the retained result and the partial-execution flags **before** writing again. A timeout is not a cancellation. |
| Dark or empty model | Model and `MeshIndex`, asset bounds vs. scale, camera position and clipping, material references, active flags, light placement and intensity — then read the logs. |
| Textures look stretched or smeared | Non-uniform `LocalScale` somewhere in the chain — check the entity's **parents** too, since scales multiply. There is no UV fix through this API; undo the scale and repeat a module instead. |
| Odd shadows or light spilling upward | Check the light `Type` first. A shaded fixture with a `PointLight` instead of an aimed `SpotLight` looks like a renderer bug and is not one. |
| Physics looks unchanged while stopped | Actors and shapes are created on Play. Box Collider `Size` is **half-extents**, scaled by the entity; `Position` is an unscaled, unrotated world-axis offset. |
| Counters disagree with the picture | `/stats` counters are diagnostics. One run reported zero `lightCount` and `vertexCount` over visibly lit geometry; that was never diagnosed. |
| Feature missing entirely | Ambient/fog/skybox, Blueprint spawning, play control, material authoring, 2D and viewport switching are not exposed. Use the UI. |

## Verifying a change to the debug server itself

There is no test framework in this repo. `Editor/src/DebugServer/Verification/` holds
per-area Python recipes (`verify-<area>.py`) that drive a running Editor over HTTP, and
`.inc` native probes that some of them compile against the Editor's own build flags to
check state the HTTP API deliberately does not expose.

```sh
cmake -S . -B build
cmake --build build --target Editor -j4
python3 Editor/src/DebugServer/Verification/verify-<area>.py --build build
```

Recipes that take `--build` launch and shut down their own disposable project under
Xvfb. The rest take `--url http://127.0.0.1:<port>` and need an Editor you already
started, with the relevant viewport open.

Two things to know before running them:

- **Point them at a disposable project, never the user's.** They create entities,
  import fixtures and write Level files, and not all of them clean up. Each defaults
  to a different port for this reason.
- **Order matters between recipes sharing an Editor.** `verify-schema.py` asserts
  `/entities` is unchanged and fails if another recipe ran against that Editor first.

When you add a route, add or extend the recipe for its area, and update the
[HTTP reference](debug-server.md).

# Building a scene through the debug server

How to drive a running Editor to assemble, inspect and save a 3D scene. Every route,
field and limit mentioned here is specified in the
[HTTP reference](debug-server.md); this page is about the order to do things in and
the mistakes that cost time.

The shape of the loop is always the same:

> **measure → edit → capture → look at the PNG**

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

**Names will not tell you what an asset looks like**, and a catalog of a thousand
models tends to have cryptic paths, so shortlisting by name gets you candidates and
nothing more. Settle the rest with two routes, neither of which touches the scene.

`POST /assets/summary` takes up to 256 references and returns each one's bounds,
mesh and vertex counts and materials in a single reply — enough to drop the
candidates that are the wrong size or share a material you already rejected:

```python
shortlist = [{'path': path} for path in candidates]
for asset in post_json('/assets/summary', {'assets': shortlist})['assets']:
    print(asset['path'], asset['bounds']['size'],
          [material['path'] for material in asset['materials']])
```

Then **look at what survives**. `GET /asset/preview.png` renders one model or
material on its own, framed to its bounds:

```sh
curl --silent --show-error --fail-with-body --get \
  --data-urlencode 'path=psx mega pack 2/props/wooden_crate_1' \
  --data-urlencode 'width=384' --data-urlencode 'height=384' \
  http://127.0.0.1:9002/asset/preview.png -o crate.png
```

Add `yaw` and `pitch` to turn the subject when one angle hides the answer. The preview
is the asset browser's icon pass, so its colours are approximate and its lighting is
not the scene's — it is for telling variants apart, not for judging how a material will
actually look. Build a throwaway **palette scene** only when you need to see candidates
at scene scale next to each other; for "which of these six lamps" the previews are
faster and leave no scene to clean up.

## 3. Measure before you repeat a prop

`POST /assets/summary` (above) gives the live model's aggregate bounds directly, which
is what you usually want. `GET /asset` gives the **stored** model's per-mesh bounds,
for when you need them separately:

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

- **`ref` names reach only a few fields.** Within the same request they address the
  `parent` of later creations and reparents, and the `target` of `entity.place`,
  `entity.aim` and `entity.fitCollider` (see [section 5](#5-place-things-accurately)). Every
  other target, including `entity.update` and all component operations, needs the
  returned ID in the next request.
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

To give props collision, add a box Collider and let `entity.fitCollider` size it from
the entity's own model geometry:

```python
edit([
    {'op': 'component.add', 'target': {'id': prop}, 'type': 'Collider'},
    {'op': 'entity.fitCollider', 'target': {'id': prop}, 'padding': 0.02},
])
```

Do not build the box from `/spatial/query` dimensions by hand. Those are world-axis
bounds, so a prop sitting at 45° gets a box √2 too wide and full of empty space.
`entity.fitCollider` measures in model space and the shape turns with the entity, so a
rotated prop keeps a tight volume. It also saves you the two things that are easy to
get wrong: `Size` is **half-extents before world scale**, and `Position` is an
unscaled, unrotated world-axis offset that has to account for an off-centre pivot.
`padding` is pre-scale like `Size`, so on a prop scaled by 3 a `padding` of 0.02 clears
it by 0.06 world units.

`entity.place`, `entity.aim` and `entity.fitCollider` all accept a batch `ref` as their
target, so a prop can be created, stood on the ground, turned and given a fitted
collider in **one** request:

```python
edit([
    {'op': 'entity.create', 'ref': 'crate', 'name': 'Crate', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': crate_model}}},
        {'type': 'Collider'}]},
    {'op': 'entity.place', 'target': {'ref': 'crate'},
     'surface': {'point': {'x': 12, 'y': 0, 'z': -40}, 'normal': {'x': 0, 'y': 1, 'z': 0}},
     'anchor': {'type': 'modelBounds'}},
    {'op': 'entity.fitCollider', 'target': {'ref': 'crate'}},
])
```

Every other target still wants an ID, so the create → collect IDs → adjust sequence
remains for anything else.

A stopped-mode raycast can check a sightline down a street or corridor, but it tests one
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
captures throughout and the comparison means something.

Use two views, not one: an elevated overview for layout, and a walking-height view for
clearance, shelf contents, wall gaps and lighting. Framing a whole building tells you
nothing about its interior, and a central column may force an offset aisle camera.

**Use `POST /render` when the camera is yours to choose.** It takes a pose and a size,
renders the scene through the same pipeline, and leaves the user's editor camera where
they left it — so an inspection sweep does not drag their view around, and the size is
not whatever their viewport happens to be:

```python
capture = post_json('/render', {
    'position': {'x': 12, 'y': 1.7, 'z': -40},
    'lookAt': {'x': 12, 'y': 1.7, 'z': -58},
    'width': 1280, 'height': 720,
})
Path('aisle.png').write_bytes(base64.b64decode(capture['image']['data']))
```

It needs no `after` token: the capture renders on a frame after the request was
accepted, so any edit whose reply you already have is in it. Keep `/observe` for the
things it uniquely gives you — entity readback, an incremental log window, capture
picking, and seeing the Game camera's own view — and reach for `/render` for the
pictures. Store the pose you used; posting the reply's `camera` back reproduces it.

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
sit in its own cone. Verify with before/after captures **from the same camera pose**;
two captures from different angles cannot tell you whether anything improved. `/render`
is the route that makes this straightforward: send the same pose and size twice, once
either side of the change, and nothing else can have moved between them.

**Ambient light is not a substitute for a light.** Pine shades nothing without one, so a
scene with no `Light` renders black however bright `AmbientColor` is — and a "the setting
had no effect" conclusion drawn from that scene is wrong. Place a light first, then tune.

`POST /level/settings` authors the rest of the atmosphere: skybox, ambient, fog, exposure,
bloom and the grain/vignette look. It merges, so a request names only what it changes:

```python
post_json('/level/settings', {'properties': {
    'AmbientColor': {'x': 0.12, 'y': 0.13, 'z': 0.18},
    'FogColor': {'x': 0.2, 'y': 0.25, 'z': 0.3, 'w': 1.0},
    'FogDistance': 80, 'FogIntensity': 0.35, 'Exposure': 1.4,
}})
```

Reach for `Exposure` before scaling every light in the scene: if the whole frame is too
dark or blown out, one exposure change is the cheaper fix and does not disturb the
relative balance you already tuned. Each request is one undo step, so an experiment costs
nothing. **Turn `GrainStrength` off before comparing captures** — the grain is animated,
so two otherwise identical `/render` calls differ while it is on.

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
| 409 on a write | Play state is not `stopped`; an import dialog is open; an import destination conflict; mouse capture held for an editor-camera write. Read the actual error. A save-as destination conflict is a 400. |
| 409 on a capture | Viewport selected and visible, perspective camera, valid scene token, entities still alive. A Game capture also needs a selected scene Camera. `/render` needs none of that — use it when the viewport is the problem. |
| 409 on an asset preview | Only Model and Material assets have one. Everything else is identity and bounds through `/assets/summary`. |
| 409 on a pick | Capture expired, was evicted, or belongs to a replaced scene. Take a new `/observe` with `picking: true` while stopped. |
| 500 or 504 | Inspect the retained result and the partial-execution flags **before** writing again. A timeout is not a cancellation. |
| Dark or empty model | Model and `MeshIndex`, asset bounds vs. scale, camera position and clipping, material references, active flags, light placement and intensity — then read the logs. |
| Textures look stretched or smeared | Non-uniform `LocalScale` somewhere in the chain — check the entity's **parents** too, since scales multiply. There is no UV fix through this API; undo the scale and repeat a module instead. |
| Odd shadows or light spilling upward | Check the light `Type` first. A shaded fixture with a `PointLight` instead of an aimed `SpotLight` looks like a renderer bug and is not one. |
| Physics looks unchanged while stopped | Actors and shapes are created on Play. Box Collider `Size` is **half-extents**, scaled by the entity; `Position` is an unscaled, unrotated world-axis offset. `entity.fitCollider` gets both right for you. |
| 400 from `entity.fitCollider` | No Collider on the entity, no ModelRenderer geometry, a non-Box collider, or a model that is flat on one axis — the last needs `padding`. |
| Counters disagree with the picture | `/stats` counters are diagnostics. One run reported zero `lightCount` and `vertexCount` over visibly lit geometry; that was never diagnosed. |
| Preview colours look wrong | Expected. `/asset/preview.png` is the icon pass and gets no display transform; it separates variants, it does not show final appearance. Use `/render`. |
| Setting the atmosphere changed nothing visible | A scene with no `Light` renders black whatever `AmbientColor` is. Check `/stats.level.lightCount` (or `game.lightCount`) before blaming the setting. |
| Two captures differ for no reason | `GrainStrength` is animated. Set it to 0 through `/level/settings` before comparing. |
| Feature missing entirely | Blueprint spawning, play control, material authoring, 2D and viewport switching are not exposed. Use the UI. |

## Verifying a change to the debug server itself

There is no test framework in this repo. `Editor/src/DebugServer/Verification/` holds
per-area Python recipes (`verify-<area>.py`) that drive a running Editor over HTTP, and
`.inc` native probes that some of them compile against the Editor's own build flags to
check state the HTTP API deliberately does not expose.

```sh
cmake -S . -B cmake-build-debug-agent -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug-agent --target Editor -j4
python3 Editor/src/DebugServer/Verification/verify-<area>.py --build cmake-build-debug-agent
```

Recipes that take `--build` launch and shut down their own disposable project under
Xvfb, through `headless_command` in `headless.py`. It renders on the GPU through VirtualGL when
`vglrun` is installed, as described in [editor.md](editor.md#running-it-headlessly). The rest take `--url http://127.0.0.1:<port>` and need an Editor you already
started, with the relevant viewport open.

Two things to know before running them:

- **Point them at a disposable project, never the user's.** They create entities,
  import fixtures and write Level files, and not all of them clean up. Most default to
  a port of their own for this reason, but `verify-aim.py`, `verify-placement.py` and
  `verify-spatial.py` share 19041, and `verify-duplication.py` and `verify-import.py`
  share 19029. Pass `--url` or `--port` to run those side by side.
- **Order matters between recipes sharing an Editor.** `verify-schema.py` asserts
  `/entities` is unchanged and fails if another recipe ran against that Editor first.

When you add a route, add or extend the recipe for its area, and update the
[HTTP reference](debug-server.md).

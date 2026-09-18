# Debug-server scene editing

Start with the [Editor API guide](debug-server.md) for the complete route list and
scene-authoring workflow. This page defines the batch-editing contract.

The Editor exposes `POST /edit` and `GET /edit/schema` when started with
`PINE_DEBUG_SERVER=<port>`. These use the existing localhost server and main-thread
request queue. All implementation lives under `Editor/src/DebugServer/Editing/`;
engine components and binary serialization have no dependency on this protocol.

For positioning the editor's view and framing created entities, see
[editor-camera controls](debug-server-camera.md).
Scene Camera properties and game-camera selection are documented in
[scene cameras](debug-server-scene-camera.md).
Collider and RigidBody authoring is documented in [3D physics](debug-server-physics.md).

## Scope and semantics

- Edits require **Stopped** play mode; Playing and Paused return HTTP 409.
- `entity.create` creates a scene entity. Its optional `components` array configures
  its existing Transform and adds ModelRenderer, Light, Camera, Collider and/or
  RigidBody. Each type may appear once. Other components are unsupported by this API.
- `entity.update` renames an existing scene entity and changes its active/static
  flags through public setters. It takes an entity UId and an optional-property
  patch; see [entity properties](#update-entity-properties).
- `entity.reparent` moves an existing entity and its hierarchy under a scene
  parent, or detaches it to the scene root. It preserves local transforms and
  rejects cycles; see [reparenting](#reparent-entities).
- `entity.delete` deletes an existing scene entity and all its descendants,
  clearing affected selections and camera references; see [deletion](#delete-entities).
- `entity.duplicate` copies an existing scene entity and its hierarchy with fresh
  entity/component IDs. It supports Transform, ModelRenderer, Light, Camera,
  primitive Collider and RigidBody; see [duplication](#duplicate-entities).
- `entity.place` positions an existing entity against a supplied surface plane using
  model bounds or a local anchor, with optional axis alignment and clearance. Its
  alternative relative form aligns world bounds with another entity and adds a
  world or reference-local offset; see [entity placement](debug-server-placement.md).
- `entity.aim` rotates an existing entity toward a world point with explicit local
  forward/up axes and world up, preserving position and scale; see
  [aiming](debug-server-placement.md#aim-at-a-world-point).
- `component.update` changes an existing component, addressed by its persistent
  component UId (not the entity ID or pool index). It does not add components.
- `component.add` adds ModelRenderer, Light, Camera, Collider or RigidBody to an
  existing scene entity, addressed by entity UId. It accepts optional initial properties and rejects a type already
  present on that entity at this point in the batch.
- `component.remove` removes an existing ModelRenderer, Light, Camera, Collider or
  RigidBody by component UId. Transform cannot be added or removed: every entity must retain its original
  Transform, which the supported additions depend on. Other component lifecycles
  remain unsupported; removal never cascades to other components.
- Omitted properties retain current values on update and engine defaults on
  creation. Supplied vectors replace the whole vector; all coordinates are required.
- Operations execute in array order. Repeated updates to the same component are
  validated against the state proposed by preceding operations in the batch.
  Remove-then-add of the same type is allowed and creates a new component identity.
  Updating or removing an ID removed earlier in the batch rejects the entire batch.
- Entity `ref` names are unique within a request. A parent can be `{ "ref": "name" }`
  referring to an **earlier** creation or duplication root, or `{ "id": "<entity-uid>" }` for an existing
  scene entity. Omit `parent` to create a root. Parenting keeps the supplied local
  transform. Temporary editor entities and descendants are excluded.
- Each successful batch is one editor undo step. Changes affect the live scene;
  save explicitly through the editor or debug server to persist them. There is
  no automatic rollback. See [history and persistence](debug-server-history.md).

## Example

Save this as `edit.json`, substituting a loaded model's virtual path from `/assets`:

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.create",
      "ref": "model",
      "name": "PoC model",
      "components": [
        {
          "type": "Transform",
          "properties": {
            "LocalPosition": { "x": 0, "y": 0, "z": -5 }
          }
        },
        {
          "type": "ModelRenderer",
          "properties": {
            "Model": { "path": "<loaded-model-path>" }
          }
        }
      ]
    },
    {
      "op": "entity.create",
      "ref": "light",
      "name": "PoC light",
      "parent": { "ref": "model" },
      "components": [
        {
          "type": "Transform",
          "properties": {
            "LocalPosition": { "x": 1, "y": 2, "z": 2 }
          }
        },
        {
          "type": "Light",
          "properties": { "Type": "PointLight", "Intensity": 3, "Range": 8 }
        }
      ]
    }
  ]
}
```

```sh
curl -sS http://127.0.0.1:9002/edit/schema
curl -sS -H 'Content-Type: application/json' --data-binary @edit.json http://127.0.0.1:9002/edit
```

The response contains `refs` mapping request names to entity IDs, and `results`
with entity/component IDs and actual writable property values after each operation.
Use the returned Light component ID for an update:

```json
{
  "version": 1,
  "operations": [
    {
      "op": "component.update",
      "target": { "id": "<light-component-uid>" },
      "properties": { "Intensity": 5, "Range": 12 }
    }
  ]
}
```

## Add and remove components

Add a component to an entity that already exists before the request:

```json
{
  "version": 1,
  "operations": [
    {
      "op": "component.add",
      "target": { "id": "<entity-uid>" },
      "type": "Light",
      "properties": { "Type": "PointLight", "Intensity": 3, "Range": 8 }
    }
  ]
}
```

Its result contains `operation`, `entityId`, and
`component: {"id": "<new-component-uid>", "type": "Light", "properties": {...}}`.
Omit `properties` to use the defaults advertised in `/edit/schema`. Addition uses
the same property validation and public setters as entity creation and updates.

Remove that component using its returned component ID:

```json
{
  "version": 1,
  "operations": [
    { "op": "component.remove", "target": { "id": "<component-uid>" } }
  ]
}
```

Its result contains `operation`, `entityId`, and
`removedComponent: {"id": "<component-uid>", "type": "Light"}`. The owning entity,
its Transform, and other components remain intact. `/entity` and observations no
longer list the removed component.

These component-operation targets accept **only** `{ "id": "..." }`: addition
takes an entity ID; component update and removal take a component ID. Request `ref`
names are parent references for creation and reparenting, including refs assigned
to duplicated roots. To configure components on an entity created in this batch,
use its `components` array. To update or remove a newly added component, use its
returned ID in a subsequent request.

Membership and pool capacity are validated in operation order before any mutation.
A removal frees capacity for later additions or entity creations. A later removal
cannot rescue an earlier allocation that would exhaust its pool. Existing scenes
may contain multiple components of a type created outside this API: removal targets
exactly the specified ID; addition requires all existing instances of that type to
have been removed first. There is no implicit replacement or dependency creation.

Removed IDs stay invalid even if Pine reuses the same pool slot. A replacement gets
defaults plus its supplied properties and a fresh ID. As with other mutations,
retry an identified add/remove with the same headers and exact body to recover its
original result. Retrying a removal does not remove a later replacement; sending
a fresh request to remove the old ID is a validation error.

## Update entity properties

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.update",
      "target": { "id": "<entity-uid>" },
      "properties": { "name": "Courtyard lamp", "active": true, "static": true }
    }
  ]
}
```

`properties` is required. Each field is optional; omitted fields retain their
values, and an empty object is a no-op. Names are nonempty strings without null
characters, matching `entity.create`; Unicode and duplicate names are allowed.
Flags accept JSON booleans only. Unknown fields and properties are rejected.
The target accepts only an existing scene entity ID, including inactive entities.
Component IDs, request refs, temporary editor entities and their descendants are
rejected. To update an entity created by this API, use its returned ID in a later
request.

Updates call `Entity::SetName`, `SetActive` and `SetStatic` for supplied fields;
flag changes also mark the entity dirty to refresh cached rendering state.
They follow Pine's current semantics: flags affect only the target entity,
without recursively changing children. An inactive parent does **not** disable
active descendants. Entity activation gates its own components without rewriting
their individual active flags. Static entities remain editable through this API.

Each result contains `operation` and `entity`, using the same description as
creation: `id`, `name`, `active`, `static`, `parent`, and supported `components`.
Values describe the state immediately after that operation. Existing IDs and
parenting stay intact. Repeated patches compose in array order, including when
interleaved with component edits. The entire batch is validated before setters
run, and identified retries return the original result without applying the patch
again. `/entity`, `/entities`, and `/observe` also expose the resulting flags and
name. Use the observation token to verify activation changes in a rendered view.

## Reparent entities

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.reparent",
      "target": { "id": "<existing-entity-uid>" },
      "parent": { "id": "<new-parent-uid>" }
    }
  ]
}
```

`target` accepts only an existing scene entity ID, as with `entity.update`.
`parent` is required: use `{ "id": "..." }` for an existing scene parent,
`{ "ref": "..." }` for an earlier `entity.create` or `entity.duplicate` root in this request, or JSON `null`
to detach to the scene root. Unknown fields, component IDs, missing entities,
forward/unknown refs, temporary editor entities and their descendants are rejected.
Inactive and static scene entities can be targets and parents.

Reparenting preserves **local** position, rotation and scale exactly. It does not
preserve the world transform or offer a world-preservation mode. This follows
Pine's current `Transform` semantics:

- World position = parent world position + local position. Parent rotation and
  scale do **not** rotate or scale the child's positional offset.
- World rotation = parent world rotation × local rotation.
- World scale = parent world scale × local scale, component by component.

At the scene root, world and local values coincide. No matrix decomposition or
inverse-parent transform is applied; zero and negative scales remain valid under
the existing Transform contract. `/edit/schema` advertises the preservation policy
at `entity.reparent.transformPreservation`.

The operation moves the target with all its descendants, preserving entity and
component IDs, local values, flags and selection references. It removes the target
from the old parent's child list and appends it to the new parent's list. Assigning
the current parent, including `null` on a root, is a no-op and preserves sibling
order. Descendants are marked dirty so cached transforms and static rendering
inputs refresh for the next observation.

Cycle validation follows the proposed hierarchy after each preceding operation,
including creations. Self-parenting and parenting beneath a descendant reject the
entire batch before mutation. A later detach cannot rescue an earlier cyclic
operation; detach first, then move. Repeated valid moves compose in array order,
and component updates interleaved with moves retain their intended local values.

Each result contains `operation` and `entity`, with the same description as
creation and entity updates. Its `parent` is the resulting `{ "id": "..." }` or
`null`, and its components expose the unchanged local transform immediately after
that operation. `/entities` reflects both child lists, while `/entity` and
`/observe` report the new parent. Identified retries return the original result
without reapplying a move after intervening edits. The existing stopped-mode,
partial-execution-failure and observation-token rules apply.

## Delete entities

```json
{
  "version": 1,
  "operations": [
    { "op": "entity.delete", "target": { "id": "<entity-uid>" } }
  ]
}
```

`target` accepts only an existing scene entity ID. Component IDs, request refs,
unknown fields, missing entities, temporary editor entities and their descendants
are rejected. A hierarchy containing a temporary entity is also protected.
Inactive and static scene entities can be deleted.

Deletion always includes **all descendants at this point in the batch**, matching
`Entities::Delete`. To keep a child, use `entity.reparent` to detach or move it
before deleting its ancestor. Moving a child into the hierarchy first includes it
in the deletion. There is no implicit promotion of children to the scene root.
All attached components are destroyed through Pine's entity lifecycle. Hierarchies
containing components other than Transform, ModelRenderer, Light, Camera, Collider and RigidBody are rejected
before mutation because they lack a supported undo restoration path. Referenced
assets remain loaded.

Each result contains `operation`, `entityId` (the targeted root), and
`removedEntities`: an array of `{ "id": "<entity-uid>", "componentIds": ["<component-uid>", ...] }`
covering the entire removed hierarchy. These IDs are captured before destruction;
array order is not a destruction-order guarantee. Component IDs reflect preceding
additions and removals in the batch.

The complete batch is validated before any mutation:

- Later entity targets, component targets, and parents cannot reference deleted
  entities or their components. Deleting an ancestor followed by a descendant is
  rejected; deleting the descendant first is valid.
- This also covers earlier creations and duplicates beneath a deleted hierarchy. A deleted
  creation or duplicate's `ref` cannot be used as a parent later in the batch, and names remain
  unique for the whole request. Its entry in the response's `refs` becomes `null`.
  Earlier `results` remain historical descriptions of each completed operation.
- Deletion frees entity and component pool slots for subsequent operations only.
  Capacity accounting includes newly created descendants, reparenting, and preceding
  component additions/removals. A later deletion cannot rescue an earlier allocation.

Affected entities are removed from editor selection before destruction; unrelated
entity or asset selections remain intact. An entity drag originating anywhere in
the removed hierarchy is cancelled, including its pending drop. Perspective Camera
components are supported; deleting the selected camera clears the Game-camera
reference, and undo restores it. See [scene cameras](debug-server-scene-camera.md).

Entity and component handles invalidate when their pool slots become unoccupied
and cannot resolve a replacement after slot reuse. Persistent entity/component IDs
stay invalid after unrelated pool slot reuse; undo can restore the original IDs. `/entity`
returns HTTP 404 for a deleted ID, and fresh edits or camera-framing requests using
it fail validation. An observation listing an already deleted entity fails validation
with HTTP 400; one accepted before deletion fails with HTTP 409 if the entity
vanishes before capture. Deletion does not replace the scene or increment its scene
generation, so references and observation tokens for surviving entities remain usable.
To capture the result, observe the deletion token with surviving entity IDs or omit
`entities`.

Identified retries return the original result without repeating destruction, even
if a pool slot now belongs to a replacement entity. A fresh request targeting the
old ID fails validation unless undo restored it. The existing stopped-mode, execution-failure and observation
rules apply. Execution failures can leave partial destruction and cleared selection,
camera or creation references; inspect current state before continuing. Deletion
registers undo history on success and does not implicitly save the level.

## Duplicate entities

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.duplicate",
      "target": { "id": "<source-entity-uid>" },
      "ref": "copy"
    },
    { "op": "entity.create", "name": "New child", "parent": { "ref": "copy" } }
  ]
}
```

`target` must be an existing scene entity ID. Component IDs, request refs,
temporary editor entities and their descendants are rejected. The only optional
field is `ref`: a nonempty name unique across all creations and duplications in
this request. Unknown fields are rejected, including parent, name, offset and
children options.

Duplication always includes **all descendants at this point in the batch**.
The copy includes preceding property patches, component additions/removals,
reparenting, new descendants (named or unnamed), and earlier duplicates within the
source hierarchy. Later changes to the source do not change the copy. The entire
batch is validated before mutation, including each snapshot's property constraints
and the entity/Transform/component slots needed in operation order. A preceding
deletion can free capacity; a later deletion cannot rescue an earlier allocation.
At most **1024 entities may be duplicated across one batch**, independently of the
128-operation limit. This bounds hierarchy expansion during preparation.

The copied root is appended under the source's current parent, or created at the
scene root when the source has no parent. Child order and all local transforms are
preserved, so the copy initially overlaps its source. Names are copied unchanged.
Entity active/static flags, tags and component active flags are preserved; selection
and the active game camera are unchanged. To rename, move or reparent the copy,
use its returned IDs in a subsequent request.

Supported components are **Transform, ModelRenderer, Light, perspective Camera,
primitive Collider and RigidBody**.
The entire batch is rejected if the source hierarchy contains any other component,
a temporary descendant, repeated component types, or a missing/misplaced required
Transform. Unsupported components are never silently omitted. Writable properties
are copied through the existing adapters and public setters. ModelRenderer's
stencil override flag/value and Camera's clear color, aspect override and dormant
orthographic size are also preserved. Rendering caches are fresh and
the copied hierarchy is marked dirty, including static descendants.

Every copied entity and component receives a **fresh persistent ID**. The response
for the operation contains:

- `operation`: its zero-based batch index.
- `entity`: the copied root's usual editing description.
- `duplicatedEntities`: an array containing every copied node, root first, then
  descendants in breadth-first child order. Each entry has `sourceId`, `entity`
  (the copy's editing description), and `componentIds` (an object mapping each
  source component ID to its copied component ID). Source IDs include entities
  and components created by earlier operations in this request.

Parent links within the copied hierarchy point to copied parents. The root keeps
its external parent. Model and material references retain the same loaded asset
IDs; assets remain shared. The supported authored properties contain no other
entity/component references. This operation does not promise reference remapping
for scripts or other unsupported component types.

An optional root `ref` is returned in the batch's `refs` map and can be used by
subsequent **parent** fields. It is not an entity/component target or a way to
address copied descendants within this batch. Deleting a hierarchy containing the
copy later in the batch invalidates that ref to `null`; earlier results remain
historical. Duplication does not replace the scene or change its generation, so
existing IDs, handles and observation tokens for surviving entities remain valid.

Identified retries return the original result and mappings without creating more
copies, even after intervening edits or deletion of the source/copy. The normal
stopped-mode and observation-token rules apply. Execution failures may leave a
partial hierarchy: `createdEntityId` identifies the root when known, and
`createdEntities` lists known allocations as `{ "sourceId", "id", "componentIds" }`
entries. These mappings can be incomplete if allocation/application failed. Inspect
current state before continuing. Successful duplication is part of the batch's
undo step; it does not roll back a failing batch or implicitly save the level.

## Operation and reference discovery

`GET /edit/schema` describes how to construct the `POST /edit` body. This is an
additive extension of version 1: `operations` remains an array of operation names,
and the existing `entity`, `components`, limits and capability fields remain available.

- `requestSchema` describes the required `version` and `operations` fields. The
  operations array accepts 1 through `maxOperations` entries, selected by `op` from
  `operationSchemas`. `maxBodyBytes` and `maxJsonDepth` apply to the whole body.
- `operationSchemas` describes every supported operation's required and optional
  fields, including nested component entries, target kinds and parent forms.
  Operations with alternative forms, such as `entity.place`, also advertise
  `oneOf` entries listing each form's `required` and `forbidden` fields.
- `referenceRules` describes reference value formats, scene/asset resolution and
  the scope, declaration and permitted uses of batch names.
- `batchRules` advertises array-order execution and validation against the state
  proposed by preceding operations before any mutation. Execution failure can
  leave completed operations applied; the existing `undo` and `rollback` flags
  still apply. Reference IDs must already exist before the batch.

For example, `operationSchemas["component.add"]` contains:

```json
{
  "type": "object",
  "required": ["op", "target", "type"],
  "additionalFields": false,
  "fields": {
    "op": { "type": "string", "const": "component.add" },
    "target": { "type": "reference", "kind": "entity", "forms": ["id"] },
    "type": { "type": "enum", "values": ["ModelRenderer", "Light"] },
    "properties": {
      "type": "componentProperties",
      "selectedBy": "type",
      "whenOmitted": "defaults",
      "omittedProperties": "defaults"
    }
  }
}
```

The description vocabulary is deliberately small:

| Field or type | Meaning |
|---|---|
| `fields`, `required`, `additionalFields` | Object members, mandatory members, and whether unlisted members are accepted. Fields absent from `required` are optional. |
| `const`, `values` | Exact required value, or accepted enum values. |
| `items`, `minItems`, `maxItems`, `uniqueBy` | Array entry description, size limits, and a field whose value cannot repeat within the array. |
| `nullable`, `whenNull` | Null is rejected unless `nullable` is true; `whenNull` describes its effect. |
| `default`, `whenOmitted` | Value or behavior when an optional field is absent. Omission and null are separate. |
| `minLength`, `allowNullCharacters` | String restrictions; names and reference values must be nonempty and contain no null characters. |
| `operation` | Resolve the entry's `op` through `operationSchemas`. |
| `entityProperties` | A patch using `entity.properties`. Empty patches are accepted; unknown properties are rejected. |
| `componentProperties` | A patch using `components[componentType].properties`. `selectedBy: "type"` uses the sibling `type`; `"targetComponentType"` uses the component addressed by the target ID. Empty patches are accepted; unknown properties are rejected. |
| `omittedProperties` | Unspecified patch properties use adapter `defaults` on creation/addition and `retain` their proposed values on updates. Supplied vectors replace the whole vector. |
| `reference` | An object with exactly one member named in `forms`; its value uses the corresponding description in `referenceRules.forms`, and its `kind` selects the resolution rules. |
| `role: "declareBatchRef"` | This string declares an operation-root name, governed by `referenceRules.batchRefs`. |

The creation `name` defaults to `Entity`. Omitting `components` creates only the
required Transform; a Transform entry configures that component. Omitting a
creation's `parent` creates a scene root (`whenOmitted: "sceneRoot"`); an explicit
null is rejected. Reparenting requires `parent`, with null meaning detach to the
root (`whenNull: "sceneRoot"`). Duplication has no parent field and retains its
source's parent, as described under `entity.duplicate` in the existing schema.

Entity and component targets accept only `{ "id": "..." }`, using their respective
persistent IDs. Scene IDs exclude temporary editor hierarchies and are rejected
if removed earlier in the batch. IDs use 1–16 hexadecimal digits, a dash, and 16
hexadecimal digits; the first group must be nonzero. The advertised `pattern`
describes the string shape; the `pineUid` format also requires that nonzero group.

Parent fields additionally accept `{ "ref": "..." }` for an earlier creation or
duplication root in the same request. Declarations must be unique across both
operations, and a name cannot be reused after its entity is deleted. Forward,
self, deleted and previous-request references are rejected. Names are never
accepted as operation targets. Use returned entity/component IDs in subsequent
requests to target newly allocated objects.

Existing property descriptors with `type: "asset"` use `referenceRules.asset`:
exactly one of `id` or virtual `path`, resolving an already loaded asset matching
the property's `assetType`. Readback uses IDs. Only nullable asset properties
accept null to clear the reference.

Discovery describes the request format; scene-dependent checks such as hierarchy
cycles, component membership, capacity and related property values still happen
on the server. Consult the existing entity policies and adapter descriptions for
operation behavior and property constraints.

## Property formats

`GET /edit/schema` describes writable properties, defaults, accepted enum names,
numeric bounds, units and asset types. This is a small editing-specific description,
not a general JSON Schema implementation.

Entity update properties are advertised separately at `entity.properties` as
`name` (string), `active` (boolean), and `static` (boolean). Their names match the
entity descriptions returned by creation, updates and inspection.

Current component adapters:

| Component | Properties |
|---|---|
| Transform | LocalPosition, LocalRotation, LocalScale |
| ModelRenderer | Model, OverrideMaterial, MeshIndex |
| Light | Type, Color, Intensity, Range, CastShadows, SpotlightOuterAngle, SpotlightInnerAngle |
| Camera | Type (Perspective), FieldOfView, NearPlane, FarPlane; see [scene cameras](debug-server-scene-camera.md) |
| Collider | Type, Position, Size, Layer, LayerMask, IsTrigger, TriggerMask; see [physics authoring](debug-server-physics.md) |
| RigidBody | Type, Mass, GravityEnabled, PositionLock, RotationLock, MaxLinearVelocity, MaxAngularVelocity; see [physics authoring](debug-server-physics.md) |

Each component description also advertises `addable` and `removable`. These are
true for ModelRenderer, Light, Camera, Collider and RigidBody, and false for Transform.

Vectors are `{x,y,z}`; rotations are `{x,y,z,w}` quaternions. Nonzero quaternions
are normalized. Numbers must be finite and fit float32; integer properties reject
fractional values. All transform properties use Pine's existing local-space semantics.

Light types are `Directional`, `PointLight`, and `SpotLight`. Light color is linear
RGB, intensity is nonnegative, range is at least 0.01 world units, and spotlight
half-angles satisfy `0 <= inner <= outer`, with outer between 1 and 89 degrees.
The proposed angle pair is checked together before setters run.

Asset references accept exactly one of `{ "id": "<uid>" }` and
`{ "path": "<virtual-path>" }`. Assets must already be loaded and match the expected
type. Responses normalize references to IDs. JSON `null` clears an optional asset
reference. MeshIndex is -1 for all meshes, or an index within the selected model;
clearing Model requires MeshIndex to be -1 as well.

## Writable-state readback

`GET /entity?id=<entity-uid>` (or `?internalId=<pool-slot>`) includes `properties`
on the entity and each component. `POST /observe` returns the same fields for its
requested entities. `/edit/schema` advertises these locations in `readback`.

- Entity `properties` contains exactly `name`, `active` and `static`, ready for
  `entity.update.properties`.
- Each supported component's `properties` contains all properties advertised by
  its adapter, ready for `component.update.properties`. Transform values are local;
  Light types use named enums; ModelRenderer asset references use `{ "id": "..." }`
  or null. The same adapter reads both these fields and editing responses.
- Unsupported components have `properties: null`. Temporary editor entities and
  every entity beneath them have null entity and component properties. These
  objects remain available for inspection, but do not advertise writable patches.
- The existing identity, hierarchy and serialized `data` fields remain available.
  `data` can contain numeric enums or opaque fields and is **not** the editing format.
  `/entities` remains the lightweight hierarchy listing without property objects.

For example, read a Light component and send its property object back unchanged,
or change `Intensity` before submitting it:

```python
state = get_json('/entity?id=' + entity_id)
light = next(c for c in state['components'] if c['type'] == 'Light')
properties = dict(light['properties'])
properties['Intensity'] = 4
post_json('/edit', {'version': 1, 'operations': [{
    'op': 'component.update',
    'target': {'id': light['id']},
    'properties': properties
}]})
```

Use the entity/component `id` as the operation target; submit only `properties`
as the patch. Readback does not include noneditable fields such as component
active flags, tags or hierarchy links in that patch.

Readback samples live getters on the main thread without setters or validation.
It is available during play, although writes still require stopped mode. Values
loaded from older assets or changed through native code/UI can violate current
edit constraints; reads do not repair them, and resubmission still goes through
normal validation. Valid values can be reapplied, subject to float32 quaternion
normalization. A full-property patch may overwrite intervening changes; submit
only changed properties when that is preferable. IDs/assets must still exist when
the write executes.

`/entity` reports state when its queued read executes. `/observe` samples properties
with the other entity fields after rendering and before UI/debug writes, following
the existing [observation timing](debug-server-observation.md#timing-and-intervening-changes).
Neither read is a saved snapshot for subsequent requests or an automatic edit.

## Validation and failure

The entire batch is prepared without mutating live objects: field/type checks,
target and asset resolution, related-property checks, and pool capacity checks.
Unknown fields/properties, missing coordinates, unsupported operations/components,
invalid IDs and references fail validation. Limits are 128 operations, 256 KiB of
JSON and 32 levels of nesting.

Validation errors return HTTP 400 with `phase: "validation"`, `completed: 0`, an
`error`, and a slash-separated `path`. Operation-specific errors also include the
zero-based `operation` index.

Execution failures return HTTP 500 with `phase: "execution"`, completed results,
the failing operation index and `failedOperationMayHaveChangedState: true`. When
known, the entity created by the failing operation is returned as `createdEntityId`.
If `component.add` allocated and attached a component before failing to apply its
properties, the response includes `createdComponentId` and its owning `entityId`.
Earlier operations remain applied; later operations are skipped. Success is HTTP
200 with `completed` equal to the operation count.

Mutating requests support opt-in identities, retained status/results and cancellation
before execution. Queued requests that time out cannot execute later. Running writes
may finish after an HTTP timeout; recover their result with the same identity.
See [request retries and completion](debug-server-requests.md) for the headers,
retention window and failure rules.

Edits execute after rendering. Their responses confirm state changes; use the
returned observation token with `/observe` to capture a subsequent rendered frame.

## Extending the implementation

`Editing.cpp` handles batch preparation and execution. `Values/` handles common
validation and conversion. `Schema/` describes request/operation envelopes and
reference rules; keep those descriptions paired with preparation changes and run
the schema verification recipe below. Component choices come from the registered
adapters and their lifecycle capabilities. Each `Components/<Type>/` adapter owns its advertised
schema, reads actual/default values, validates related properties, and applies
prepared state through public component setters. Register new adapters in
`Components::GetAdapters()`.

Adapter application must keep component behavior correct; do not replace it with
JSON-to-binary followed by `LoadData()`. That bypasses setter behavior for some
components. More complex component lifecycles should be evaluated before enabling
their writes.

Adapters opt into addition/removal with `AllowAddRemove`; adding a property adapter
does not automatically enable these operations. The currently enabled lifecycles
require only Transform. Evaluate dependency validation, pool accounting, and both
creation and destruction side effects before opting in another type.

`Duplication/` owns the explicit copy capability list and component/entity state
that must survive duplication. Adding an adapter does not enable duplication.
Evaluate its complete authored state, references and lifecycle before extending
`Duplication::Supports`, `Read`, `Validate` and `ApplyComponent`. Batch preparation
tracks ordered child lists and proposed component state to prepare each hierarchy
snapshot without mutating live entities.

## Observing an edit

Successful edits and execution failures that may have changed state include an
`observationToken`. Send it as `after` to `POST /observe` to obtain a subsequent
rendered view, entity state, and incremental logs. See
[rendered observations](debug-server-observation.md) for timing and failure rules.

## Verification

For undo/redo and explicit saving, see the
[history and persistence verification recipe](debug-server-history.md#verification).

For writable-state readback, use the disposable-project launch recipe in
[rendered observations](debug-server-observation.md#verification) with port 19031
and the Level viewport open. Run:

```sh
python3 Editor/src/DebugServer/Verification/verify-readback.py \
  --url http://127.0.0.1:19031 --output /tmp/pine-readback-results
```

The recipe checks adapter defaults, local transforms beneath a parent, entity
flags, every light type, material/model path-to-ID conversion and null clearing,
unchanged read → edit → read round trips, protected editor entities, observation
readback after intervening edits, component replacement and deleted targets. It
saves representative readback responses and removes its authored hierarchy.
Close the disposable Editor afterwards.

Verified locally: Editor configure/build; readback, schema and observation HTTP
recipes in a disposable project on a virtual display with null audio; and a
temporary native probe for unsupported scene components, disabled components,
protected descendants, invalid native values without repair, and playing/paused
readback. The temporary Editor and display processes were stopped afterwards.

For schema discovery, use the disposable-project launch recipe in
[rendered observations](debug-server-observation.md#verification) with port 19030.
The Level viewport does not need to be selected for this recipe. Run:

```sh
python3 Editor/src/DebugServer/Verification/verify-schema.py \
  --url http://127.0.0.1:19030 --output /tmp/pine-schema-results
```

It saves the schema response, builds requests from advertised descriptors, and
executes all eight operations. It checks required/unknown fields for every operation,
batch envelopes and limits, nested component entries, target ID kinds, parent
omission/null, earlier creation/duplication refs, rejected forward/previous-request
refs, adapter defaults and asset reference forms. Rejected batches are checked for
unchanged scene state. Close the disposable Editor after running the recipe.

Verified locally: Editor configure/build, schema discovery recipe and existing
component lifecycle recipe against a disposable project on a virtual display.
Model-present and model-removed captures were also inspected. The temporary
Editor and display were stopped after verification.

Build the Editor and use the disposable-project launch recipe in
[rendered observations](debug-server-observation.md#verification), with port 19025
and the Level viewport open. From the repository root, run:

```sh
python3 Editor/src/DebugServer/Verification/verify-components.py \
  --url http://127.0.0.1:19025 --output /tmp/pine-component-results
```

The recipe checks default and configured additions, Transform protection,
unsupported types, duplicate types, invalid properties/targets, whole-batch
validation without mutation, update/remove/add ordering, stale IDs after slot
reuse, and identified retries of additions and removals. It captures a model with
and without its light, restores the light, then removes the renderer and captures
again using observation barriers. Inspect the PNGs in the output directory.
The recipe leaves entities in the disposable scene; close its Editor afterwards.

For entity renaming and flags, launch a separate disposable project on port 19026
with the Level viewport open and run:

```sh
python3 Editor/src/DebugServer/Verification/verify-entities.py \
  --url http://127.0.0.1:19026 --output /tmp/pine-entity-results
```

This recipe checks partial and repeated patches, Unicode and duplicate names,
strict property/target validation, whole-batch rejection, temporary-entity
protection, identity preservation, and retries after intervening edits. It
captures an active static child beneath an inactive parent, disables and restores
the child, then combines static-flag and Transform edits and captures its new
position. Inspect the PNGs, then close the disposable Editor.

For reparenting, launch a disposable project on port 19027 with the Level viewport
open and run:

```sh
python3 Editor/src/DebugServer/Verification/verify-reparent.py \
  --url http://127.0.0.1:19027 --output /tmp/pine-reparent-results
```

This recipe checks attachment, detachment, same-parent no-ops, unique tree
membership, identity/local-transform preservation, inactive parents, static
descendants, ordered cycle rejection (including newly created parents), parent
refs, invalid envelopes/targets, whole-batch rejection, interleaved Transform
patches and retries after intervening moves. Framed bounds check Pine's additive
position and multiplicative rotation/scale semantics; captures show the hierarchy
before, after reparenting, and after detaching. Inspect the PNGs, then close the
disposable Editor.

Verified locally: Editor build and the reparenting recipe, including visual
inspection of before/reparent/detach captures of a static descendant beneath an
inactive, rotated and nonuniformly scaled parent. World-space bounds matched
Pine's position, rotation and scale composition. The existing entity-property,
component-lifecycle, retry and observation recipes also passed in the disposable
project on a virtual display. Pool exhaustion and injected execution failures
were not forced in this run.

For deletion, launch a disposable project on port 19028 with the Level viewport
open and run:

```sh
python3 Editor/src/DebugServer/Verification/verify-deletion.py \
  --url http://127.0.0.1:19028 --output /tmp/pine-deletion-results
```

The recipe checks leaf and hierarchy deletion, strict target validation, temporary
entity protection, whole-batch rejection, stale component/entity/parent references,
reparent-before-delete ordering, named and unnamed creations within a deleted
hierarchy, component membership, pool slot reuse, and identified retries. It captures
a model before deletion and the resulting empty view using observation barriers.
Earlier observation tokens remain usable for surviving entities. Inspect the PNGs,
then close the disposable Editor.

Also verify native/UI state: select a root, descendant and unrelated entity, delete
the root and confirm only the unrelated selection survives. Repeat during an entity
drag and verify no stale drop occurs. Verify hierarchies containing orthographic cameras
or unsupported components reject deletion, and verify an accepted observation
reports a removed entity if deletion happens before capture. Retain entity and
component handles across deletion and slot reuse; both must resolve to null.
With small pools, fill capacity and exercise delete/create and remove/delete/add
ordering, including a rejected allocation before a later deletion.

Verified locally: Editor build; deletion, reparenting, entity-property,
component-lifecycle, retry and observation HTTP recipes; and visual inspection of
the before/after deletion captures. A temporary native probe linked against the
Editor and Engine verified selection preservation, drag payload cancellation,
camera cleanup across multiple rendering contexts, immediate handle invalidation
and slot reuse, pending-observation invalidation, temporary descendants, and exact
entity/Transform/Light capacity accounting with 16-slot pools. Execution failures
during destruction were not injected.

For duplication, launch a disposable project on port 19029 with the Level viewport
open and run:

```sh
python3 Editor/src/DebugServer/Verification/verify-duplication.py \
  --url http://127.0.0.1:19029 --output /tmp/pine-duplication-results
```

The recipe checks hierarchy/child order, entity and component ID mappings, shared
model references, independent edits, strict envelopes, whole-batch rejection,
ordered property and component changes, new named/unnamed descendants, reparenting,
earlier duplicates within a copied hierarchy, deletion/ref invalidation, and
identified retries after edits and deletion. It captures the original model and
its moved duplicate using observation barriers. Inspect the PNGs and close the
disposable Editor afterwards.

Also verify native state: entity tags, disabled components, ModelRenderer stencil
settings, fresh rendering caches, selection/handle preservation, protected and
unsupported descendants, and invalid original values rejected before a
history-dependent component change or deletion.
With small pools, verify exact entity/Transform and independent Light capacity,
delete/duplicate ordering, deletion of copied descendants freeing slots, and the
1024-entity preparation limit. These cases need a native fixture because the HTTP
editing API intentionally does not expose all of that state.

Verified before history integration: Editor build; duplication HTTP recipe; component, entity,
reparenting, deletion, retry and observation regression recipes; and visual
inspection of the original and moved-copy captures. A temporary native probe
linked against the Editor and Engine verified tags, disabled components, stencil
settings, shared assets, fresh caches, selection and handles, temporary/unsupported
descendants, repeated component types, invalid source state repaired by a prior
patch, exact entity/Transform/Light capacity with 16-slot pools, copied descendants
freeing capacity on deletion, and the 1024-entity preparation limit. Execution
failures during duplication were not injected.

# Entity placement

`entity.place` in `POST /edit` moves an existing entity against a supplied
world-space plane. Use a point and normal from a fresh
[spatial raycast](debug-server-spatial.md#cast-a-ray) or
[capture pick](debug-server-picking.md), or supply a known plane directly.
This is an authoring operation in stopped mode; it does not run physics.
Alternatively, use [relative bounds placement](#relative-bounds-placement) to align
an entity with another entity and apply an offset. The two request forms are
exclusive; `/edit/schema` advertises their required and forbidden fields in `oneOf`.

## Request

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.place",
      "target": {"id": "<prop-entity-id>"},
      "surface": {
        "point": {"x": 10, "y": 2, "z": 5},
        "normal": {"x": 0, "y": 1, "z": 0}
      },
      "anchor": {"type": "modelBounds"},
      "clearance": 0.02
    }
  ]
}
```

- `target` is an existing scene entity ID. Temporary editor entities and their
  descendants are excluded. As with other entity targets, batch refs are not
  accepted; create first, then place using the returned ID.
- `surface.point` is a point on the target plane in world units.
- `surface.normal` points **out of the surface toward the prop**. Its magnitude
  must exceed `1e-12`; the server normalizes it. Raycast normals oppose the ray,
  so cast from the side on which the prop should sit.
- `anchor` is required and selects one of the two policies below.
- `clearance` is optional, defaults to zero, and must be nonnegative. It is a
  distance in world units **along the normalized surface normal**.
- `alignment` is optional. Omit it to preserve rotation. Supply it to set both
  the normal-facing axis and the roll, as described below.

Vectors are strict `{x,y,z}` objects. Numeric inputs must be finite with absolute
value at most `1e12`. Clearance is limited to `[0, 1e12]`. The same coordinate and
scale limit applies to the world transforms used by placement and its resulting
position. Ancestor rotations must be unit quaternions (length tolerance `1e-5`);
repair invalid native state through the Transform adapter first. The engine stores transforms as float32; allow an appropriate tolerance
when checking clearance, especially far from the origin.

Unknown fields, invalid axes, missing geometry and invalid transforms reject the
whole batch before mutation. The regular edit body/depth/operation limits,
[retry rules](debug-server-requests.md), and stopped-mode restriction apply.
`GET /edit/schema` advertises the operation, its fields and related constraints.

## Contact anchors

### Model bounds

`{"type":"modelBounds"}` measures the target's own ModelRenderer geometry. It
uses the model's local bounding box, or the selected mesh's box when `MeshIndex`
is not `-1`. Multiple ModelRenderers contribute to a combined local box. The
calculation uses the proposed rotation and scale, including parent transforms,
negative scale and zero scale. It does not use cached renderer bounds.

Placement centers that transformed box over `surface.point` in the plane's two
tangential directions, then puts its **minimum projection along the normal** at
`clearance`. This accounts for an off-center asset pivot and rotated walls without
inflating the box to a world-axis-aligned box first. It preserves local scale.

Coverage is deliberately limited to the target's own model bounds, regardless of
entity/component active flags. Descendants, terrain, colliders, sprites and shader
deformation do not contribute. A group with geometry only on children requires
an explicit local anchor or placement of its individual models. Unassigned models
and models without meshes do not supply a bound.

This guarantees bounding-box clearance from the **supplied infinite plane**,
within float precision. It does not establish mesh contact, support, or clearance
from other scene surfaces. A box can enclose empty space, so the visible mesh may
sit farther from the plane.

In particular, a raycast on uneven terrain provides a point and one face normal.
The tangent plane can pass through another part of the terrain under a wide prop.
Inspect additional ray samples and the resulting capture before accepting that
placement; this operation does not settle an object onto an uneven footprint or
perform collision avoidance. A finite wall's edges and openings are not tested.

### Explicit local point

```json
"anchor": {
  "type": "localPoint",
  "point": {"x": 0, "y": 0, "z": -0.25}
}
```

The point is in the entity's model-local coordinates **before scale**. Placement
transforms it with the resulting world rotation and scale, then translates the
entity so that the anchor equals `surface.point + normal * clearance`.
`{x:0,y:0,z:0}` selects the pivot. This policy works without model geometry and
makes no clearance claim about the rest of the entity.

`point` is required for `localPoint` and forbidden for `modelBounds`.

## Alignment and wall mounting

For a lamp whose local −Z should face away from the wall and whose +Y should
remain upright, add:

```json
"alignment": {
  "axis": "-Z",
  "upAxis": "+Y",
  "up": {"x": 0, "y": 1, "z": 0}
}
```

`axis` and `upAxis` each accept `+X`, `-X`, `+Y`, `-Y`, `+Z`, or `-Z`, and must
be perpendicular. `axis` is rotated onto the surface normal. `upAxis` is rotated
onto the projection of the normalized world-space `up` vector onto the surface
plane. `up` must have magnitude greater than `1e-12`; its projection must have
length greater than `1e-6`. Parallel or nearly parallel directions are rejected
rather than choosing an arbitrary roll.

Alignment axes are rotation axes, independent of scale and mirroring, matching
`/spatial/query` orientation vectors. Contact bounds and local anchors still use
the signed world scale. To align a crate's +Y with the ground normal, use `+Y`
for `axis` and, for example, `+Z` for `upAxis` with a world +Z `up` direction that
is not parallel to the normal.

## Batches, parents, history and readback

Placement is calculated during whole-batch validation using the state after
preceding operations. Parent creation/reparenting, Transform updates and model
assignment/removal earlier in the batch are reflected in the calculation. A
later partial Transform update retains fields omitted from that patch, including
the placement's position/rotation. Later duplication copies the placed state.

Parent conversion follows Pine's actual semantics: positions add independently
of parent rotation/scale, rotations compose as `parent * local`, and scales
multiply. Placement subtracts the parent world position and converts the chosen
world rotation to a local rotation. Children keep their local transforms and
move according to those same engine rules; their geometry is not a placement
constraint. A later operation moving a parent can therefore move a placed child
away from the plane.

The surface is a literal point/normal, not a retained geometry reference. Moving
the wall after the raycast, including earlier in the same edit batch, does not
update that plane. Re-query when the geometry on which placement depends changes.
A capture pick can intentionally describe an older captured scene.

Each result contains `operation` and `entity`, including the Transform properties
in the regular writable representation. `/spatial/query` reports fresh world and
local transforms, bounds and axes for checking the result. The edit response
includes an observation token; pass it to `/observe` for a rendered capture
without arbitrary sleeps.

A successful batch is one undo step. Undo/redo restore the calculated local
transforms through the existing adapter and dirty affected descendants. Redo
restores the recorded placement; it does not resample a changed surface. Saving
and reloading a Level preserves these transforms through the existing persistence
workflow. Placement does not save implicitly.

## Relative bounds placement

To place a prop beside another with a 0.1-world-unit gap along +X, align the target's
minimum X with the reference's maximum X, then add an offset. This example also
aligns their bottom bounds and centers them along Z:

```json
{
  "version": 1,
  "operations": [
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
  ]
}
```

- `relativeTo` requires an existing scene entity ID, like `target`. Batch refs,
  temporary editor entities and deleted references are rejected. The reference
  cannot be the target or its descendant, since moving the target would move that
  reference too. An ancestor reference is allowed.
- `boundsAlignment` requires at least one of `x`, `y`, `z`. Each selected axis
  requires `target` and `reference`, each one of `min`, `center`, `max`. These are
  coordinates of **world-axis-aligned model bounds**, regardless of either entity's
  rotation. Rotation and scale are preserved. Unselected position coordinates are
  preserved before applying the offset.
- `offset` is optional and defaults to zero. When supplied, both `space` and
  `value` are required. `world` adds the vector directly; `referenceLocal` rotates
  it by the reference's world rotation. Both use world-unit distances. Reference
  scale and mirroring do not scale or reverse the offset. The offset applies on
  **all** axes, including axes omitted from `boundsAlignment`.
- The relative form forbids `surface`, `anchor`, `clearance` and `alignment`.
  The surface form forbids `relativeTo`, `boundsAlignment` and `offset`.

For example, to center a prop over a shelf and place its bottom on the shelf's
top, use center/center on X and Z, and `{"target":"min","reference":"max"}`
on Y. Leave out `offset` for zero gap. A rotated shelf's world bounding box may
include empty space; use surface-plane placement for contact with a tilted face.

Both entities require their own assigned ModelRenderer geometry. Each renderer's
local box (selected by `MeshIndex`) is transformed before combining into world
bounds, matching `/spatial/query` for model-only entities with `includeChildren:
false`. Off-center pivots, rotation, parent transforms, negative and zero scale
are included. Entity/component active flags do not affect these bounds. Descendants,
terrain, colliders and shader deformation do not contribute. This is a bounds
alignment operation; it makes no mesh contact or collision-avoidance guarantee.

The reference's geometry, position, rotation and ancestors are resolved from the
state after **preceding operations in the batch**, including earlier placements,
model changes and reparenting. No ongoing attachment is created. A later operation
moving either entity or its ancestors may change the final relationship. Redo
restores the recorded target transform without resampling the reference.

The same finite input limits, parent conversion, strict whole-batch validation,
history, retries, readback, observation tokens and explicit saving apply as for
surface placement. Relative placement also requires calculated world bounds to
remain within ±1e12. Calculations ultimately produce float32 transforms; compare
bounds with a tolerance appropriate to scene scale.

## Aim at a world point

`entity.aim` in `POST /edit` rotates an existing entity so an explicit local axis
points from its world position toward a supplied world point. It preserves local
position and scale and requires no model or light component. For a Pine spotlight
or scene camera, use `forwardAxis: "-Z"` and `upAxis: "+Y"`:

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.aim",
      "target": {"id": "<spotlight-entity-id>"},
      "point": {"x": 10, "y": 2, "z": 5},
      "forwardAxis": "-Z",
      "upAxis": "+Y",
      "up": {"x": 0, "y": 1, "z": 0}
    }
  ]
}
```

All six operation fields are required; unknown fields are rejected. `target`
accepts an existing scene entity ID, excluding temporary editor hierarchies and
batch refs. `point` is a literal world-space position in world units; a fresh
[capture pick](debug-server-picking.md) or [raycast](debug-server-spatial.md#cast-a-ray)
can supply it. The distance from the entity's world position must exceed `1e-12`.
No ongoing tracking relationship is created.

`forwardAxis` and `upAxis` accept `+X`, `-X`, `+Y`, `-Y`, `+Z`, or `-Z`, and must
be perpendicular. The forward axis points at `point`; the up axis follows the
projection of world-space `up` onto the plane perpendicular to that direction.
The server normalizes `up`, whose length must exceed `1e-12`. Its perpendicular
projection must have length greater than `1e-6`; parallel or nearly parallel
inputs reject the batch instead of choosing an arbitrary roll. Axes describe
rotation independently of scale, including negative and zero scale, matching
`/spatial/query` orientation vectors and the renderer's spotlight direction.

Vectors are strict `{x,y,z}` objects with finite numbers in `[-1e12, 1e12]`.
The same bounds apply to composed world positions and scales. Ancestor rotations
must be unit quaternions within `1e-5`. The resulting quaternion is stored as
float32; use a tolerance when checking the world direction.

Aim planning uses the state after preceding batch operations, including changes
to the target, its ancestors, parenting and placement. Pine composes rotation as
`parent * local`; aiming converts the desired world rotation through the inverse
parent rotation. A later partial Transform patch retains the aimed rotation when
omitted, and duplication copies it. Later position or parent rotation changes can
move the entity off target; aim again after those edits if needed.

Aiming does not preserve surface contact after rotation. For a mounted lamp, aim
a separate light child when its housing must retain a particular contact or
orientation, or verify the housing's clearance after aiming.

The operation uses the standard stopped-mode restriction, whole-batch validation,
retry identities and history. One successful batch is one undo step; redo restores
the recorded local rotation without recalculating against a changed point. The
result includes `operation` and `entity` with writable Transform properties, and
the response's observation token supports a subsequent rendered capture. Inspect
world axes with `/spatial/query`. Saving and reloading a Level retains the rotation;
saving remains explicit. Aiming itself works with a hidden viewport.

## Verification

```sh
cmake -S . -B build
cmake --build build --target Editor -j 2
python3 Editor/src/DebugServer/Verification/verify-spatial.py --build build
```

The spatial recipe starts a disposable project with Xvfb and invokes
`verify-placement.py` with a live uneven terrain fixture. It checks an imported
box with an off-center pivot, a lamp on a rotated wall under a mirrored/scaled
parent, selected mesh bounds, zero scale, explicit local anchors, ordered batch
composition, schema validation, unchanged state on rejection, undo/redo, retry
identity, captures after edits and save/reload. Captures are written beneath
`/tmp/pine-spatial-results/placement/` by default.
It also checks relative placement of two off-center props under transformed
parents, every bounds-anchor pair on each axis, retained unselected coordinates,
world/reference-local offsets, invalid/deleted/descendant references, prior batch
edits to both hierarchies, undo/redo, retries and save/reload. The `bounds-aligned`,
`bounds-redone` and `bounds-reloaded` captures show the resulting pair.

For a running disposable project with the Level viewport open, run the placement
recipe alone with `--url http://127.0.0.1:<port>`. Without `--terrain` it uses a
supplied sloped plane for the crate. The recipe imports a temporary OBJ and writes
a verification Level; it should not be run against a user's working project.

The same harness runs `verify-aim.py`, checking all perpendicular signed axis pairs,
rotated/mirrored/scaled parents, zero scale, ordered edits and reparenting, duplicate
state, strict rejection without mutation, history, retry identities and save/reload.
It picks a rotated wall from a capture and aims a parented spotlight at that point;
`aim/aimed`, `aim/undone`, `aim/redone` and `aim/reloaded` captures show the lighting.
Native checks cover hidden viewports, invalid ancestor state and play/pause rejection.
Run `verify-aim.py --url http://127.0.0.1:<port>` alone only against a disposable
project; it creates a fixture and saves/reloads a verification Level.

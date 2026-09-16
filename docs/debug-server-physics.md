# 3D physics authoring

The debug server supports `Collider` (Pine's 3D collider component) and `RigidBody`
through the existing `/edit` operations. Both support creation, addition, updates,
removal, writable-state readback, duplication, deletion, undo/redo and level saving.
Discover their properties and defaults in `/edit/schema`.

## Lifecycle

Edits require **Stopped** play mode. They change the authored component properties;
the first physics step after Play creates actors and shapes from those properties.
Stop destroys the simulation's actors and restores the pre-play scene. Edit again
and the next Play creates fresh actors. There is no live physics-shape editing or
new play-control endpoint in this feature.

- A Collider without a RigidBody creates a static actor.
- A RigidBody uses the Collider on the **same entity**. Without a Collider it stores
  configuration but creates no actor. Adding either component does not implicitly
  add the other, and removing either does not cascade. Either addition order works.
- An entity's `static: true` flag forces a static actor, regardless of RigidBody Type.
  The configured Type remains unchanged in readback and serialization.
- The API allows one of each component per entity, following the existing addition
  rules. Compound colliders and colliders on child entities attached to a parent's
  body are not supported.
- Readback during play reports component configuration, not an inspection of the
  backend actor. The existing engine setters do not promise runtime synchronization.

## Collider properties

| Property | Meaning |
| --- | --- |
| `Type` | `Box`, `Sphere`, or `Capsule`. Mesh and heightfield editing are unsupported. |
| `Position` | `{x,y,z}` offset added to entity world position, in world units. Pine does not rotate or scale this offset. |
| `Size` | `{x,y,z}` dimensions, multiplied componentwise by entity world scale when the shape is created. |
| `Layer` | Unsigned 32-bit membership bit mask; default `1`. |
| `LayerMask` | Unsigned 32-bit collision mask; default `4294967295`. Both objects must allow the other's layer for contact. |
| `IsTrigger` | Creates a non-solid trigger shape; default `false`. |
| `TriggerMask` | Unsigned 32-bit mask; default `4294967295`. Trigger pairs are enabled when either trigger's mask allows the other's layer. |

All Size coordinates must remain positive when converted to float32, including
unused coordinates. Supplied vectors replace all three coordinates. There are no
separate Radius/Height aliases, so patches and history have one representation.

- **Box:** Size is **half-extents**, not full width/height/depth. `{x:1,y:1,z:1}`
  creates a box two units across at unit scale.
- **Sphere:** Size.x is the radius, scaled by world scale.x. Y and Z are retained
  but do not affect its geometry; nonuniform scale does not create an ellipsoid.
- **Capsule:** Size.x is the radius; Size.y is the cylindrical section's
  **half-height**. Total height at unit scale is `2 * (Size.y + Size.x)`. The capsule
  aligns with entity Y; entity rotation rotates it. Size.z is retained but unused.

Transform editing retains its existing rules, including zero/negative scales. It
does not perform cross-component geometry validation. Use finite, positive scaled
dimensions for physics; invalid geometry fails shape creation on play. The debug
server validates the authored Size independently of the hierarchy's scale.

Triggers configure backend overlap filtering only. This does not add gameplay
trigger callbacks; Physics3D currently installs no simulation event callback.

## RigidBody properties

| Property | Meaning |
| --- | --- |
| `Type` | `Static`, `Kinematic`, or `Dynamic`; default `Dynamic`. |
| `Mass` | Positive finite float32 mass with a finite reciprocal, in kilograms; default `1`. Static actors retain but do not use it. |
| `GravityEnabled` | Boolean; default `true`. Applies to the dynamic actor on creation. |
| `PositionLock` | `{x:bool,y:bool,z:bool}` translation locks on world axes; default all false. |
| `RotationLock` | Same representation for rotation locks on world axes. |
| `MaxLinearVelocity` | Nonnegative world units per second; zero selects the backend creation default. |
| `MaxAngularVelocity` | Nonnegative radians per second; zero selects the backend creation default. |

Axis locks use the schema type `boolean3`: all three boolean fields are required
when supplied, numeric 0/1 and arrays are rejected, and the whole object is replaced.
Mass, gravity, locks and limits are stored even when the configured body type does
not use them. Kinematic bodies follow their entity transform; dynamic bodies are
simulated. For dynamic simulation use scene-root entities: the existing engine
writes the world physics pose back as a local Transform, so transformed parents
are not correctly handled by dynamic-body pose synchronization.

## Example: floor and falling box

```json
{
  "version": 1,
  "operations": [
    {
      "op": "entity.create",
      "name": "Floor",
      "components": [
        {"type": "Transform", "properties": {"LocalPosition": {"x": 0, "y": -0.5, "z": 0}}},
        {"type": "Collider", "properties": {"Size": {"x": 10, "y": 0.5, "z": 10}}}
      ]
    },
    {
      "op": "entity.create",
      "name": "Falling box",
      "components": [
        {"type": "Transform", "properties": {"LocalPosition": {"x": 0, "y": 4, "z": 0}}},
        {"type": "Collider", "properties": {"Size": {"x": 0.5, "y": 0.5, "z": 0.5}}},
        {"type": "RigidBody", "properties": {"Mass": 7}}
      ]
    }
  ]
}
```

Add ModelRenderer components separately for visible geometry. Play through the
Editor to simulate, then Stop before further `/edit` requests.

## Verification

Launch a disposable project with the Level viewport open, then run:

```sh
python3 Editor/src/DebugServer/Verification/verify-physics.py \
  --url http://127.0.0.1:19034 --output /tmp/pine-physics-results
```

The recipe checks schema discovery, defaults, invalid values, ordered patches,
uint32 mask boundaries, observation readback, duplication, deletion, component
removal/replacement, retry identity, history and save/reload. Restart the same
disposable project and repeat with `--reload-only` to check disk persistence.

After building Editor with Ninja, run the native checks (requires Xvfb):

```sh
python3 Editor/src/DebugServer/Verification/verify-physics-native.py --build build
```

This builds a probe using the Editor's boot sequence and linked objects, and runs
it against a disposable project. It inspects actual actor classes, primitive sizes
and orientation, mass, gravity, locks, velocity limits and trigger filtering. A
falling box settles against a floor; after Stop/edit/undo/redo/Play, its replacement
sphere settles at the new radius. It also verifies stopped-mode guards, actor
cleanup, static collision after body removal, and dormant bodies without colliders.

Verified locally: Editor build, both physics recipes (including disk reload after
an Editor restart), and the existing schema, component lifecycle, duplication,
history, readback and request recipes.

Related: [editing](debug-server-editing.md), [history](debug-server-history.md),
[physics subsystem](physics.md).

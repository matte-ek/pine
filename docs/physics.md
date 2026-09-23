# Physics

Pine has separate 2D and 3D physics subsystems, each a `namespace Pine::Physics*`. Only 3D is
implemented; 2D is a stub (see below). Paths relative to `Engine/src/Pine/`.

## Start here
- `Physics/Physics3D/Physics3D.{hpp,cpp}` — 3D physics, backed by **PhysX 5**.
- `Physics/Physics2D/` — 2D physics, **a stub**: no backend is linked and nothing simulates.
- `Physics/Physics3D/TerrainCollision/` — cooks a `Terrain` into a heightfield collision shape.

## How it fits together
- Both subsystems follow the namespace-subsystem pattern: `Setup()`, `Update(deltaTime)`, `Shutdown()`. `World::Update()` calls `Physics3D::Update()` and `Physics2D::Update()` each frame (see [world-ecs.md](world-ecs.md)).
- The component ↔ physics coupling runs through the component virtuals `OnPrePhysicsUpdate` / `OnPostPhysicsUpdate`:
  - 3D: `World/Components/RigidBody/`, `World/Components/Collider/`.
  - 2D: `World/Components/RigidBody2D/`, `World/Components/Collider2D/`.
  - `Collider` and `Collider2D` override only `OnPrePhysicsUpdate`; the post hook matters for the rigid bodies.
- `Physics3D::Update` runs, per tick: every `Collider`'s pre hook, then every `RigidBody`'s, then `CharacterController::Simulate`, then `simulate`/`fetchResults`, then the colliders' post hooks, then the rigid bodies'.
- **PhysX** is the 3D backend. Its headers/libs live in `third-party/physx/` and are **not committed** (see root `README.md` / `setup-env.sh`); the CMake `physx` target links the static libs. PhysX types are **not** confined to `Physics/Physics3D/`: `Collider.hpp`, `RigidBody.hpp` and `CharacterController.hpp` include PhysX and expose `physx::` types in their public API (e.g. `RigidBody::GetRigidBody()`, `RigidBody::ApplyForce(..., physx::PxForceMode::Enum)`, `Collider::CreateCollisionShape()`), `Physics3D.hpp` returns `physx::` pointers, and the script interfaces (`Script/Interfaces/ScriptInterfacePhysics.cpp`, `ScriptInterfaceComponent.cpp`) use them directly. Prefer engine-level types in new public API rather than adding to that.

## Notes
- **2D physics is a stub.** `Physics2D::Setup`/`Physics2D::Shutdown` are empty, the `b2World` behind `Physics2D::GetWorld()` is always `nullptr`, and Box2D is only forward-declared — it is not linked. `Physics2D::Update` still runs the `Collider2D`/`RigidBody2D` hooks on its own 1/120 s accumulator, but nothing is stepped and `RigidBody2D::OnPostPhysicsUpdate` writes nothing back, so 2D bodies do not move.
- Component properties are **applied when the PhysX actor is created**, not when the setter runs — `RigidBody::CreateActor()` reads mass, gravity, locks and limits once, and mass goes through `PxRigidBodyExt::setMassAndUpdateInertia` so the inertia tensor follows the collider's shape. Changing a property on a live actor means recreating it.

## Character controller

`World/Components/CharacterController/` is a kinematic capsule walker on PhysX's `PxController`,
outside the actor/`RigidBody` world above: it does not simulate, it sweeps. `Physics3D::Update`
calls its `Simulate()` once per tick, *before* `m_Scene->simulate()`, and the controller writes the
entity's Transform itself — so a Transform-only move is undone on the next tick and `SetPosition()`
is the way to teleport one.

**The split with gameplay is the thing to understand.** The component owns the *vertical* axis and
the collision response; the script owns the *horizontal* walk.

- `Move()` takes a **displacement**, not a velocity, and only accumulates it — the tick consumes and
  clears it. How that displacement is arrived at (acceleration, sprint, air control) is per-character
  policy and lives in the game's script, not in engine config. `data/projects/gm/assets/PlayerController.cs`
  is the worked example.
- Gravity, landing and ceiling contacts accumulate into `m_VerticalVelocity`, which the component
  clears on both. A script jumps by assigning `SetVerticalVelocity()`; the verb ("jump", "double
  jump", "cut the jump short") stays in the script, the integration stays here.
- `GetVelocity()` reports the velocity the tick **actually achieved**, measured from the foot
  position either side of the move, and `IsTouchingSides()` says whether something stopped it.
  A script that integrates its own velocity needs both: collide-and-slide means a blocked tick
  travels less than it was asked to, and keeping the difference banks speed against the wall that
  is released the moment the player turns away from it.

⚠ The tick is **not** the frame. `Physics3D::Update` gates on a 1/120 s accumulator and then steps
by whatever has accumulated, while scripts run every frame, after physics. Queued displacements sum
correctly across frames, but anything read back from the controller is from the last tick, which
above 120 fps is not this frame.

### Verification

```sh
python3 Editor/src/DebugServer/Verification/verify-character-controller.py --build cmake-build-debug-agent
```

A native probe (the `verify-physics-native.py` pattern) because `/edit` has no CharacterController
operation. It walks the controller over open ground and into a wall and requires the reported
velocity to match what was travelled rather than what was asked for, requires a teleport to leave no
velocity behind, jumps from an assigned vertical velocity and checks the apex against the one
gravity implies, then repeats that jump under a ceiling and requires both the apex and the airtime
to come down — without the ceiling clamp the controller hangs there spending speed it cannot use.


## Terrain collision

A terrain gets collision through an ordinary `Collider` of `ColliderType::HeightField`, which sources
its geometry from the sibling `TerrainRendererComponent`'s terrain. That is not a terrain special case:
`ColliderType::ConvexMesh` and `ConcaveMesh` will read their mesh off the sibling `ModelRenderer` in
exactly the same way. The alternative — the terrain component creating its own actor — would hide
collision inside a *renderer*, which is the last place anyone would look for it.

`TerrainCollision::CreateShape` cooks **one `PxHeightField` for the whole terrain**, not one per
chunk. Chunks exist for rendering LOD and frustum culling; collision needs neither, and PhysX runs
its own broadphase over a height field. Keeping it whole is what lets a terrain attach as a single
shape, so neither `Collider` nor `RigidBody` has to learn about multi-shape actors. The returned
shape holds the only reference to the cooked field, so releasing the shape frees it — there is no
second handle to keep or to forget.

Three things have to agree with the renderer, and all three are derived rather than restated:

- **Row and column scale** are `Terrain::GetSampleSpacing()`, the same number the mesh generator
  spaces its vertices by.
- **Height scale** comes out of `Terrain::DecodeHeight` itself, so the physics surface uses the
  asset's own encoding rather than a second copy of it. PhysX samples are `PxI16` and the terrain's
  are `uint16`, which differ by exactly half the range — a re-centring that loses nothing and whose
  constant is folded into the shape's local pose.
- **The quad diagonal.** PhysX's cleared tessellation flag splits a quad between its two *other*
  corners, which is the rule `Terrain::IsInFirstQuadTriangle` documents and `GetHeightAt`
  interpolates against. The previous implementation had three expressions of this and they
  disagreed, which is why `verify-terrain-physics.py` asserts it directly.

⚠ **PhysX's rows run along local X and its columns along local Z, so the sample copy transposes** —
the terrain stores its field row-major in Z. Transposing beats rotating the shape: a rotated height
field makes every later question about the collision only answerable after applying that rotation.

⚠ **Rotation is ignored** for a height field collider on its own (`Collider::UpdateBody`). A height field has
one surface per column by construction, so a rotated terrain is not something the asset or the
renderer can represent — `TerrainRenderer` places chunks by position alone. Rotating only the
collision would make the ground you walk on disagree with the ground you see. This only holds when
the entity has no `RigidBody`: then the shape sits on `RigidBody`'s actor, and `RigidBody::UpdateBody`
applies the transform's rotation whatever the collider type, so keep terrain entities unrotated or
without a `RigidBody`.

Cooking is fast (a 257² field is a millisecond) but far too slow per frame, so collision is built
once when the actor is created. **Rebuild with
`Physics3D::TerrainCollision::RebuildColliders(terrain)`** once an edit is complete: it calls
`Collider::Reset()` on every height field collider whose entity renders that terrain, which drops
the actor carrying the shape - the collider's own, or the sibling `RigidBody`'s through
`RigidBody::Reset()` - and the next physics update cooks a new one.

The editor calls it at the end of a sculpt stroke (not on each step), when a height stroke is undone
or redone, and after the terrain properties panel's **Apply Layout** and **Generate From Noise**.
Painting layer weights does not touch collision. Anything else that edits a terrain's heights has to
call it too, or the ground you walk on stops matching the ground you see.

### Verification

```sh
python3 Editor/src/DebugServer/Verification/verify-terrain-physics.py --build cmake-build-debug-agent
```

Builds a probe out of the Editor's boot sequence (the `verify-physics-native.py` pattern) because
`/edit` has no `TerrainRenderer` operation. It casts a grid of rays down at the terrain and requires
every hit to match `Terrain::GetHeightAt` — a sweep rather than a single point, because a scale error
that happens to vanish at the origin does not survive one. It repeats that under an entity offset, a
negative chunk origin and a rotated entity, checks the quad diagonal against a hand-built wedge where
the two possible diagonals differ by the whole raised height, drops a body and requires it to come to
rest on the ground *where it landed*, and checks that a height field collider with no terrain
produces no actor at all. Last, one terrain under two entities - a bare collider and one with a
static `RigidBody` - is edited: the collision must keep the old ground until
`TerrainCollision::RebuildColliders` runs and then match the new one on both, and a sculpt stroke
must leave the collision alone mid-stroke and rebuild it once the stroke ends.

Related: [world-ecs.md](world-ecs.md)

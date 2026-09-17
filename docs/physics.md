# Physics

Pine has separate 2D and 3D physics subsystems, each a `namespace Pine::Physics*`. Paths
relative to `Engine/src/Pine/`.

## Start here
- `Physics/Physics3D/Physics3D.{hpp,cpp}` — 3D physics, backed by **PhysX 5**.
- `Physics/Physics2D/` — 2D physics.
- `Physics/Physics3D/TerrainCollision/` — cooks a `Terrain` into a heightfield collision shape.

## How it fits together
- Both subsystems follow the namespace-subsystem pattern: `Setup()`, `Update(deltaTime)`, `Shutdown()`. `World::Update()` calls `Physics3D::Update()` and `Physics2D::Update()` each frame (see [world-ecs.md](world-ecs.md)).
- The component ↔ physics coupling runs through the component virtuals `OnPrePhysicsUpdate` / `OnPostPhysicsUpdate` on the relevant components:
  - 3D: `World/Components/RigidBody/`, `World/Components/Collider/`.
  - 2D: `World/Components/RigidBody2D/`, `World/Components/Collider2D/`.
- **PhysX** is the 3D backend. Its headers/libs live in `third-party/physx/` and are **not committed** (see root `README.md` / `setup-env.sh`); the CMake `physx` target links the static libs. PhysX types (`Px*`) appear only inside `Physics/Physics3D/` — keep them there and expose engine-level types outward.

## Notes
- 3D and 2D are independent worlds; a given entity uses one or the other via its component set.
- Component properties are **applied when the PhysX actor is created**, not when the setter runs — `RigidBody::CreateActor()` reads mass, gravity, locks and limits once, and mass goes through `PxRigidBodyExt::setMassAndUpdateInertia` so the inertia tensor follows the collider's shape. Changing a property on a live actor means recreating it.

## Terrain collision

A terrain gets collision through an ordinary `Collider` of `ColliderType::HeightField`, which sources
its geometry from the sibling `TerrainRenderer`'s terrain. That is not a terrain special case:
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

⚠ **Rotation is ignored** for a height field collider (`Collider::UpdateBody`). A height field has
one surface per column by construction, so a rotated terrain is not something the asset or the
renderer can represent — `TerrainRenderer` places chunks by position alone. Rotating only the
collision would make the ground you walk on disagree with the ground you see.

Cooking is fast (a 257² field is a millisecond) but far too slow per frame, so collision is built
once when the actor is created. **Rebuild by calling `Collider::Reset()`**, which drops the actor and
lets the next physics update cook a new one — that is what a sculpting stroke will do on mouse-up.

### Verification

```sh
python3 Editor/src/DebugServer/Verification/verify-terrain-physics.py --build build
```

Builds a probe out of the Editor's boot sequence (the `verify-physics-native.py` pattern) because
`/edit` has no `TerrainRenderer` operation. It casts a grid of rays down at the terrain and requires
every hit to match `Terrain::GetHeightAt` — a sweep rather than a single point, because a scale error
that happens to vanish at the origin does not survive one. It repeats that under an entity offset, a
negative chunk origin and a rotated entity, checks the quad diagonal against a hand-built wedge where
the two possible diagonals differ by the whole raised height, drops a body and requires it to come to
rest on the ground *where it landed*, and checks that a height field collider with no terrain
produces no actor at all.

Related: [world-ecs.md](world-ecs.md)

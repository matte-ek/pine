# Physics

Pine has separate 2D and 3D physics subsystems, each a `namespace Pine::Physics*`. Paths
relative to `Engine/src/Pine/`.

## Start here
- `Physics/Physics3D/Physics3D.{hpp,cpp}` — 3D physics, backed by **PhysX 5**.
- `Physics/Physics2D/` — 2D physics.
- `Physics/Physics3D/PhysicsTerrain/` — heightfield/terrain collision.

## How it fits together
- Both subsystems follow the namespace-subsystem pattern: `Setup()`, `Update(deltaTime)`, `Shutdown()`. `World::Update()` calls `Physics3D::Update()` and `Physics2D::Update()` each frame (see [world-ecs.md](world-ecs.md)).
- The component ↔ physics coupling runs through the component virtuals `OnPrePhysicsUpdate` / `OnPostPhysicsUpdate` on the relevant components:
  - 3D: `World/Components/RigidBody/`, `World/Components/Collider/`.
  - 2D: `World/Components/RigidBody2D/`, `World/Components/Collider2D/`.
- **PhysX** is the 3D backend. Its headers/libs live in `third-party/physx/` and are **not committed** (see root `README.md` / `setup-env.sh`); the CMake `physx` target links the static libs. PhysX types (`Px*`) appear only inside `Physics/Physics3D/` — keep them there and expose engine-level types outward.

## Notes
- 3D and 2D are independent worlds; a given entity uses one or the other via its component set.
- Component properties are **applied when the PhysX actor is created**, not when the setter runs — `RigidBody::CreateActor()` reads mass, gravity, locks and limits once, and mass goes through `PxRigidBodyExt::setMassAndUpdateInertia` so the inertia tensor follows the collider's shape. Changing a property on a live actor means recreating it.
- Terrain colliders are driven from the `Terrain` asset + `TerrainRenderer` component through `PhysicsTerrain`.

Related: [world-ecs.md](world-ecs.md)

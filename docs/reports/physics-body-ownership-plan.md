# Implementation plan — physics body ownership

Written against `ai` @ f553804 plus the small fix described below. The question this answers is
*who owns the PhysX actor*, which is currently nobody, and which is why a floor could be marked
Static and silently cease to exist.

---

## What already landed (the small fix)

`RigidBody` now holds a `PxRigidActor*` rather than a `PxRigidDynamic*`:

- `RigidBodyType::Static` creates a real `PxRigidStatic`. It used to create a gravity-enabled
  dynamic actor and teleport it back into place every tick with `setGlobalPose()` — the velocity
  was never cleared, so it was accumulating downward speed forever behind the teleport.
- The entity `Static` flag no longer skips actor *creation*, only the per-tick pose sync. It now
  means what it reads as: "this transform will not change at runtime."
- Type changes between static and dynamic rebuild the actor, since they're different PhysX classes.
- `OnCopied()` resets the actor pointer. It didn't, so a duplicated entity shared its actor with
  the original and both would release it.
- `CreateActor` null-checks the shape. `Collider::CreateCollisionShape()` returns null for
  `ConvexMesh` and `ConcaveMesh` (they fall through the switch), and the old code dereferenced it.

That makes the workflow correct. It does not make the ownership model correct, and the rest of this
document is about that.

---

## What's still wrong

**Nobody owns the actor.** `Collider::UpdateBody()` asks "do I have a sibling `RigidBody`?" and
abdicates if so; `RigidBody::UpdateBody()` asks "do I have a sibling `Collider`?" and takes over if
so. Neither is wrong on its own. Together they had a hole, and the hole was silent precisely
*because* each component had correctly concluded the other one would handle it. That is the failure
mode worth designing out — not the specific hole, which is now patched.

Three consequences that are still live:

**1. One collider per entity, permanently.** `RigidBody::UpdateBody()` attaches exactly one shape
from exactly one `GetComponent<Collider>()`. Compound shapes — a table, a character with separate
hitboxes, a room built from several boxes — are not expressible, and there is no seam to add them
at: the shape list isn't a list.

**2. Colliders can't be edited at runtime.** `Collider::SetSize()` mutates `m_Size` and nothing
else; the shape was baked at actor creation. `SetColliderType()` works around this by destroying
the actor, but only on the branch where the collider owns it (`if (m_Standalone ||
!m_CollisionRigidBody) return;`) — so changing collider type on an entity that has a `RigidBody`
does nothing at all. Both of these are the same missing concept: nothing knows the actor is stale.

**3. The sibling handshake runs in the physics hot loop.** Both components call
`GetComponent<T>()` every fixed tick, and that is a linear walk over the entity's component vector
doing `typeid(*component) == typeid(T)` followed by a `dynamic_cast`. RTTI, twice per entity, at
120 Hz. It buys nothing — the answer only changes when a component is added or removed.

The same handshake was copy-pasted into `Collider2D`/`RigidBody2D`, so the pattern has already
propagated once.

---

## The shape

Introduce a `Body` in `Physics3D`: the thing that exists in the physics scene. One per entity,
owned by `Physics3D`, not by any component.

```
Physics3D::Body
    PxRigidActor*  actor
    Kind           kind        // Static | Kinematic | Dynamic
    ShapeDesc[]    shapes      // geometry + local pose + filter data + trigger flag
    bool           dirty
```

The components stop owning anything and become declarations of intent:

- **`Collider` describes a shape.** Geometry, local offset, filter data, trigger flag. It creates
  no actor and asks about no siblings. Every setter marks the body dirty.
- **`RigidBody` describes dynamics.** Kind, mass, gravity, locks, velocity limits. Same: no actor,
  no sibling lookup.
- **`Physics3D` decides and builds.** One place resolves the kind, and it is the only place that
  can produce a contradiction, so it is the only place that has to log one.

Store bodies as a flat array indexed by entity internal id, mirroring `ComponentDataBlock`.
`m_MaxObjectCount` already caps entities, so this is allocation-free in the hot path and matches
the existing pool idiom rather than introducing a map.

Kind resolution, in full, in one function:

```
kind = rigidBody ? rigidBody->GetKind() : Kind::Static      // no rigid body means static geometry
if (entity->GetStatic())
{
    if (kind == Kind::Dynamic) warn once — a Dynamic body on a Static entity is a contradiction
    kind = Kind::Static
}
```

That is the whole of what the two components were negotiating between them, and it fits on screen.

**Why this shape and not a smaller one:** the alternative is to keep ownership in `RigidBody` and
formalise `Collider` as a shape provider. That is today's structure with better names — it still
can't hold a shape *list*, and it still needs the sibling lookup to decide who builds. The body has
to be a thing before "several colliders feed one actor" is expressible at all.

**Serialization is untouched.** `RigidBodyType` keeps its values and its meaning, `ColliderSerializer`
is unchanged, no `.passet` or level migration. The reshape is entirely internal to the physics
subsystem, which is what makes it safe to do in one pass.

---

## Steps

Each step builds and is separately judgeable in the editor.

**1. `ShapeDesc` + `Collider` stops owning an actor.**
Extract the body of `CreateCollisionShape()` into a `ShapeDesc` the collider produces, and a
`Physics3D` function that turns a `ShapeDesc` into a `PxShape`. `Collider` keeps creating its own
`PxRigidStatic` for now — this step is a pure extraction with no behaviour change. *Check: static
geometry still collides.*

**2. `Physics3D::Body` + the body pool.**
Add the pool, the kind resolution, and actor build/rebuild from `ShapeDesc`s. Route `RigidBody`
through it: `RigidBody` keeps its public API (`ApplyForce`, `GetRigidBody`) but forwards to the
body. `Collider` still owns the no-rigidbody case. *Check: dynamic bodies fall and collide as
before.*

**3. `Collider` hands its static case to the pool.**
Delete `Collider::UpdateBody()`, `m_CollisionRigidBody`, and the sibling check. Every entity with
at least one `Collider` now gets a body, kind resolved centrally. This is the step that closes the
hole structurally. *Check: a floor with Collider only, Collider + Static RigidBody, and Collider +
Static entity all behave identically.*

**4. Dirty tracking.**
Collider and RigidBody setters mark the body dirty; `Physics3D` rebuilds dirty bodies before
simulating. *Check: resizing a collider in the editor during play takes effect — currently it
silently does nothing.*

**5. Compound shapes.**
`Entity::GetComponents<Collider>()` (needs adding — only `GetComponent` exists) feeds all colliders
into one body. *Check: two box colliders on one entity produce two solid boxes.*

Steps 1–4 are the ownership fix and are worth doing as a unit. Step 5 is the payoff and can wait,
but the point of 1–4 is that it becomes a small step instead of a redesign.

---

## Deliberately not in scope

- **The 2D side.** `Physics2D` never creates a `b2World` and `RigidBody2D::UpdateBody()` has empty
  `if` bodies where the Box2D calls used to be — it is a non-functional stub. Reshaping it to match
  would be speculative work on code that doesn't run. When 2D is revived, this is the shape to
  copy; that's a reason to get 3D right, not a reason to touch 2D now.
- **The dynamic pose round-trip.** `UpdateBody()` writes the transform into a dynamic actor every
  tick (`// scary.`) and `OnPostPhysicsUpdate()` reads it back out, so any external transform write
  wins and bodies can never sleep. The fix is to push the pose only when the transform changed
  externally — but `Entity::IsDirty()` is owned by the renderer (cleared at the end of
  `SceneProcessor::Prepare`) and `Transform::IsDirty()` is cleared by `Transform::OnRender`, so
  physics needs its own dirty source rather than borrowing one with an ordering dependency.
  Separate change, and it wants its own think.
- **`CharacterController` teleporting.** The controller writes the transform and never reads it, so
  script-side position writes are overwritten next tick. Wants a `SetPosition()` calling
  `PxController::setFootPosition()`. Independent of body ownership.

---

## Found while reading — unrelated to the above

**Triggers have never fired.** `Collider::GetFilterData()` puts the trigger mask in `word2` and
hard-zeroes `word3`; `PineFilterShader` tests `word3` against the other shape's `word0`. Since
`word3` is always zero, no pair ever gets `eTRIGGER_DEFAULT`. One-word fix, but worth confirming
which word is intended to be canonical before changing it — and there's no contact/trigger callback
registered on the scene either, so nothing would be delivered to gameplay yet regardless.

**`Collider::CreateCollisionShape()` is missing a `break;`** on the `Capsule` case. It falls
through to `default: break;` so it's harmless today, and will stop being harmless the moment a case
is added after it.

**`ConvexMesh` and `ConcaveMesh` are enum values with no implementation.** They fall through the
switch and return null. Now logged rather than dereferenced, but they're still dead options in the
editor's collider-type dropdown.

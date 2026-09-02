# World / ECS

Pine's entity-component system. It is **custom and pool-based** (not EnTT/flecs). Paths
relative to `Engine/src/Pine/`.

## Start here
- `World/Components/Component/Component.hpp` — the `Component` base class + the `ComponentType` enum.
- `World/Components/Components.{hpp,cpp}` — component storage (`ComponentDataBlock<T>`) and registration in `Components::Setup()`.
- `World/Entity/Entity.hpp` — the `Entity` type.
- `World/Entities/Entities.hpp` — entity lifetime (`Create`/`Find`/`Delete`/`GetList`).
- `World/World.{hpp,cpp}` — per-frame `World::Update()` and the active `Level`.

## How it fits together
- Each component type gets a **`ComponentDataBlock<T>`**: one contiguous array of `m_MaxObjectCount` instances plus a parallel occupation array, iterated by a custom iterator that skips empty/disabled slots. New components take the first free slot, initialized by copying a default-constructed prototype. Overflow throws.
- **`Component`** defines the virtual lifecycle everything relies on: `OnCreated/OnDestroyed/OnCopied/OnSetup/OnUpdate/OnRender/OnPre|PostPhysicsUpdate` plus `LoadData/SaveData` (serialization). Each folder under `World/Components/` subclasses it.
- **`Entity`** owns a `vector<Component*>`, a parent/child hierarchy, a `UId`, flags, and a paired managed (C#) object. Always has a `Transform`. Use `AddComponent<T>()` / `GetComponent<T>()` / `RemoveComponent<T>()`.
- **"Systems" are not objects.** Behavior lives either in the component virtuals or in subsystem `Update()` functions. `World::Update()` drives physics and (unless paused) script updates; the renderer iterates component blocks directly (see [rendering.md](rendering.md)).
- Access storage via `Components::Get<T>()` (typed block for iteration), `Components::Create<T>()`, `GetType<T>()`, `FindById`, `GetByInternalId`.

## ⚠️ Gotcha: adding a component
The `ComponentType` enum order (`Component/Component.hpp`) **must** match the
`CreateComponentDataBlock<T>()` call order in `Components::Setup()` — they are kept in sync
by hand. Adding a component means all three of:
1. a new `World/Components/<Name>/` folder subclassing `Component`,
2. a new `ComponentType` enum entry, and
3. a matching `CreateComponentDataBlock<T>()` call in the same position.

Related: [assets.md](assets.md) (Blueprint = a serialized entity) · [scripting.md](scripting.md) · [physics.md](physics.md)

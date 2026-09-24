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
- Each component type gets a **`ComponentDataBlock<T>`**: one contiguous array of instances plus a parallel occupation array, iterated by a custom iterator that skips empty/disabled slots. The array holds `m_MaxObjectCount` instances unless `Components::Setup()` passes a smaller count (`TerrainRendererComponent` and `Camera` get 32, `NativeScript` 1). New components take the first free slot, initialized by copying a default-constructed prototype. Overflow throws.
- **`Component`** defines the virtual lifecycle everything relies on: `OnCreated/OnDestroyed/OnCopied/OnSetup/OnUpdate/OnRender/OnPre|PostPhysicsUpdate` plus `LoadData/SaveData` (serialization). Each folder under `World/Components/` subclasses it.
- **`Entity`** owns a `vector<Component*>`, a parent/child hierarchy, a `UId`, flags, and a paired managed (C#) object. Always has a `Transform`. Use `AddComponent<T>()` / `GetComponent<T>()` / `RemoveComponent<T>()`.
- **`Transform` stores local values and caches world ones.** A child sits in its parent's space:
  world position = parent position + parent rotation × (parent scale × local position), while
  rotations compose and scales multiply component by component (no shear, as in Unreal's
  `FTransform`). The world getters and `GetTransformationMatrix()` recompute lazily. Every setter,
  and `Entity::SetParent`, calls `Transform::SetDirty`, which marks the whole subtree.
  `SetPosition/SetRotation/SetScale` take world values and store the matching local ones.
  `verify-transform.py` (in `Editor/src/DebugServer/Verification/`) checks all of this through a
  native probe.
- **"Systems" are not objects.** Behavior lives either in the component virtuals or in subsystem `Update()` functions. `World::Update()` drives physics, then `Audio::Update()`, then (unless paused) script updates; the renderer iterates component blocks directly (see [rendering.md](rendering.md)).
- Access storage via `Components::Get<T>()` (typed block for iteration), `Components::Create<T>()`, `GetType<T>()`, `FindById`, `GetByInternalId`.
- **Pooled components never run a constructor or destructor.** `Components::Create` copies the
  prototype's bytes into the slot with `memcpy`, and `Components::Destroy` only calls
  `OnDestroyed()` and marks the slot free. So a component that owns heap memory (a `std::vector`,
  a `std::string`) must release it in `OnDestroyed()`, or it leaks. Use
  `std::vector<T>().swap(member)` rather than `clear()`, because only the swap gives the capacity
  back. `ScriptComponent::OnDestroyed` shows the pattern. Moving the bytes around is fine: the heap
  pointer travels with them.
- **`SaveData()/LoadData()` cover a component's own fields only** - not the base `Component`
  state such as the active flag. Anything that round-trips a component through them (blueprint
  serialization and copying, the editor's undo/redo commands) has to carry that flag itself.
- The debug server's history (`Editor/src/DebugServer/Editing/History/`) recreates removed
  identities with `Entities::CreateWithId()` (valid unused IDs only), and restores component
  order with `Entity::MoveComponent()` while keeping Transform first. Restored objects can occupy
  different pool slots.

## ⚠️ Gotcha: adding a component
The `ComponentType` enum order (`Component/Component.hpp`) **must** match the
`CreateComponentDataBlock<T>()` call order in `Components::Setup()` and the managed
`ComponentType` enum in `ScriptRuntime/World/Component.cs` — they are kept in sync by hand.
Saved Levels and Blueprints store the type as a raw integer (`Entity::SaveData`), so **append**
new entries; inserting mid-enum breaks every saved scene. Adding a component means:
1. a new `World/Components/<Name>/` folder subclassing `Component`,
2. a new `ComponentType` enum entry at the end, plus its cases in `ComponentTypeToString` and
   `ComponentTypeToHumanString`,
3. a matching `CreateComponentDataBlock<T>()` call at the end of `Components::Setup()`,
4. the same entry at the end of the managed `ComponentType` enum, and
5. a case in the editor's properties switch
   (`Editor/src/Gui/Panels/Properties/EntityPropertiesRenderer/ComponentPropertiesRenderer/`).

Related: [assets.md](assets.md) (Blueprint = a serialized entity) · [scripting.md](scripting.md) · [physics.md](physics.md)

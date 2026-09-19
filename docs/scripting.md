# C# scripting (.NET)

Gameplay scripting in C#, running on CoreCLR, which the engine hosts itself through `hostfxr`.
The C++ side is `Engine/src/Pine/Script/`; the managed side is the top-level `ScriptRuntime/`
project (builds `Pine.dll`, targeting `net10.0`). Paths below are relative to
`Engine/src/Pine/` unless noted.

## Start here
- `Script/ScriptManager.{hpp,cpp}` — the high-level bridge (`Pine::Script::Manager`).
- `Script/Runtime/` — the host (`Pine::Script::Runtime`): starting CoreCLR, loading `Pine.dll`, GC.
- `Script/GameAssembly/` — the game's own assembly and its script classes, in a load context that
  can be replaced (`Pine::Script::GameAssembly`).
- `Script/Factory/` — `ScriptObjectFactory`, which pairs each engine object with a managed one.
- `Script/Bindings/` — the name-to-function table of everything C# may call (`Pine::Script::Bindings`).
- `Script/ManagedCall.hpp` — the plumbing behind every call the engine makes *into* `Pine.dll`.
- `Script/Interfaces/` — the engine functions that fill that table: Log, Input, Entity, Component,
  Asset, Physics.
- `Script/Scripts/ScriptFieldRegistry.hpp` — the engine's half of the field reflection that lives in
  `Pine.dll`. `Script/`-internal, like the rest of the hosting half.
- `ScriptRuntime/` (repo root) — the C# `Pine` API that game code references.

## How it fits together
- **`Script::Runtime`** starts CoreCLR from `data/engine/script/Pine.runtimeconfig.json` and loads `Pine.dll` into the **default** load context. Both happen once, at boot, **very early** (before Assets/World) so other systems can register bindings. A machine with no .NET runtime installed is not an error the engine stops for: it logs, and `Runtime::IsAvailable()` stays false.
- **`Script::GameAssembly`** owns the game's `Game.dll`, in a **collectible** load context of its own so a rebuild can replace it while the editor runs. The engine holds nothing of that assembly except integer ids — a managed type or method it held would keep the old assembly alive and turn every reload into a leak.
- **`Script::Manager`** drives gameplay: `LoadGameAssembly(path)`, `ReloadScripts()` / `ReloadGameAssembly()` (hot reload), and the per-frame `OnStart()` / `OnUpdate(dt)` / `OnRender(dt)` dispatch (called from `World`). It keeps a `ScriptData` per C# class — the class' id in the game assembly, which lifecycle methods it has, and the fields `Pine.dll` reflected for it (`Script/Scripts/ScriptField.hpp`).
- **`Script::ObjectFactory`** gives every `Entity`, `Component` and `Asset` a paired managed object via an embedded `ObjectHandle`, so engine objects and their C# mirrors stay linked. `ObjectHandle` is opaque — a `GCHandle` value plus `IsValid()` — and **nothing native dereferences it**: the engine passes the value back and managed code resolves it, with `Interop.ObjectFrom<T>` (`ScriptRuntime/Core/Interop.cs`).
- **Making those objects is managed code.** `Pine::Script::ObjectFactory` keeps its signatures, but its body is a call into `Pine.Core.ObjectFactory` (`ScriptRuntime/Core/ObjectFactory.cs`), which finds the class, creates the instance, writes the identity the engine addresses it by, and anchors it behind a GC handle. Disposing goes the same way, and is what clears the mirror's valid flag so a script still holding a destroyed entity is told so.
- **`Script::ManagedCall`** is how the engine reaches any of that: it resolves a static `[UnmanagedCallersOnly]` entry point in `Pine.dll` by name, once, and the caller holds it as an ordinary typed function pointer. `ObjectFactory`, `FieldRegistry` and `GameAssembly` all go through it. **Nothing may be thrown out of an entry point** — there is no managed frame above one to unwind into, so an escaping exception kills the process; every one of them catches, logs and answers with a failure value.
- **C# reaches the engine through a name-resolved table.** `Script::Bindings` maps a name to an engine function; the six `Interfaces::*::Setup` tables fill it at boot, and `Pine.Core.Interop.Initialize` asks for each one back by the same name into a `delegate* unmanaged<>` field (`ScriptRuntime/Core/Bindings/`). A binding one side has and the other does not is a named error at startup, not a call that lands somewhere unexpected. Two marshalling rules go with it: a `bool` crosses as a **`byte`**, because a C++ `bool` is one byte and P/Invoke's default is four; and a `string` crosses as UTF-8, `Interop.Utf8Scope` going out and `Bindings::ReturnString` plus `Interop.StringFrom` coming back.
- **Nothing native allocates a managed array.** Every C# API that hands back a set of objects — `Entity.Children`, `Entity.GetComponents<T>`, `EntityList.Find(tag)`, `EntityList.GetAll`, `Physics3D.RayCast` — is a count binding plus an indexed read of one handle, and the managed side builds the array.
- **`Script/Interfaces/`** is where those engine functions are written and named: Log, Input, Entity, Component, Asset, Physics. Each has a matching class on the C# side (`ScriptRuntime/Core`, `/Input`, `/World`, `/Assets`, `/Physics`, `/Math`).

## Two script component kinds
- **`ScriptComponent`** (`World/Components/Script/`) — a managed C# script, backed by a `CSharpScript` asset (`Assets/CSharpScript/`). This is the normal one.
- **`NativeScript`** (`World/Components/NativeScript/`) — a C++ script; currently a stub.

## Script fields

A script's **public** fields are shown in the properties panel and saved with the component. A field
whose type the engine cannot store is not reflected at all - it still works in C#, it just cannot be
authored. What is stored:

| C# type | Stored as |
|---|---|
| `bool`, `int`, `float` | the raw value |
| `Vector2` / `Vector3` / `Vector4` | the raw value |
| `string` | UTF-8 bytes |
| an asset (`Model`, `Material`, ... - anything deriving from `Pine.Assets.Asset`) | the asset's `UId` |

**The reflection itself is managed code.** `ProcessScriptFields` in `ScriptManager.cpp` hands the
script class to `Pine.Core.Reflection.FieldRegistry` (`ScriptRuntime/Core/Reflection/`) and gets back
a descriptor per field; from then on the engine reads and writes those fields by index through
`Script/Scripts/ScriptFieldRegistry.hpp`. It lives there because which fields to reflect is decided
from custom attributes, and only managed code can read those.

Two consequences worth knowing. The registry enumerates **declared** fields and walks the hierarchy
itself, stopping at `Pine.World.Components.Script` - so a script deriving from another script gets
both classes' fields, and neither gets `Component`'s `Parent` and `Type`. And every entry point
catches its own exceptions and answers with a failure value, because there is no managed frame above
it to unwind into.

### Editor attributes

Declared on a field, from `Pine.Core` (`ScriptRuntime/Core/Attributes/`). None of them change how a
value is stored, so a script may gain or lose one without invalidating an authored scene.

| Attribute | Effect |
|---|---|
| `[SerializeField]` | Reflect a private field |
| `[HideInInspector]` | Do not reflect a public field |
| `[Range(min, max)]` | Draw a slider instead of an input field. `float` and `int` only; ignored on any other type |
| `[Tooltip("...")]` | Hover text over the field's row |
| `[Header("...")]` | A labelled separator above the field |
| `[Space]` | Blank space above the field |

```csharp
public class Door : Script
{
    [Header("Movement")]
    [Range(0f, 10f)]
    [Tooltip("Seconds the door takes to swing fully open.")]
    public float OpenDuration = 1.5f;

    [SerializeField] private bool _startsLocked;

    [HideInInspector] public float CurrentAngle;
}
```

**`Entity` fields are reflected but not stored yet.** Resolving one has to wait until a whole level
has loaded (`Level::Load` spawns blueprints in a loop, so the first entity cannot reference the
last), and `Blueprint::Spawn` gives every copy new `UId`s, so a reference inside a blueprint would
have to be remapped to the copy. Use `EntityList.Find(name)` or `Find(tag)` from `OnStart` instead.
The editor shows such a field as "Not supported yet" rather than an empty row.

**Where the values live.** A script field's value is only really a field on the managed object, and
that object is destroyed and rebuilt constantly: on level load, on blueprint spawn, and on every hot
reload, which replaces the assembly its class came from. So `ScriptComponent` keeps its own copy
(`ScriptFieldValue`, matched to fields by name) and moves it across at three points:

- `CreateInstance()` pushes the stored values into each new managed object.
- `SaveData()` pulls the current ones out first, so whatever the properties panel just typed is what
  gets written. This is also what makes the editor's play/stop survive, since `PlayHandler`
  snapshots and restores the world through `SaveData`/`LoadData`.
- `Manager::ReloadGameAssembly()` pulls them before the old assembly goes, which is what carries
  them over a hot reload.

Stored values whose name no longer matches a field are **kept**, not dropped - otherwise a script
that fails to compile, or a level loaded before its game assembly, would silently discard everything
the author set. The debug server has no operation for script fields; they are authored in the
properties panel.

## What makes a reload actually reclaim the old assembly

⚠ **Unloading a collectible load context is cooperative, and silent when it fails.** `Unload()`
only starts the process; the context goes away once nothing refers into it any more and a garbage
collection has run. One missed reference and the call does nothing at all, leaving the old
assembly and its compiled code in memory - and then the next one, and the one after that.

So `Manager::ReloadGameAssembly()` drops all three kinds of reference the engine holds, in order,
before it asks: each `ScriptComponent`'s managed instance, then the `ScriptData` and the field
registry (`DestroyScriptData`), and only then `GameAssembly::Unload()`. Managed-side,
`Pine.Core.GameAssembly` clears its own `Classes` list on the way. **Anything new that caches a
`Type`, a `MethodInfo` or a `FieldInfo` from `Game.dll` has to be cleared at one of those points**,
or hot reload starts leaking an assembly per rebuild.

`Pine.Core.GameAssembly.Unload` watches the context through a `WeakReference` and warns if it is
still alive afterwards, which is the only outward sign that any of this has gone wrong. It collects
*until* the context is gone rather than a fixed number of times: reflection emits an invoke stub
for a method it has been asked for often enough, that stub is freed by a finalizer, and each such
layer costs a collect-and-finalize cycle of its own - a script whose `OnUpdate` has run a few
frames needs three. `verify-script-reload.py` is the regression test, and it exists because a
two-cycle version of that loop reported a leak that was not there.

## Which components C# can reach

⚠ **`[ComponentType(ComponentType.X)]` is the whole binding** (`ScriptRuntime/World/ComponentTypes.cs`).
A component gets a managed mirror the moment a class under `ScriptRuntime/World/Components/` carries
that attribute, because `Pine.Core.ObjectFactory` finds the class by it; and it is what
`GetComponent<T>()` / `AddComponent<T>()` / `GetComponents<T>()` / `HasComponent<T>()` resolve `T`
back to, because native cannot inspect a `System.Type`. One attribute answers both directions, so
they cannot drift apart. Without it the class is not a component as far as either is concerned: no
mirror is made, and the generic methods log an error and behave as though the entity has no such
component. The attribute is inherited, so a game's own `class Player : Script` resolves to `Script`.

A component type with no class at all is warned about once per type rather than every time one is
created. The individual properties still need their internal calls in
`Script/Interfaces/ScriptInterfaceComponent.cpp`.

Bound today: `Transform`, `ModelRenderer`, `RigidBody`, `CharacterController`, `Script`, `Light`,
`Camera`, `Collider`, `AudioSource`, `AudioListener`. Not bound: `SpriteRenderer`,
`TilemapRenderer`, `TerrainRenderer`, `Collider2D`, `RigidBody2D`, `NativeScript`.

⚠ **A managed asset class is resolved by the asset type's own name**, not by the native class's:
`Pine.Core.ObjectFactory` looks for `Pine.Assets.<the AssetType member's name>`. So `Pine::AudioFile`
is `Pine.Assets.Audio` on the C# side, because that is what `AssetTypeToString(AssetType::Audio)`
returns and what the managed `AssetType` member is called. Nothing enforces the match, and getting it
wrong costs the asset its managed mirror - `AssetManager.Get<T>()` and every property that returns it
then hand back `null`, which is what `Script.ScriptAsset` did for as long as
`AssetTypeToString(AssetType::CSharpScript)` said `"Script"` and the class was called `CSharpScript`.
It is at least said now: a type with no class warns once. `AssetTypeToString` is the identifier and
has to match the class; the editor's labels come from `AssetTypeToHumanString`, which is free to
still read "Script".

There are also **no collision or trigger callbacks** - `Physics3D` exposes `RayCast` and nothing
else, so a pickup or a proximity check is a distance test or a ray, not an `OnTriggerEnter`.

## `CSharpScript` is a source-backed asset
A `CSharpScript` `.passet` is **not** the C# code — it's a thin identity asset. Its payload
stores the fully-qualified managed **type name** (e.g. `Game.Player`), and the editable `.cs`
source lives **next to it** in the project's `assets/` tree, registered as an `AssetSource`
(`Asset::AddSource`). The `.cs` is compiled into the game assembly separately (see below); the
engine only loads the compiled DLL and resolves each script's managed class by its stored type
name (`ScriptManager.cpp` → `ResolveScriptData`). Older assets with an empty payload fall back
to the legacy convention: namespace `Game`, class name == the `.passet` file stem.

Creating a script in the editor (Asset Browser → Create → Script) writes both `Foo.cs` (from a
built-in template, `Editor/.../ScriptUtilities.cpp`) and `Foo.passet` side by side; rename/delete
keep the pair in sync. There is **no** `.ih` sidecar for scripts (unlike shaders) — the `.passet`
isn't a compile output, so the source list + payload is all that's needed.

## Building & running the managed side
- **Engine API assembly (`Pine.dll`):** from `ScriptRuntime/`, `dotnet build -c Release`
  → outputs `Pine.dll`, `Pine.runtimeconfig.json` and `Pine.deps.json` to `data/engine/script/`
  (loaded by `ScriptingRuntime.cpp`, which starts the runtime from that `runtimeconfig.json`).
  Targets `net10.0`.

  ⚠ **A rebuilt `Pine.dll` needs an editor restart.** It is loaded once into the default load
  context and hot reload replaces the *game* assembly only. That is the trade that makes reload
  simple: engine-side C# is engine development, not gameplay iteration.
- **Per-project game assembly (`Game.dll`):** each project owns `projects/<name>/runtime/Game.csproj`,
  which globs `..\assets\**\*.cs` and outputs `projects/<name>/runtime-bin/Game.dll`. Build it with
  `dotnet build -c Release`, or in your IDE (CLion/Rider). The engine loads it **per project**: the
  Editor calls `Script::Manager::LoadGameAssembly(GetProjectPath() + "/runtime-bin/Game.dll")` after
  the project is selected (`Editor/src/Application.cpp`); GameHost loads its baked
  `game/runtime-bin/Game.dll`. It is read into memory and loaded from a stream rather than by path,
  so a build can overwrite it while the editor has it open. The editor hot-reloads on window focus
  when the DLL's write-time changes (`ScriptUtilities.cpp`). Builds are **external** — the engine
  never invokes a compiler.

  ⚠ Three csproj properties are load-bearing, and `data/projects/project-template/runtime/` is
  where every new project gets them from. `<AppendTargetFrameworkToOutputPath>false` keeps the
  output out of a `net10.0/` subfolder, which every path that loads one of these assemblies is
  written without. `<Private>false</Private>` on the `Pine` reference stops a copy landing beside
  `Game.dll`, which the game's load context would load a *second* time — its `Script` base class
  would then be a different type from the engine's. And `<EnableDynamicLoading>true` on
  `Pine.csproj` is what emits the `runtimeconfig.json` the host starts from.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

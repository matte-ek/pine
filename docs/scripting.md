# C# scripting (Mono)

Gameplay scripting in C#, hosted through Mono. The C++ side is `Engine/src/Pine/Script/`;
the managed side is the top-level `ScriptRuntime/` project (builds `Pine.dll`). Paths below
are relative to `Engine/src/Pine/` unless noted.

## Start here
- `Script/ScriptManager.{hpp,cpp}` — the high-level bridge (`Pine::Script::Manager`).
- `Script/Runtime/` — the Mono host (`Pine::Script::Runtime`): domain, assemblies, GC.
- `Script/Factory/` — `ScriptObjectFactory`, which pairs each engine object with a managed one.
- `Script/Interfaces/` — the native "internal calls" exposed to C#.
- `ScriptRuntime/` (repo root) — the C# `Pine` API that game code references.

## How it fits together
- **`Script::Runtime`** wraps Mono directly (`MonoDomain`, `MonoAssembly`/`MonoImage`, GC) and owns the engine's own `Pine` assembly. It is set up **very early** in engine boot (before Assets/World) so other systems can register bindings.
- **`Script::Manager`** drives gameplay: `LoadGameAssembly(path)`, `ReloadScripts()` / `ReloadGameAssembly()` (hot reload), and the per-frame `OnStart()` / `OnUpdate(dt)` / `OnRender(dt)` dispatch (called from `World`). It keeps a `ScriptData` per C# class, resolving Mono methods by name and reflecting serialized fields (`Script/Scripts/ScriptField.hpp`).
- **`Script::ObjectFactory`** gives every `Entity`, `Component` and `Asset` a paired `MonoObject` via an embedded `ObjectHandle` (managed pointer + GC handle), so engine objects and their C# mirrors stay linked.
- **`Script/Interfaces/`** registers the native functions C# calls into: Log, Input, Entity, Component, Asset, Physics. Each has a matching class on the C# side (`ScriptRuntime/Core`, `/Input`, `/World`, `/Assets`, `/Physics`, `/Math`).

## Two script component kinds
- **`ScriptComponent`** (`World/Components/Script/`) — a managed C# script, backed by a `CSharpScript` asset (`Assets/CSharpScript/`). This is the normal one.
- **`NativeScript`** (`World/Components/NativeScript/`) — a C++ script; currently a stub.

## Script fields

A script's **public** fields are reflected out of the game assembly (`ScriptManager.cpp` ->
`ProcessScriptFields`), shown in the properties panel, and saved with the component. A field whose
type the engine cannot store is not reflected at all - it still works in C#, it just cannot be
authored. What is stored:

| C# type | Stored as |
|---|---|
| `bool`, `int`, `float` | the raw value |
| `Vector2` / `Vector3` / `Vector4` | the raw value |
| `string` | UTF-8 bytes |
| an asset (`Model`, `Material`, ... - anything deriving from `Pine.Assets.Asset`) | the asset's `UId` |

**`Entity` fields are reflected but not stored yet.** Resolving one has to wait until a whole level
has loaded (`Level::Load` spawns blueprints in a loop, so the first entity cannot reference the
last), and `Blueprint::Spawn` gives every copy new `UId`s, so a reference inside a blueprint would
have to be remapped to the copy. Use `EntityList.Find(name)` or `Find(tag)` from `OnStart` instead.
The editor shows such a field as "Not supported yet" rather than an empty row.

**Where the values live.** A script field's value is only really a field on the managed object, and
that object is destroyed and rebuilt constantly: on level load, on blueprint spawn, and on every hot
reload, which resets the whole Mono domain. So `ScriptComponent` keeps its own copy
(`ScriptFieldValue`, matched to fields by name) and moves it across at three points:

- `CreateInstance()` pushes the stored values into each new managed object.
- `SaveData()` pulls the current ones out first, so whatever the properties panel just typed is what
  gets written. This is also what makes the editor's play/stop survive, since `PlayHandler`
  snapshots and restores the world through `SaveData`/`LoadData`.
- `Runtime::Dispose()` pulls them before the domain goes, which is what carries them over a hot
  reload.

Stored values whose name no longer matches a field are **kept**, not dropped - otherwise a script
that fails to compile, or a level loaded before its game assembly, would silently discard everything
the author set. The debug server has no operation for script fields; they are authored in the
properties panel.

## `CSharpScript` is a source-backed asset
A `CSharpScript` `.passet` is **not** the C# code — it's a thin identity asset. Its payload
stores the fully-qualified managed **type name** (e.g. `Game.Player`), and the editable `.cs`
source lives **next to it** in the project's `assets/` tree, registered as an `AssetSource`
(`Asset::AddSource`). The `.cs` is compiled into the game assembly separately (see below); the
engine only loads the compiled DLL and resolves each script's `MonoClass` by its stored type
name (`ScriptManager.cpp` → `ResolveScriptData`). Older assets with an empty payload fall back
to the legacy convention: namespace `Game`, class name == the `.passet` file stem.

Creating a script in the editor (Asset Browser → Create → Script) writes both `Foo.cs` (from a
built-in template, `Editor/.../ScriptUtilities.cpp`) and `Foo.passet` side by side; rename/delete
keep the pair in sync. There is **no** `.ih` sidecar for scripts (unlike shaders) — the `.passet`
isn't a compile output, so the source list + payload is all that's needed.

## Building & running the managed side
- **Engine API assembly (`Pine.dll`):** from `ScriptRuntime/`, `msbuild -t:Build -p:Configuration=Release`
  → outputs `Pine.dll` to `data/engine/script/` (loaded by `ScriptingRuntime.cpp`). Targets .NET
  Framework 4.7.2 via Mono.
- **Per-project game assembly (`Game.dll`):** each project owns `projects/<name>/runtime/Game.csproj`,
  which globs `..\assets\**\*.cs` and outputs `projects/<name>/runtime-bin/Game.dll`. Build it in
  your IDE (CLion/Rider). The engine loads it **per project**: the Editor calls
  `Script::Manager::LoadGameAssembly(GetProjectPath() + "/runtime-bin/Game.dll")` after the project
  is selected (`Editor/src/Application.cpp`); GameHost loads its baked `game/runtime-bin/Game.dll`.
  The editor hot-reloads on window focus when the DLL's write-time changes (`ScriptUtilities.cpp`).
  Builds are **external** — the engine never invokes a compiler.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

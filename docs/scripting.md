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

## Building & running the managed side
From `ScriptRuntime/`: `msbuild -t:Build -p:Configuration=Release` → outputs `Pine.dll` to
`data/engine/script/` (the path the engine loads). Targets .NET Framework 4.7.2 via Mono.
See the root `README.md` / `CLAUDE.md` for the full flow.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

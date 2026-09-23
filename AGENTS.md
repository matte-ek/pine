## What Pine is

Pine is a 3D/2D game engine written in C++17, developed over several years as a personal project. The repo is one CMake tree producing an `Engine` static library plus three executables that link it: **Editor** (the GUI editor, uses Dear ImGui), **GameHost** (standalone game runner), and **EngineCli**. C# gameplay scripting runs on .NET, with the engine hosting CoreCLR itself; the managed side lives in `ScriptRuntime/` and compiles to a `Pine.dll`.

# Working preferences

The modules linked below are loaded into your context automatically (see `CLAUDE.md`). They are instructions, not background reading: apply the relevant module at the point it applies. Do not work from a summary, a previous session, or an assumption about what a module contains. The one exception is [Code review and audit](agent-guidelines/code-review.md), which is not preloaded — read it with your file-reading tool if and when it activates.

When interpreting requests and working with the user, apply [Collaborative iteration](agent-guidelines/collaborative-iteration.md).

Before writing or changing code, apply these modules:

- [Readable code](agent-guidelines/readable-code.md)
- [Design for likely changes](agent-guidelines/design-for-change.md)
- [Consistency with nearby code](agent-guidelines/local-consistency.md)

Before reporting code work as finished, apply [Complete the change](agent-guidelines/complete-the-change.md).

Do not commit, push, or deploy anything unless the user asks for that action in the current request. Apply [Actions to leave to the user](agent-guidelines/leave-to-the-user.md).

Only when the user explicitly asks for a code review or audit, read and apply [Code review and audit](agent-guidelines/code-review.md). Apply it to the requested review scope and phase only. Do not activate it for general questions, implementation work, routine development checks, or requests to write review guidelines. A request such as "Review these changes" or "Audit the permission handling" activates it for that scope; an implementation request or a routine development check does not.

Write code that the user and their coworkers can quickly read, understand, and maintain. Do not optimize source code for fewer tokens or lines.

Apply these preferences alongside the target project's instructions and tooling. Follow explicit project conventions for syntax and formatting, and use these modules to guide clarity and design within those conventions.

## Conventions

- **Subsystems are namespaces, not classes.** Nearly every subsystem is a `namespace Pine::<Name>` exposing free functions — a `Setup()`/`Shutdown()` (sometimes `Dispose()`) lifecycle plus `Update()`/`Run()` — with its mutable state in an anonymous `namespace { }` at the top of the `.cpp`. You reach a subsystem by calling `Pine::<Name>::Function()`, not by holding an object. Engine-internal entry points live in a nested `Internal` namespace (e.g. `Assets::Internal`, `WindowManager::Internal`). Editor code has no single root namespace: newer areas sit under `Editor` (`Editor::DebugServer`, `Editor::Gui`, `Editor::Utilities`), while panels are `Panels::<Name>` and older subsystems (`PlayHandler`, `Selection`, `MenuBar`, `Gizmo`, `KeybindSystem`) are top-level namespaces.

- **Layout:** one folder per class/subsystem, containing a `.hpp`/`.cpp` pair named after the folder (e.g. `World/Components/Camera/Camera.hpp`). Everything under `Engine/src/`, included as `Pine/...` (that's the include root). `CMakeLists.txt` uses `GLOB_RECURSE`, so a fresh CMake configure is needed after adding files.

- **Interfaces** are prefixed `I` and live in an `Interfaces/` folder — this is the backend seam (`Graphics/Interfaces/IGraphicsAPI` with an `OpenGL/` impl; `Audio/` mirrors this). **Handle** types (`EntityHandle`, `ComponentHandle<T>`, `AssetHandle<T>`, `ObjectHandle`) store an id + cached pointer and re-validate on access.

- **Logging:** `PInfo/PWarning/PError/PFatal/PVerbose(msg)` macros taking a single string; format it yourself, e.g. `PInfo(fmt::format("Loaded {}", path))`. `PVerbose` is dropped unless enabled (`PINE_VERBOSE` in the environment, or `Log::SetVerboseEnabled`). Profiling: `PINE_PF_SCOPE()`.

- **Naming:** namespaces mirror directories; config/member fields commonly use an `m_` prefix.

## Build and verify

- **Native:** agents build into `cmake-build-debug-agent/`, or `cmake-build-release-agent/` when a release build is needed. Leave the other `cmake-build-*` directories to the developer. Use Ninja: the verification recipes and native probes read the build through `ninja -t commands`.

  ```sh
  cmake -S . -B cmake-build-debug-agent -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build cmake-build-debug-agent --target Editor -j4
  ```

  The recipes in `Editor/src/DebugServer/Verification/` use `cmake-build-debug-agent/` unless given `--build`.

- **Managed:** after changing `ScriptRuntime/`, run `dotnet build -c Release` in that folder. It writes `Pine.dll` to `data/engine/script/`, where the engine loads it from.

- **There are no unit tests.** A change is verified by building it and then running it: launch the Editor headlessly with the debug server enabled ([`docs/editor.md`](docs/editor.md#running-it-headlessly)) and inspect the state or the viewport through the routes in [`docs/debug-server.md`](docs/debug-server.md). `Editor/src/DebugServer/Verification/` holds the existing `verify-<area>.py` recipes and native probes; reuse one when it covers your area.

- **Performance:** judge speed from a release build only. Debug is `-O0` and makes per-object CPU loops look many times more expensive than they are. On a machine that renders through a software rasterizer (`glxinfo -B` reports `llvmpipe`), take no timings at all. Reason from the code, name the `PINE_PF_SCOPE` scopes worth watching, and leave the measuring to the user.

## Architecture (the parts that span multiple files)

The summaries below are the map; **`docs/` holds a one-page orientation doc per subsystem** (key files, how the pieces fit, gotchas) — read the relevant one before working in that area. Index: [`docs/README.md`](docs/README.md).

- [`docs/world-ecs.md`](docs/world-ecs.md) — entities, components, the pool storage, the enum-ordering gotcha
- [`docs/assets.md`](docs/assets.md) — asset manager, the `.passet` format, serializer, Level/Blueprint
- [`docs/rendering.md`](docs/rendering.md) — Graphics wrapper, RenderManager, Renderer3D, pipelines & features
- [`docs/scripting.md`](docs/scripting.md) — the hosted .NET/C# bridge and `ScriptRuntime/`
- [`docs/audio.md`](docs/audio.md) — device layer, the voice pool, the audio components, mono vs. `Spatial`
- [`docs/physics.md`](docs/physics.md) — Physics3D (PhysX), the Physics2D stub, and the component coupling
- [`docs/core.md`](docs/core.md) — Core utilities (Math, File, Serialization, UId, Log, WindowManager)
- [`docs/editor.md`](docs/editor.md) — the Editor executable: boot, panels, viewports, play mode
- [`docs/data-and-projects.md`](docs/data-and-projects.md) — the `data/` tree, project structure, asset path resolution

- **Lifecycle** (`Pine::Engine`, driven by each app's `Application.cpp`): `Setup(EngineConfiguration)` → `Run()` → `Shutdown()`. Boot order matters: GLFW/window → Graphics → Audio → Threading → **Script runtime first** (so other systems can register C# bindings) → Assets (`LoadAssetsFromDirectory("engine")` must succeed) → Components/Entities → Rendering → Physics → Input → World → Game. `Run()` shows the window then loops: poll events → clear → `Input::Update()` + `World::Update()` → `RenderManager::Run()` → swap. Key `EngineConfiguration` flags: `m_MaxObjectCount` (caps entities **and** per-type components, except the few types `Components::Setup()` gives a fixed count, such as `Camera`), `m_ProductionMode` (game vs. editor behavior), `m_Standalone`, `m_GraphicsAPI`.

- **ECS (custom, pool-based** — not EnTT/flecs). `World/`. Each component type gets a `ComponentDataBlock<T>`: a contiguous array of `m_MaxObjectCount` instances (or a per-type override) + a parallel occupation array, iterated by a custom iterator that skips empty/disabled slots. New components are placed in the first free slot and initialized by copying a default-constructed prototype. `Component` (base) defines the virtual lifecycle everything relies on: `OnCreated/OnDestroyed/OnCopied/OnSetup/OnUpdate/OnRender/OnPre|PostPhysicsUpdate` + `LoadData/SaveData`. `Entity` owns a `vector<Component*>`, a parent/child hierarchy, a `UId`, and always has a `Transform`. **Gotcha:** the `ComponentType` enum (`World/Components/Component/Component.hpp`) order must stay exactly in sync with the `CreateComponentDataBlock<T>()` call order in `Components::Setup()`, and with the managed copy of the enum in `ScriptRuntime/World/Component.cs`. Append new types at the end: saved Levels and Blueprints store the type as its integer value. Adding a component = new `World/Components/<Name>/` folder **plus** those three edits, plus cases in `ComponentTypeToString`/`ComponentTypeToHumanString` and the editor's properties renderer — see [`docs/world-ecs.md`](docs/world-ecs.md).

- **Assets & the `.passet` format** (`Assets/`, `Pine::Assets` is the manager). A `.passet` file is the engine's own **compressed** binary container (`File::Read/WriteCompressed`): a header (UId, timestamp, `AssetType`, virtual path, source-file list) followed by a type-specific payload the subclass writes via `SaveAssetData()`/`LoadAssetData()`. `.passet` files are therefore not human-readable. Raw sources (`.png`, models, `.cs`) are brought in through the **Importer** pipeline (`Assets/Importer/`). Serialization is a reflective binary serializer (`Core/Serialization/`): declare a `struct XSerializer : Serialization::Serializer` and list fields with `PINE_SERIALIZE_PRIMITIVE/STRING/DATA/ARRAY/ARRAY_FIXED/ASSET`; this same mechanism serializes assets, components, and scenes. A **scene** is a `Level` asset (skybox/ambient/fog/camera + a list of `Blueprint`s); a `Blueprint` is a serialized entity-with-components that `Spawn()` instantiates.

- **Rendering** — two layers. `Graphics/` is the API-agnostic GPU wrapper (`IGraphicsAPI` + `OpenGL/` impl: shader programs, buffers, textures, framebuffers, VAOs, UBOs). `Rendering/` is the engine renderer on top: `RenderManager::Run()` (once per frame) drives a list of `RenderingContext`s (each ≈ a camera/target — multiple can render simultaneously, e.g. editor viewport + game camera), running `Pipeline3D` then `Pipeline2D` with hookable stages (`AddRenderCallback`). `Renderer3D` is the low-level submission API (instanced batching, lights, shadows). `Rendering/Features/` holds the passes (AmbientOcclusion, Bloom, PostProcessing, RenderCulling, Shadows, Skybox, TerrainRenderer, TerrainDetail); `Rendering/SceneProcessor/` walks the ECS blocks to gather draws/lights. Shaders/materials/meshes are all assets.

- **C# scripting (.NET)** — `Engine/src/Pine/Script/` on the C++ side, `ScriptRuntime/` on the managed side. `Script::Runtime` starts CoreCLR through `hostfxr` and loads the engine's own `Pine.dll` into the default load context, once, at boot; `Script::GameAssembly` keeps the game's `Game.dll` in a **collectible** context of its own, which is what makes hot reload work. `Script::Manager` bridges gameplay: `LoadGameAssembly`, `ReloadScripts()`, `ReloadGameAssembly()` (hot reload), and `OnStart` / per-frame `OnUpdate` (from `World`) / `OnRender` (from `RenderManager::Run`, before drawing) dispatch over the `ScriptData` and serialized `ScriptField`s it keeps per script class. `Script::ObjectFactory` gives every `Entity`/`Component`/`Asset` a paired managed object via an embedded `ObjectHandle` — an opaque `GCHandle` value that nothing native dereferences. Crossings go both ways by name: `Script/Bindings/` is the table of engine functions C# may call (filled by `Script/Interfaces/`: Log, Input, Entity, Component, Asset, Physics), and `Script/ManagedCall/ManagedCall.hpp` resolves the `[UnmanagedCallersOnly]` entry points the engine calls in `Pine.dll`. Two script component kinds exist: `ScriptComponent` (managed C#, backed by a `CSharpScript` asset) and `NativeScript` (C++, currently a stub).

## What Pine is

Pine is a 3D/2D game engine written in C++17, developed over several years as a personal project. The repo is one CMake tree producing an `Engine` static library plus three executables that link it: **Editor** (the GUI editor, uses Dear ImGui), **GameHost** (standalone game runner), and **EngineCli**. C# gameplay scripting is provided through a Mono runtime; the managed side lives in `ScriptRuntime/` and compiles to a `Pine.dll`.

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

- **Subsystems are namespaces, not classes.** Nearly every subsystem is a `namespace Pine::<Name>` exposing free functions — a `Setup()`/`Shutdown()` (sometimes `Dispose()`) lifecycle plus `Update()`/`Run()` — with its mutable state in an anonymous `namespace { }` at the top of the `.cpp`. You reach a subsystem by calling `Pine::<Name>::Function()`, not by holding an object. Engine-internal entry points live in a nested `Internal` namespace (e.g. `Assets::Internal`, `WindowManager::Internal`). Editor code is under `namespace Editor`.

- **Layout:** one folder per class/subsystem, containing a `.hpp`/`.cpp` pair named after the folder (e.g. `World/Components/Camera/Camera.hpp`). Everything under `Engine/src/`, included as `Pine/...` (that's the include root). `CMakeLists.txt` uses `GLOB_RECURSE`, so a fresh CMake configure is needed after adding files.

- **Interfaces** are prefixed `I` and live in an `Interfaces/` folder — this is the backend seam (`Graphics/Interfaces/IGraphicsAPI` with an `OpenGL/` impl; `Audio/` mirrors this). **Handle** types (`EntityHandle`, `ComponentHandle<T>`, `AssetHandle<T>`, `ObjectHandle`) store an id + cached pointer and re-validate on access.

- **Logging:** `PInfo/PWarning/PError/PFatal/PVerbose(msg)` macros (fmt-formatted). Profiling: `PINE_PF_SCOPE()`.

- **Naming:** namespaces mirror directories; config/member fields commonly use an `m_` prefix.

## Architecture (the parts that span multiple files)

The summaries below are the map; **`docs/` holds a one-page orientation doc per subsystem** (key files, how the pieces fit, gotchas) — read the relevant one before working in that area. Index: [`docs/README.md`](docs/README.md).

- [`docs/world-ecs.md`](docs/world-ecs.md) — entities, components, the pool storage, the enum-ordering gotcha
- [`docs/assets.md`](docs/assets.md) — asset manager, the `.passet` format, serializer, Level/Blueprint
- [`docs/rendering.md`](docs/rendering.md) — Graphics wrapper, RenderManager, Renderer3D, pipelines & features
- [`docs/scripting.md`](docs/scripting.md) — the Mono/C# bridge and `ScriptRuntime/`
- [`docs/physics.md`](docs/physics.md) — Physics2D/Physics3D (PhysX) and the component coupling
- [`docs/core.md`](docs/core.md) — Core utilities (Math, File, Serialization, UId, Log, WindowManager)
- [`docs/editor.md`](docs/editor.md) — the Editor executable: boot, panels, viewports, play mode
- [`docs/data-and-projects.md`](docs/data-and-projects.md) — the `data/` tree, project structure, asset path resolution

- **Lifecycle** (`Pine::Engine`, driven by each app's `Application.cpp`): `Setup(EngineConfiguration)` → `Run()` → `Shutdown()`. Boot order matters: GLFW/window → Graphics → Audio → Threading → **Script runtime first** (so other systems can register C# bindings) → Assets (`LoadAssetsFromDirectory("engine")` must succeed) → Components/Entities → Rendering → Physics → Input → World → Game. `Run()` shows the window then loops: poll events → clear → `Input::Update()` + `World::Update()` → `RenderManager::Run()` → swap. Key `EngineConfiguration` flags: `m_MaxObjectCount` (caps entities **and** per-type components), `m_ProductionMode` (game vs. editor behavior), `m_Standalone`, `m_GraphicsAPI`.

- **ECS (custom, pool-based** — not EnTT/flecs). `World/`. Each component type gets a `ComponentDataBlock<T>`: a contiguous array of `m_MaxObjectCount` instances + a parallel occupation array, iterated by a custom iterator that skips empty/disabled slots. New components are placed in the first free slot and initialized by copying a default-constructed prototype. `Component` (base) defines the virtual lifecycle everything relies on: `OnCreated/OnDestroyed/OnCopied/OnSetup/OnUpdate/OnRender/OnPre|PostPhysicsUpdate` + `LoadData/SaveData`. `Entity` owns a `vector<Component*>`, a parent/child hierarchy, a `UId`, and always has a `Transform`. **Gotcha:** the `ComponentType` enum (`World/Components/Component/Component.hpp`) order must stay exactly in sync with the `CreateComponentDataBlock<T>()` call order in `Components::Setup()`. Adding a component = new `World/Components/<Name>/` folder **plus** edits to both of those.

- **Assets & the `.passet` format** (`Assets/`, `Pine::Assets` is the manager). A `.passet` file is the engine's own **compressed** binary container (`File::Read/WriteCompressed`): a header (UId, timestamp, `AssetType`, virtual path, source-file list) followed by a type-specific payload the subclass writes via `SaveAssetData()`/`LoadAssetData()`. `.passet` files are therefore not human-readable. Raw sources (`.png`, models, `.cs`) are brought in through the **Importer** pipeline (`Assets/Importer/`). Serialization is a reflective binary serializer (`Core/Serialization/`): declare a `struct XSerializer : Serialization::Serializer` and list fields with `PINE_SERIALIZE_PRIMITIVE/STRING/DATA/ARRAY/ASSET`; this same mechanism serializes assets, components, and scenes. A **scene** is a `Level` asset (skybox/ambient/fog/camera + a list of `Blueprint`s); a `Blueprint` is a serialized entity-with-components that `Spawn()` instantiates.

- **Rendering** — two layers. `Graphics/` is the API-agnostic GPU wrapper (`IGraphicsAPI` + `OpenGL/` impl: shader programs, buffers, textures, framebuffers, VAOs, UBOs). `Rendering/` is the engine renderer on top: `RenderManager::Run()` (once per frame) drives a list of `RenderingContext`s (each ≈ a camera/target — multiple can render simultaneously, e.g. editor viewport + game camera), running `Pipeline2D`/`Pipeline3D` with hookable stages (`AddRenderCallback`). `Renderer3D` is the low-level submission API (instanced batching, lights, shadows). `Rendering/Features/` holds pluggable passes (AmbientOcclusion, PostProcessing, Shadows, Skybox, TerrainRenderer); `Rendering/SceneProcessor/` walks the ECS blocks to gather draws/lights. Shaders/materials/meshes are all assets.

- **C# scripting (Mono)** — `Engine/src/Pine/Script/` on the C++ side, `ScriptRuntime/` on the managed side. `Script::Runtime` wraps Mono directly (`MonoDomain`, assembly load/unload, GC) and owns the engine's own `Pine` assembly. `Script::Manager` bridges gameplay: `LoadGameAssembly`, `ReloadScripts()` (hot reload), and per-frame `OnStart/OnUpdate/OnRender` dispatch, resolving Mono methods and reflecting serialized `ScriptField`s. `Script::ObjectFactory` gives every `Entity`/`Component`/`Asset` a paired managed object via an embedded `ObjectHandle`. `Script/Interfaces/` registers the native internal-calls (Log, Input, Entity, Component, Asset, Physics) that the C# side in `ScriptRuntime/` calls into. Two script component kinds exist: `ScriptComponent` (managed C#, backed by a `CSharpScript` asset) and `NativeScript` (C++, currently a stub).

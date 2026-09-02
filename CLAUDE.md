# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What Pine is

Pine is a 3D/2D game engine written in C++17, developed over several years as a personal project. The repo is one CMake tree producing an `Engine` static library plus three executables that link it: **Editor** (the GUI editor, uses Dear ImGui), **GameHost** (standalone game runner), and **EngineCli**. C# gameplay scripting is provided through a Mono runtime; the managed side lives in `ScriptRuntime/` and compiles to a `Pine.dll`.

> The `README.md` is out of date (it describes an `assets/` directory, reactphysics3d, and an msbuild-first flow). This file reflects current reality: the data root is `data/`, 3D physics is **PhysX**, and the build is **CMake-only** (the author builds via CLion).

## Working in this repo (git)

- **Commit only to the `ai` branch for now.** This branch is where AI-assisted work lands; do not commit to `main` (or other branches) unless explicitly asked.
- Do not commit prebuilt third-party binaries (see Dependencies below) unless the library's license clearly allows redistribution.

## Build & run

**Build (CMake only).** The author uses CLion, whose build dir is `cmake-build-debug/`. Equivalent from a shell:

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug -j            # all targets
cmake --build cmake-build-debug --target Editor -j   # single target
```

Targets: `Engine` (library), `Editor`, `GameHost`, `EngineCli`.

**Dependencies** are system packages plus bundled libs in `third-party/` (`imgui`, `material-icons`, `nvtt`, `perlin-noise`, `physx`). System packages on Arch: `glew assimp fmt mono openal glfw libjpeg-turbo libpng glm freetype2 nlohmann-json`. Note: the PhysX static libs in `third-party/physx/lib/*.a` and headers in `third-party/physx/include/` are **intentionally not committed** (kept out of version control unless a library's license clearly permits redistribution) — they must be present locally (`setup-env.sh` documents the PhysX build, though its copy paths predate the move into `third-party/`). Do not add prebuilt third-party binaries to git without checking the license first.

**Run the Editor.** The executables load assets via paths relative to the current working directory (e.g. `LoadAssetsFromDirectory("engine")` / `"editor"`), so **run from the `data/` directory** and pass a project name (projects live in `data/projects/`, e.g. `gm`, `stress`, `project-template`):

```bash
cd data
../cmake-build-debug/Editor/Editor gm
```

Set `PINE_X11=1` to force GLFW onto X11/XWayland (useful on Wayland, e.g. for RenderDoc).

**Build the C# runtime.** From `ScriptRuntime/`, `msbuild -t:Build -p:Configuration=Release` (Release outputs `Pine.dll` to `data/engine/script/`, the path the engine loads). Targets .NET Framework `v4.7.2` via Mono. Each **project** has its own gameplay assembly under `data/projects/<name>/runtime/` (`Game.csproj`, globs `../assets/**/*.cs`, outputs `runtime-bin/Game.dll`); the Editor loads it per-project after selecting the project, GameHost loads its baked `data/game/runtime-bin/Game.dll`. Builds are external (IDE); the engine only watches/loads the DLL. See [`docs/scripting.md`](docs/scripting.md).

**Tests:** there is no test suite or test framework in this repo. CI (`.github/workflows/cmake-build-linux.yml`) builds PhysX (cached) and compiles only the `Engine` target.

## Conventions

- **Subsystems are namespaces, not classes.** Nearly every subsystem is a `namespace Pine::<Name>` exposing free functions — a `Setup()`/`Shutdown()` (sometimes `Dispose()`) lifecycle plus `Update()`/`Run()` — with its mutable state in an anonymous `namespace { }` at the top of the `.cpp`. You reach a subsystem by calling `Pine::<Name>::Function()`, not by holding an object. Engine-internal entry points live in a nested `Internal` namespace (e.g. `Assets::Internal`, `WindowManager::Internal`). Editor code is under `namespace Editor`.
- **Layout:** one folder per class/subsystem, containing a `.hpp`/`.cpp` pair named after the folder (e.g. `World/Components/Camera/Camera.hpp`). Everything under `Engine/src/`, included as `Pine/...` (that's the include root). `CMakeLists.txt` uses `GLOB_RECURSE`, so a fresh CMake configure is needed after adding files.
- **Interfaces** are prefixed `I` and live in an `Interfaces/` folder — this is the backend seam (`Graphics/Interfaces/IGraphicsAPI` with an `OpenGL/` impl; `Audio/` mirrors this). **Handle** types (`EntityHandle`, `ComponentHandle<T>`, `AssetHandle<T>`, `ObjectHandle`) store an id + cached pointer and re-validate on access.
- **Logging:** `PInfo/PWarning/PError/PFatal/PVerbose(msg)` macros (fmt-formatted). Profiling: `PINE_PF_SCOPE()`.
- **Naming:** namespaces mirror directories; config/member fields commonly use an `m_` prefix.

## Architecture (the parts that span multiple files)

Read `Engine/src/Pine/Engine/Engine.cpp` first — the whole boot order and main loop are in one file. The summaries below are the map; **`docs/` holds a one-page orientation doc per subsystem** (key files, how the pieces fit, gotchas) — read the relevant one before working in that area. Index: [`docs/README.md`](docs/README.md).

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

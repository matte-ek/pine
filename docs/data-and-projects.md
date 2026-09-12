# `data/` layout & projects

The `data/` directory is the runtime root. **The executables load assets relative to the
current working directory, so they must be run from `data/`** (see root `README.md`). This
doc explains what lives where and how asset paths resolve — easy things to get wrong.

## Top-level layout
- **`data/engine/`** — built-in engine assets, always loaded (`Assets::LoadAssetsFromDirectory("engine")` during `Engine::Setup`). Holds `shaders/`, `materials/`, `primitive/` (built-in meshes), and `script/` (the compiled `Pine.dll` runtime — see [scripting.md](scripting.md)).
- **`data/editor/`** — editor-only assets, loaded by the Editor: `fonts/`, `icons/`, `models/` (gizmos), `shaders/`.
- **`data/game/`** — standalone game runtime output (`runtime/`), used by GameHost. Not committed.
- **`data/projects/`** — user projects, one folder each (`gm`, `stress`, `project-template`).
- **`data/cache/`** — derived data, e.g. `cache/import/`. Safe to delete; regenerated.

## Project structure
A project (`data/projects/<name>/`) has:
- **`assets/`** — the engine-native `.passet` files that make up the game (levels, materials, imported textures/models, C# scripts). This is what actually loads. Mostly compiled binaries, but it also holds editable **`.cs` script source** sitting next to its `CSharpScript` `.passet` (the one asset kind whose source is hand-edited in place — see [scripting.md](scripting.md)).
- **`content/`** — the *raw source files* (`.glb`, `.png`, …) that get **imported** into `.passet` files in `assets/`. Think "source" vs. "compiled". (See [assets.md](assets.md) for the importer.) Scripts do **not** go here — their `.cs` lives beside the `.passet` in `assets/`.
- **`runtime/`** — the project's C# gameplay project (`Game.csproj` / `game.sln`), built against the engine's `Pine` runtime. Its csproj globs `..\assets\**\*.cs` and outputs `runtime-bin/Game.dll`, which the engine loads per-project.

`data/projects/project-template/` is the skeleton copied to create a new project.

## How asset paths resolve
Assets use a **virtual path** rooted at a working directory, not raw filesystem paths:
- `Pine::Assets::SetWorkingDirectory(path)` sets the VFS prefix.
- `Pine::Assets::LoadAssetsFromDirectory(dir)` loads (recursively) relative to it; an empty string means "everything under the working directory".

The Editor wires this up across two calls (`Editor/src/Projects/Projects.cpp`):
```
Projects::SetProject(name)      -> Assets::SetWorkingDirectory("projects/" + name + "/assets");
Projects::LoadProjectAssets()   -> Assets::LoadAssetsFromDirectory("");  // load the whole project
```
`SetProject` only points the VFS at the project; nothing is read until `LoadProjectAssets`.
So within a project, an asset's engine path is relative to its `assets/` folder — that
virtual path (plus the asset's `UId`) is how references between assets are stored, which is
why moving `.passet` files around by hand breaks references.

## Running
Always launch from `data/` with a project name, e.g. `../cmake-build-debug/Editor/Editor gm`
(full command + `PINE_X11` in the root `README.md`).

Related: [assets.md](assets.md) · [editor.md](editor.md) · [scripting.md](scripting.md)

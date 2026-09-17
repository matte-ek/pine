# Editor

The `Editor` executable — an ImGui-based scene/asset editor that links the `Engine`
library. Source is under `Editor/src/` (namespace `Editor`). It is a *host* for the engine:
it boots the engine, then adds its own UI, rendering contexts and tooling on top.

To operate a running Editor through HTTP, start with the
[Editor API guide](debug-server.md): route reference, scene-building workflow,
asset discovery, captures, request recovery and saving.

## Start here
- `Editor/src/Application.cpp` — `main()`. The whole editor boot sequence in one file.
- `Editor/src/Gui/Gui.{hpp,cpp}` — ImGui setup + the panel loop (`Gui::Setup/Shutdown`).
- `Editor/src/Rendering/RenderHandler.hpp` — owns the editor's two `RenderingContext`s and their framebuffers.
- `Editor/src/Projects/Projects.hpp` — project selection + asset loading.
- `Editor/src/Other/PlayHandler/PlayHandler.hpp` — play/pause/stop of the simulation.
- `Editor/src/DebugServer/Editing/Editing.hpp` — debug-server scene writes; protocol and limits in [debug-server-editing.md](debug-server-editing.md).
- `Editor/src/DebugServer/Editing/History/History.hpp` and `Persistence/Persistence.hpp` — batch undo/redo and explicit level saving; see [history and persistence](debug-server-history.md).
- `Editor/src/DebugServer/LevelCamera/LevelCamera.hpp` — scene Camera selection; see [scene cameras](debug-server-scene-camera.md).
- `Editor/src/DebugServer/Camera/Camera.hpp` — editor-camera look-at, framing and read/restore; see [debug-server-camera.md](debug-server-camera.md).
- `Editor/src/DebugServer/Requests/Requests.hpp` — synchronized queue, mutation retry identities, status and cancellation; see [request lifecycle](debug-server-requests.md).
- `Editor/src/DebugServer/Observation/Observation.hpp` — frame-aware captures and combined observations; see [debug-server-observation.md](debug-server-observation.md).
- `Editor/src/DebugServer/Import/Import.hpp` — synchronous local-file imports through the shared editor utility; see [asset import](debug-server-import.md).

## Boot sequence (`Application.cpp`)
1. `Pine::Engine::Setup(...)` with `m_ProductionMode = false` (editor behavior, not game).
2. `Assets::LoadAssetsFromDirectory("editor")` — load editor-only assets (icons, fonts, gizmo models).
3. `Projects::SetProject(argv[1])` then `Projects::LoadProjectAssets()` — load the user's project (see [data-and-projects.md](data-and-projects.md)).
4. `World::SetPaused(true)` — never auto-start simulation in the editor.
5. Editor subsystems: `LevelEntity::Setup()`, `RenderHandler::Setup()`, `Gui::Setup()`, `Utilities::Script::Setup()`.
6. `Pine::Engine::Run()` (blocks), then symmetric shutdown.

So: an editor needs a project name as `argv[1]`, and must run with `data/` as the working directory.

## How it fits together
- **Two rendering contexts** (`RenderHandler`): a **Level** context (the editable scene view) and a **Game** context (what the running game sees), each rendering to its own framebuffer that a viewport panel displays as an ImGui image. This is why the engine supports multiple simultaneous `RenderingContext`s (see [rendering.md](rendering.md)).
- **Panels** live in `Gui/Panels/` — each is an immediate-mode ImGui panel drawn every frame: `LevelViewport`/`GameViewport`, `EntityList`, `Properties` (with `EntityPropertiesRenderer` → `ComponentPropertiesRenderer` and `AssetPropertiesRenderer`), `AssetBrowser`, `Console`, `Profiler`, `DebugPanel`, `Engine`, `LevelPanel`, `GamePanel`,
  `GraphicsSettings`, `TerrainTools`. The menu bar is its own thing in `Gui/MenuBar/`.
- **Shared UI tooling** in `Gui/Shared/`: `Gizmo/` (2D/3D transform gizmos), `Selection/` + `Other/EntitySelection`, `KeybindSystem/`, `Commands/` (undo/redo-style actions, see `Other/Actions/`), `Widgets/`, `IconStorage/`, `AssetImportSettings/` (the import-settings widgets, shared by the properties panel and the import dialog so the two can't drift).
- **Dialogs** live in `Gui/Dialogs/`. `AssetImport/` is the import review: dropping files onto the window only *stashes* the paths (the GLFW drop callback runs inside `glfwPollEvents`, no place to compile textures), and the next frame resolves them, shows what each file will do — new, replaces-existing, unsupported, blocked — lets the selection's import settings be edited, and then runs the import a time-budgeted slice per frame. See [assets.md](assets.md) for the phases it drives. Cancelling restores the settings of every row that was re-importing a live asset.
- **`Other/EditorEntity` / `LevelEntity`** is the editor-only camera/entity used to fly around the Level view; it is *not* part of the user's scene.
- **Play mode** (`Other/PlayHandler`): `EditorGameState` = Stopped / Playing / Paused; `Play()`/`Pause()`/`Stop()` toggle `World` simulation and script updates without leaving the editor.
- **Terrain sculpting and painting** (`Gui/Panels/TerrainTools/` + `Other/TerrainSculpting/`): the
  panel carries the brush — mode, radius, strength, falloff, and a layer while painting — and a
  switch that hands the Level viewport's left button over to it, suppressing ImGuizmo and entity
  picking while it is on. It deliberately does *not* pick a terrain to edit: the cursor ray already
  says which terrain is under it, so a target chosen in the panel would be a second, sometimes
  wrong, answer to a question already answered.
  `TerrainSculpting` holds the parts worth testing away from a mouse — `BuildCursorRay` (viewport
  point → world ray), `PickTerrain` (world ray → nearest terrain and the point on it) and the brush
  itself, which turns a stroke into one undo step. `Paint` is a fifth brush mode rather than a
  second tool: a stroke writes one of the terrain's two fields, so which field it writes is what a
  mode already says. Both fields go through the same rectangle accessors and the same undo command,
  which is templated over the field rather than written twice. The ring drawn on the ground under
  the brush is a shader version of the terrain shader (see [rendering.md](rendering.md)), not an
  overlay drawn on top, so it follows uneven ground exactly. `POST /terrain/sculpt` drives the same
  brush over HTTP, painting included.
- **Editor utilities** in `Utilities/`: `Assets/` (build an import context for dropped files, create/delete assets, refresh), `Scripts/` (compile/reload the project's C# assembly).

## Conventions
- Panels are stateless-ish immediate-mode code: they read/write engine + selection state each frame rather than holding models. Add a panel by creating a `Gui/Panels/<Name>/` folder and registering it in the `Gui` panel loop.
- Editor code never assumes production mode; keep editor-only behavior behind the editor, not in `Engine`.

Related: [data-and-projects.md](data-and-projects.md) · [rendering.md](rendering.md) · [assets.md](assets.md) · [scripting.md](scripting.md)

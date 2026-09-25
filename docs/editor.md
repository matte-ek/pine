# Editor

The `Editor` executable — an ImGui-based scene/asset editor that links the `Engine`
library. Source is under `Editor/src/`. Most of it is in `namespace Editor` (`Editor::Gui`,
`Editor::Projects`, `Editor::DebugServer`, `Editor::Utilities`, ...), but a number of UI pieces are
top-level namespaces: `PlayHandler`, `Selection`, `Panels::*`, `MenuBar`, `KeybindSystem`, `Gizmo`,
`Widgets`, `EntityPropertiesPanel`. It is a *host* for the engine:
it boots the engine, then adds its own UI, rendering contexts and tooling on top.

To operate a running Editor through HTTP, read the
[debug server HTTP reference](debug-server.md) for the routes and their contracts,
and [building a scene through the debug server](debug-server-workflow.md) for the
workflow that uses them.

## Start here
- `Editor/src/Application.cpp` — `main()`. The whole editor boot sequence in one file.
- `Editor/src/Gui/Gui.{hpp,cpp}` — ImGui setup + the panel loop (`Gui::Setup/Shutdown`).
- `Editor/src/Rendering/RenderHandler.hpp` — owns the editor's two `RenderingContext`s and their framebuffers.
- `Editor/src/Projects/Projects.hpp` — project selection + asset loading.
- `Editor/src/Other/PlayHandler/PlayHandler.hpp` — play/pause/stop of the simulation.
- `Editor/src/DebugServer/` — the localhost HTTP control server. `DebugServer.cpp` owns the
  transport and the threading rule (HTTP workers never touch engine state); `Requests/` is the
  main-thread queue, retry identities and cancellation; `Endpoints/Endpoints.cpp` registers every
  route. One folder per area below it:
  - `Editing/` — the `POST /edit` batch: preparation and execution, `Schema/` for discovery,
    `Values/` for shared validation, `Components/<Type>/` adapters that apply state through public
    setters, plus `History/`, `Placement/` and `Duplication/`.
  - `Inspection/`, `Spatial/` (with `Queries/` for raycasts and overlaps) — read-only scene
    queries, bounds and transforms; `Spatial/` shares its model measurement with camera framing.
  - `Observation/`, `Picking/`, `Screenshot/`, `Camera/` — frame-aware captures, retained
    surface picking, PNG output and editor-camera control.
  - `Capture/` — `GET /asset/preview.png` and `POST /render`, offscreen renders of an asset or
    the scene.
  - `Catalog/` — `/assets`, `/asset` and `/assets/summary`, asset discovery.
  - `Persistence/`, `LevelCamera/`, `LevelSettings/`, `Import/`, `LogHistory/` — level
    save/load, game-camera selection, `/level/settings`, asset import and incremental logs.
  - `Verification/` — per-area Python recipes and native probes; see the
    [workflow guide](debug-server-workflow.md#verifying-a-change-to-the-debug-server-itself).

  Route contracts are in [debug-server.md](debug-server.md).

## Boot sequence (`Application.cpp`)
1. `Pine::Engine::Setup(...)` with `m_ProductionMode = false` (editor behavior, not game).
2. `Assets::LoadAssetsFromDirectory("editor")` — load editor-only assets (icons, fonts, gizmo models).
3. `Projects::SetProject(argv[1])` then `Projects::LoadProjectAssets()` — load the user's project (see [data-and-projects.md](data-and-projects.md)).
4. `Script::Manager::LoadGameAssembly()` on `<project>/runtime-bin/Game.dll` if it exists; otherwise a warning, and scripts are unavailable until the project is built.
5. `World::SetPaused(true)` — never auto-start simulation in the editor.
6. Editor subsystems, in order: `LevelEntity::Setup()`, `RenderHandler::Setup()`, `DebugServer::SetupRenderObservation()`, `Gui::Setup()`, `Utilities::Script::Setup()`, and last `DebugServer::Setup()` (a no-op unless `PINE_DEBUG_SERVER` is set).
7. `Pine::Engine::Run()` (blocks), then shutdown: `DebugServer::Shutdown()`, `Gui::Shutdown()`, `RenderHandler::Shutdown()`, `LevelEntity::Dispose()`, `Pine::Engine::Shutdown()`. `Utilities::Script` has no shutdown.

So: an editor needs a project name as `argv[1]`, and must run with `data/` as the working directory.

## How it fits together
- **Two rendering contexts** (`RenderHandler`): a **Level** context (the editable scene view) and a **Game** context (what the running game sees), each rendering to its own framebuffer that a viewport panel displays as an ImGui image. This is why the engine supports multiple simultaneous `RenderingContext`s (see [rendering.md](rendering.md)).
- **Panels** live in `Gui/Panels/` — each is an immediate-mode ImGui panel drawn every frame: `LevelViewport`/`GameViewport`, `EntityList`, `Properties` (with `EntityPropertiesRenderer` → `ComponentPropertiesRenderer` and `AssetPropertiesRenderer`), `AssetBrowser`, `Console`, `Profiler`, `DebugPanel`, `Engine`, `LevelPanel`, `GamePanel`,
  `GraphicsSettings`, `TerrainTools`. The menu bar is its own thing in `Gui/MenuBar/`.
- **Shared UI tooling** in `Gui/Shared/`: `Gizmo/` (2D/3D transform gizmos), `Selection/` + `Other/EntitySelection`, `KeybindSystem/`, `Commands/` (undo/redo-style actions, see `Other/Actions/`), `Widgets/`, `IconStorage/`, `AssetImportSettings/` (the import-settings widgets, shared by the properties panel and the import dialog so the two can't drift).
- **Dialogs** live in `Gui/Dialogs/`. `AssetImport/` is the import review: dropping files onto the window only *stashes* the paths (the GLFW drop callback runs inside `glfwPollEvents`, no place to compile textures), and the next frame resolves them, shows what each file will do — new, replaces-existing, unsupported, blocked — lets the selection's import settings be edited, and then runs the import a time-budgeted slice per frame. See [assets.md](assets.md) for the phases it drives. Cancelling restores the settings of every row that was re-importing a live asset.
- **`Other/EditorEntity` / `LevelEntity`** is the editor-only camera/entity used to fly around the Level view; it is *not* part of the user's scene.
- **Play mode** (`Other/PlayHandler`): `EditorGameState` = Stopped / Playing / Paused. `PlayHandler::Play()` snapshots the level (`Level::CreateFromWorld`) and the active level's `LevelSettings`, and unpauses `World`; `PlayHandler::Stop()` restores both, re-pauses, clears the selection and runs the managed GC — **anything changed during play is thrown away on Stop**. The viewport panels only offer Play and Stop: `PlayHandler::Pause()` is called only by the native verification probes, and there is no resume, since `PlayHandler::Play()` asserts the state is Stopped.
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
- **Component copy, paste and reset**: each component header in the Properties panel has a
  `⋮` menu, also opened by right-clicking the header, with Copy, Paste Values (onto a component of
  the same type) and Reset (to what "Add new component" gives). The paste button next to "Add new
  component..." adds the copied component as a new one. The clipboard
  (`Editor::Clipboard::Component`) holds a `SaveData()` snapshot taken at copy time, so like
  `SaveData()` it leaves out the active flag. Every paste and reset is one undo step, and Paste
  Values and Reset reach the other selected entities the same way a field edit does.
- **Clipboards** live together in `Other/Clipboard/`, one per kind of thing: `ComponentClipboard/`,
  `EntityClipboard/` and `AssetClipboard/` (`Editor::Clipboard::Component`, `::Entity`, `::Asset`).
  They hold data, not UI: `Editor::Commands` (Edit menu, entity list context menu, Ctrl+C/V/D) and
  the Properties panel call them and handle selection themselves. The entity clipboard snapshots
  each copied entity with `Entity::SaveData()`, so Cut and delete-after-copy paste safely, and
  pastes through a `Blueprint`. An entity copied together with one of its ancestors is only copied
  as part of that ancestor (`Utilities::Entity::GetTopmost`). `Clipboard::Entity::Duplicate` copies
  and pastes without touching the clipboard. Paste, duplicate and delete are each one undo step. The
  asset clipboard only remembers asset ids so far; nothing pastes assets yet.
- **Entity undo** is `Actions::CreateDeleteEntityCommand`. It snapshots whole hierarchies with their
  entity and component ids (`Actions::EntitySnapshot`) and restores them with those ids, back in
  their place among their siblings and in the entity list, so later history entries that name them
  still apply. It does not restore which camera the game view used.
- **Deleting entities** goes through `Utilities::Entity::DeleteHierarchy` everywhere, the debug
  server included. It drops the entities from the selection, cancels an entity-list drag holding
  one, and clears any rendering context whose scene camera is on one of them, since all three keep
  raw pointers.
- **Audio clip preview**: the Properties panel's audio clip section has a play/stop button and a
  progress bar, built on `Audio::PlayPreview` (see [audio.md](audio.md#previewing-a-clip)).
  `Panels::Properties::Render` stops the preview once the panel is no longer showing that clip.
- **Editor utilities** in `Utilities/`: `Assets/` (build an import context for dropped files, create/delete assets, refresh), `Scripts/` (`Utilities::Script`: create a C# source from the template, delete a script's source, and on window focus hot-reload `<project>/runtime-bin/Game.dll` when its write time changed and the game is stopped). The editor does not compile C#; `Game.dll` is built externally, e.g. by the user's IDE.

## Conventions
- Panels are stateless-ish immediate-mode code: they read/write engine + selection state each frame rather than holding models. Add a panel by creating a `Gui/Panels/<Name>/` folder and registering it in the `Gui` panel loop.
- Editor code never assumes production mode; keep editor-only behavior behind the editor, not in `Engine`.
- **Editor icons (`data/editor/icons/`) are imported as `Uncompressed`, which means linear and
  raw, and must stay that way.** ImGui composites straight into the non-sRGB backbuffer, so
  everything it samples has to already be in display space. An albedo hint would upload them
  with an sRGB internal format, the GPU would decode them to linear on sample, and nothing
  would encode them back - they'd render about a 2.2 gamma too dark. This applies to the gizmo
  icons too: `Gizmo3D` draws them through an ImGui draw list, not into the scene. Their usage
  hint is recorded with `TextureUsageHintSource::User` so a re-import cannot quietly undo it.

## Running it headlessly

How to launch the Editor with no display, usually to drive it through the
[debug server](debug-server.md).

**Use a disposable data directory, never the repo's `data/`.** The editor rewrites
`data/imgui.ini` on exit, and a verification run can write Levels and imported assets into the
project it opened.

```sh
DATA=$(mktemp -d)/data
mkdir -p "$DATA/projects/scratch/assets"
cp -a data/engine data/editor data/imgui.ini "$DATA/"

cd "$DATA"
ALSOFT_DRIVERS=null PINE_X11=1 PINE_DEBUG_SERVER=19100 \
    xvfb-run -a vglrun -d egl /path/to/cmake-build-debug-agent/Editor/Editor scratch
```

- **Copy with `cp -a`, not `cp -r`.** `cp -r` makes every engine source look newer than its
  `.passet`, and the editor then re-imports all of them on boot.
- **Copy `imgui.ini` too.** Without the saved panel layout there is no open viewport, and
  endpoints that need one (`/camera/frame`, `/observe` on the game view) answer 409.
- **The working directory must be the data directory itself.** Engine assets, and
  `engine/script/Pine.dll` for the script runtime, are resolved against the cwd. Getting it wrong
  gives "Failed to load engine assets" and a scripting error, which is easy to mistake for a code
  regression.
- **The project argument is the bare name** (`scratch`). The editor prepends `projects/` itself.
- **`vglrun -d egl` puts rendering on the GPU.** Xvfb has no GPU of its own, so without
  [VirtualGL](https://virtualgl.org) every GL context falls back to Mesa's llvmpipe software
  rasterizer. VirtualGL's EGL back end renders on the GPU and copies each frame into the Xvfb
  window, so UI screenshots and `xdotool` still work. It needs no 3D X server and no
  `vglserver_config`. `nvidia-smi` listing the Editor process confirms it took effect. Leave the
  prefix out on a machine without VirtualGL. The `verify-*.py` recipes launch through
  `headless_command` in `Verification/headless.py`, which adds the prefix when `vglrun` is
  installed.
- `ALSOFT_DRIVERS=null` gives OpenAL a real context with a mixer running at the real sample rate,
  so audio behaves as it would on a sound card. The editor still boots without it.
- `xvfb-run -a` gives a 640x480 screen. To screenshot the ImGui UI itself (which `/observe` and
  `/viewport.png` do not capture), use `xvfb-run -n <display> -s "-screen 0 1920x1080x24"` and grab
  that display, for example with `import -window root` or `ffmpeg -f x11grab`.
- Nothing delivers GLFW window-focus events without a window manager, so anything behind
  `WindowManager::AddWindowFocusCallback` (the editor's `Game.dll` hot reload, for one) cannot be
  triggered headlessly. Call `Script::Manager::ReloadGameAssembly()` from a native probe instead.
- When you are done, stop only the Editor and Xvfb processes you started. A developer may have their
  own running.

Related: [data-and-projects.md](data-and-projects.md) · [rendering.md](rendering.md) · [assets.md](assets.md) · [scripting.md](scripting.md)

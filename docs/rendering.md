# Rendering

How Pine turns a world into pixels. Two layers: a backend-agnostic GPU wrapper
(`Graphics/`) and the engine renderer built on top of it (`Rendering/`). Paths below are
relative to `Engine/src/Pine/`.

## Start here
- `Rendering/RenderManager/RenderManager.{hpp,cpp}` — per-frame orchestrator, called from the main loop as `RenderManager::Run()`.
- `Rendering/RenderingContext.hpp` — a render target + camera; the engine renders a *list* of these (e.g. editor viewport + game camera) each frame.
- `Rendering/Renderer3D/` — the low-level 3D submission API (mesh prep, instanced batching, lights, shadows).
- `Graphics/Graphics.hpp` — entry to the GPU wrapper.

## How it fits together
- **`Graphics/`** is the API seam. `Graphics/Interfaces/I*.hpp` declares the abstraction (`IGraphicsAPI`, framebuffers, shader programs, textures, VAOs, buffers, UBOs); `Graphics/OpenGL/` is the only implementation today (a `Vulkan` enum value is stubbed). Also here: `Graphics/ShaderStorage/`, `Graphics/TextureAtlas/`.
- **`RenderManager`** owns the contexts and the stage model — `RenderStage` (Pre/PostRender, RenderContext, Pre/PostRender2D, Pre/PostRender3D, PostProcessing) and `PipelineStage` (Prepass, Default). External code hooks in via `AddRenderCallback(fn(context, stage, dt))`.
- Per context it runs **`Rendering/Pipeline/Pipeline3D/`** or **`Pipeline2D/`** depending on the context config.
- **`Rendering/SceneProcessor/`** (incl. `SceneLightsProcessor/`) walks the ECS component blocks to gather what to draw and light — this is the bridge from the ECS to the renderer.
- **`Rendering/Features/`** are the pluggable passes: `AmbientOcclusion`, `PostProcessing`, `Shadows`, `Skybox`, `RenderCulling`, `TerrainRenderer`. Shared helpers live in `Rendering/Common/` (`Blur`, `QuadTarget`) and ordering in `Rendering/RenderGraph/`.
- **`Renderer2D/`** mirrors `Renderer3D/` for sprites/tilemaps.

## Notes
- Shaders, materials, meshes and models are all **assets** (see [assets.md](assets.md)); the renderer pulls them from the asset system rather than owning GPU resources directly.
- To add a screen-space effect, add a pass under `Rendering/Features/` and wire it into the pipeline setup, rather than editing `RenderManager` directly.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

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

## Frame order within a context

`RenderManager::Run` calls `Pipeline3D::Prepare()` once (building the draw batch and per-object
light slots), then loops the contexts. For each context it **must** finish preparing that context
before its `Prepass` stage: reset statistics, update the camera, *then* `Run(ctx, Prepass)`, then
`Run(ctx, Default)`.

That ordering is load-bearing. `Prepass` renders the depth buffer that ambient occlusion consumes
and computes the context's visibility, and `Default` consumes both. Updating the camera after the
prepass — as it once was — means shadows, the depth prepass and AO all run on the previous frame's
viewpoint, which shows up as AO lagging behind geometry whenever the camera moves.

## Visibility & culling

Visibility is a property of **(object, frustum)**, not of the object. That distinction is the whole
design:

- `Core/Math/Frustum/` — six inward-facing world planes from *any* view-projection matrix. It takes
  a matrix rather than a camera, so a shadow cascade, a spot cone or a point-light face are all
  valid sources without a variant per caller.
- `Rendering/Features/RenderCulling/` — `Cull(frustum, visibilitySet)` tests every `ModelRenderer`
  and fills a `VisibilitySet`: a bitset indexed by `Component::GetInternalId()` (the pool slot),
  ~512 bytes per frustum at the default `m_MaxObjectCount`.
- **`RenderingContext` owns its `VisibilitySet`.** Two viewports looking different ways cull
  independently. Never store visibility on the component — a single flag there cannot represent more
  than one camera, and both stages of a context read the same set.
- World-space bounds *are* cached per object per frame (`ModelRendererHintData::BoundsMin/Max`,
  filled by `SceneProcessor`) — bounds belong to the object alone, and computing them once beats
  recomputing per frustum. They are built from `Transform::GetPosition/GetRotation/GetScale`, **not**
  `GetTransformationMatrix()`: the matrix is only rebuilt in `Transform::OnRender`, which in
  production mode does not run until `RenderBatch`, after culling needs it.

Shadow passes do not use this yet — they still walk the whole batch with their own distance test.
Wiring them up means a `VisibilitySet` per cascade and per shadow-casting light; no new mechanism.

## Lighting

**Light slot layout.** An object gets a fixed set of light slots
(`Renderer3D::Specifications::ObjectLightSlots`): slots 0-4 are the nearest point lights, slot 5 the
nearest spot. The directional light is global and always light index 0. `SceneLightsProcessor`
assigns slots per object by distance and caches them until a light moves, is added/removed, or
changes type.

⚠ **That layout is asserted in four places with nothing tying them together**: `Specifications.hpp`,
`SceneLightsProcessing.cpp`, `Renderer3D::AddLight`, and the hand-unrolled subscripts in
`generic.fragment.glsl`. Change one and nothing tells you about the other three.

**Direction convention.** `Light.directionToLight` in the UBO points *towards* the light — the
opposite of where the lamp shines — matching `vIn.lightDir[]`. `AddLight` uploads `-forward` to make
that true. The directional light and the spot cone test both rely on it; negating it in a shader
inverts the cone.

**Cone angles** are authored in degrees on the component and converted to cosines at upload, where
the two `smoothstep` edges are also forced apart (equal edges divide by zero in GLSL).

**Ambient is computed once per fragment** (`CalculateAmbientLight`), not per light. Bundling it into
each light's result makes scene brightness scale with how many lights reach an object, and lets it
escape both attenuation and the cone mask.

⚠ **Never index `vIn.lightDir[]` with a variable.** Dynamically subscripting that varying array
returns garbage on some drivers (seen on NVIDIA), silently zeroing N·L. The loops in
`generic.fragment.glsl` are hand-unrolled with literal subscripts for this reason.

⚠ **Terrain and editor gizmos draw with no light hint data**, so they fall back to whatever
`AddLight` left in `Instances[0]` — an arbitrary global subset (first five point lights in pool
order, last spot), identical across the whole terrain. Terrain needs real per-chunk slots.

## Notes
- Shaders, materials, meshes and models are all **assets** (see [assets.md](assets.md)); the renderer pulls them from the asset system rather than owning GPU resources directly.
- To add a screen-space effect, add a pass under `Rendering/Features/` and wire it into the pipeline setup, rather than editing `RenderManager` directly.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

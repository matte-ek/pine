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
- **`Rendering/Features/`** are the pluggable passes: `AmbientOcclusion`, `Bloom`, `PostProcessing`, `Shadows`, `Skybox`, `RenderCulling`, `TerrainRenderer`. Shared helpers live in `Rendering/Common/` (`Blur`, `QuadTarget`), ordering in `Rendering/RenderGraph/`, and the quality presets in `Rendering/GraphicsSettings/`.
- **`Renderer2D/`** mirrors `Renderer3D/` for sprites/tilemaps.

## The shared scene buffers & internal resolution

Contexts don't own scene buffers. There is **one** HDR scene target (`RenderManager`), **one**
depth/normal pre-pass target (`Pipeline3D`), and one set of AO and bloom buffers, all allocated at
the *internal resolution* (`Rendering/InternalResolution/`). A context renders the scene into the
`Size`-sized corner of the shared target, and every pass that reads the scene back scales its
texture coordinates by `Size / InternalResolution::Get()` — the `viewportScale` uniform in
`post-process` and `bloom-extract`. The pre-pass, AO and bloom are the exception: they fill their
buffers edge to edge and are composited at plain texture coordinates.

That arithmetic only holds while a context fits inside the allocation. `RenderManager::Run` takes
the largest active context each frame and calls `InternalResolution::Internal::GrowTo()`, which
rebuilds every registered buffer if it has to. Features that allocate from the resolution register
an `AddResizeCallback` to rebuild themselves.

It only ever grows (a high-water mark for the session), it rounds growth up to a 128-pixel step so a
window or splitter drag rebuilds a handful of times rather than per pixel, and it starts at the
window size so the common one-context-filling-the-window case never reallocates. Memory scales with
the square of the resolution — roughly 75 MB of buffers at 1080p, ~300 MB at 4K.

Note the Editor caps itself separately: `RenderHandler`'s viewport target framebuffers are
hardcoded 1920x1080, and its Game context is deliberately fixed at that size.

## Frame order within a context

`RenderManager::Run` calls `Pipeline3D::Prepare()` once (building the draw batch and per-object
light slots), then loops the contexts. For each context it **must** finish preparing that context
before its `Prepass` stage: reset statistics, update the camera, *then* `Run(ctx, Prepass)`, then
`Run(ctx, Default)`.

That ordering is load-bearing. `Prepass` renders the depth buffer that ambient occlusion consumes
and computes the context's visibility, and `Default` consumes both. Updating the camera after the
prepass — as it once was — means shadows, the depth prepass and AO all run on the previous frame's
viewpoint, which shows up as AO lagging behind geometry whenever the camera moves.

## Draw order & batching

What a pass draws is gathered once per frame into the scene batch (`SceneProcessor`), which is
keyed by (model, override material) and knows nothing about any viewer. What *order* it draws in is
a property of the viewer, so it lives in a `Rendering::DrawList` (`Rendering/DrawList/`) built per
view and consumed by it, the same way a `VisibilitySet` is.

`DrawList::Build` flattens the batch into one item per (mesh, object) that is in the requested
rendering mode and passes the view's visibility, then orders it: `Batched` (the batch's own
grouping, no sort), `FrontToBack` (nearest point of the world bounds first) or `BackToFront`
(furthest centre first, what blending needs). `Pipeline3D::RenderBatch` then walks the ordered list
and emits one instanced draw per **run** of consecutive items sharing a mesh and a material.

**Batching is therefore derived from the order, not imposed on it** — which is the whole point, but
it also means depth order and instancing are in direct competition. `DrawOrdering::DepthBuckets`
is the dial between them: items are quantised into that many bands between the nearest and furthest
of them, so within a band they re-group by mesh and material. Measured on `levels/new-holm` (gm
project) framed from outside, 486 visible objects and 588,534 vertices submitted, counting every
draw the level context issues:

| Pre-pass order | Draw calls |
| --- | --- |
| `Batched` | 147 |
| `FrontToBack`, 4 depth buckets | 254 |
| `FrontToBack`, 16 depth buckets | 474 |
| `FrontToBack`, exact | 1291 |

A level built from a modular kit is the worst case for exact depth order: a few hundred copies of a
few dozen models batch into almost nothing, and sorting them scatters every copy. The sort itself is
not what costs — `DrawList::Build` measures ~0.2 ms for all seven lists in that frame.

### What the order buys back

The other half of the trade is how many fragments each order lets through the depth test, which is
how many a forward pass shades. An occlusion query (`GL_SAMPLES_PASSED`) counts them, and unlike a
clock the count is decided by the geometry and the submission order rather than by the GPU. Same
level, camera at street level looking down the road, 421 visible objects, 1014x627:

| Order | Draws | Fragments shaded |
| --- | --- | --- |
| `Batched` | 73 | 880,873 |
| `FrontToBack`, 4 buckets | 177 | 707,016 (-19.7%) |
| `FrontToBack`, 16 buckets | 380 | 671,168 (-23.8%) |
| `FrontToBack`, exact | 951 | 673,062 (-23.6%) |
| `BackToFront` | 950 | 1,208,285 (+37.2%) |

Two things follow. **Exact depth order is strictly dominated** — 16 buckets shades the same number
of fragments for 40% of the draw calls, so opaque geometry should never be sorted exactly. And the
most the ordering can win is the gap between the 1.87x overdraw this view has and the 1.04x that
perfect front-to-back leaves.

**The shared depth buffer below wins that whole gap for no extra draw calls**, because the pre-pass
already runs every frame. Measured the same way, through the engine's actual scene pass, it shades
636,010 fragments for 635,778 pixels — one shade per pixel, better than a perfect sort and without
touching the submission order. That is why the pre-pass stays `Batched`: ordering it is the more
expensive way to buy a saving that is already taken. What is left to win by ordering is the
pre-pass's own depth writes, where a coarse bucket count is the setting to try.

Two caveats on the table. The figures cover the opaque model batch only — the probe renders it with
the depth shader, without terrain or the discard pass. And the `Batched` row is not a stable
number: the batch is an `unordered_map` keyed by pointer, so its iteration order changes between
runs and with it the overdraw, which measured anywhere from 725k to 880k across runs of the same
build. An unordered pass has no particular cost, it has an arbitrary one. The ordered rows and the
`BackToFront` bound are stable to within a percent.

### The depth pre-pass feeds the scene pass

The pre-pass renders at the **context's viewport**, into the same corner of the shared buffers the
scene pass uses, so the depth it writes lands in the pixels that pass will rasterize. At the top of
`RenderScene` that depth is blitted into the scene framebuffer and the pass draws with
`TestFunction::LessEqual`. Every opaque fragment behind another is then rejected before it is
shaded, at no cost in draw calls.

Three details that are load-bearing:

- **`LessEqual`, not `Equal`.** `Equal` is the tighter test, but it only works for geometry the
  pre-pass actually drew. The pre-pass draws `Opaque` only — it has no alpha test, so a `Discard`
  material rendered there would write depth across its transparent parts and occlude what is behind
  them. Those materials are therefore absent from the pre-pass depth and write their own in the
  scene pass, which `LessEqual` allows and `Equal` would not. Depth writes stay on for the same
  reason.
- **A copy, not a shared attachment.** The scene buffer belongs to `RenderManager`, which clears it
  *after* the pre-pass has run; sharing the texture would mean that clear wiping the depth. The
  pre-pass buffer's depth is the packed `DepthStencil` format purely so the blit is legal — a depth
  blit requires both buffers to hold depth in the same format, and the scene buffer packs a stencil
  the editor's outlines use.
- **Every shader that writes this depth computes `gl_Position` identically.** `depth`, `generic`
  and `terrain` all use `projectionMatrix * viewMatrix * transformationMatrix * vertexPosition`.
  Change one of them and the depth it writes stops matching what the scene pass computes, which
  `LessEqual` turns into missing surfaces rather than an error.

One consequence worth knowing: **ambient occlusion now reads a viewport-resolution buffer.** It used
to read a pre-pass that filled the whole internal resolution — effectively supersampled relative to
the viewport — and it now reads the same corner as everything else, scaling its lookups by a
`viewportScale` uniform the way `post-process` does. Slightly less fine-scale occlusion is found;
measured on the terrain verification scene, the ground came out 1.4% brighter.

**The depth pre-pass is `Batched`**, despite being the pass that would most obviously want
front-to-back: its depth is handed to the scene pass (below), so the shading that ordering would
save has already been saved. Ordering it would only make the pre-pass's own depth writes cheaper,
and on this content that costs more in draw calls than it returns.

Timings in that comparison are not in the table on purpose: they were taken under a software
rasterizer, where repeated runs of an identical build varied by more than the differences being
measured. `GET /stats` on the debug server reports every profiler scope, so the same comparison is
one request on real hardware.

## Materials & transparency

A material's `MaterialRenderingMode` decides which pass draws it, and a draw list is built per
mode. The scene pass runs `Opaque`, then `Discard`, then the skybox, then `Transparent` in
`RenderBlendedObjects`.

The mode is normally not picked by hand: importing a texture measures its alpha channel, and
`Material::ResolveRenderingModeFromDiffuse()` turns that into a mode whenever a material is given a
diffuse map. See [assets.md](assets.md#what-a-texture-does-with-its-alpha).

**The blend pass** builds its list from `ObjectBatchData::BlendObjects` — the objects `SceneProcessor`
saw carrying a transparent material, which is a much smaller set than the scene — ordered
`BackToFront` with **`DepthBuckets` left at 0**. Blending is not commutative, so two surfaces that
swap places composite differently; this is the one pass that has to pay the draw calls an exact sort
costs, and it can afford to because a scene holds far less blended geometry than opaque. It draws
with the depth test on and **depth writes off**: solid geometry still hides a blended surface, but a
blended surface must not reject the ones drawn after it, which in this order are the ones in front
of it.

The skybox is drawn **before** this pass rather than last, because it is what a transparent surface
with nothing solid behind it blends against.

**The alpha itself** is carried by the material. `Material::GetAlpha()` reaches the shader through
`ShaderStorages::MaterialProperties`, which mirrors `MaterialProperties` in
`shaders/3d/shared/common.glsl` member for member — a field added to one has to be added to the
other in the same position, or every member after it reads the wrong std140 offset. Only the
generic shader's `VERSION_TRANSPARENT` variant writes that alpha out; every other version writes a
constant 1, because `post-process.fragment.glsl` forwards the scene buffer's alpha to the final
image and the editor composites that image with blending on.

Three limits worth knowing:

- **Sorting is per object-mesh, by centre distance.** Two transparent surfaces that interpenetrate,
  or a single mesh that overlaps itself, composite in whatever order their centres imply. That is
  the usual limitation of a sorted blend pass and the reason engines keep transparent geometry
  simple.
- **Transparent surfaces cast no shadows.** `ShadowPass` renders `Opaque` and `Discard` only.
- **Membership is decided per object, in `SceneProcessor`.** It resolves each mesh's material the
  same way the draw list does, override material included — a renderer made transparent by its
  override is what the blend pass would otherwise silently miss.

### Verification

```sh
python3 Editor/src/DebugServer/Verification/verify-blending.py --build build
```

Nothing creates a material over HTTP and `/edit` cannot make one transparent, so this builds a probe
out of the Editor's boot sequence — the `verify-physics-native.py` pattern — and puts a red cube in
front of a green one. It then reads the same centre pixel three times: with the near cube opaque,
semi-transparent at alpha 0.5, and removed. A blend is the only thing that lands between the other
two readings, and the reading is taken off the level viewport's own framebuffer rather than through
a capture endpoint. The near cube is coloured through an **override material**, which is the path
that decides blend membership and the one that was wrong when the pass was first written. The same
run asserts a `DrawList` comes out descending for `BackToFront` and ascending for `FrontToBack`.

The per-phase line it prints carries the draw-call count, which is the cheapest signature of the
blend pass working: two objects cost four draws with both opaque (pre-pass and scene pass each), and
three once the near one is blended, since the blend pass draws it and the pre-pass does not.

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

Shadow passes use the same mechanism: every `ShadowView` owns a `VisibilitySet` and is culled
against its own frustum, so there is no second visibility concept to keep in sync.

**`RenderingContext` also owns its `ViewFrustum`**, built in the prepass and read by both stages.
The set answers "is *this component* visible"; the frustum is there for anything that culls at a
finer grain than a component. Terrain is the reason it exists: a terrain has hundreds of chunks
under one component id, which a bitset indexed by that id cannot represent, so `TerrainRenderer`
tests chunk bounds against this frustum inline instead. Anything else needing the viewer's frustum
should read it here rather than rebuild it — a second `FromViewProjection` call is a second
expression that has to keep agreeing with the first.

## Shadows

Everything decomposes into **`ShadowView`** (`Features/Shadows/ShadowView/`): one projection, its frustum,
a slice of a render target, and a `VisibilitySet`. A directional light is `CASCADE_COUNT` views, a
spot is one, a point light is six cube faces — so the build/cull/render loop has no per-light-type
branch left in it.

Views land in a **`ShadowAtlas`** (`Features/Shadows/ShadowAtlas/`): one depth texture partitioned
into tiles (`Half`/`Quarter`/`Eighth` of the atlas edge), handed out by importance and capped at
`Specifications::Shadows::SHADOW_VIEW_COUNT` live views. One texture means one sampler
(`Samplers::SHADOW_ATLAS`); a light keeping the *same* tile across frames is what makes caching
possible, and a tile whose contents are still correct is not re-rendered.

**The feature is split by what each part decides** (all under `Features/Shadows/`):

- **`ShadowTileSelection/`** — which lights cast, at what size, and what it costs them: importance,
  incumbency, the challenger margin, the minimum residency, the promotion/demotion thresholds and
  the fade. Everything hysteretic is here, because those knobs are only correct read against each
  other.
- **`ShadowAtlas/`** — the allocator: tiles in, tiles out, mark-and-sweep. Knows nothing about lights.
- **`ShadowCascades/`** — fits the directional cascades to the camera frustum, into its own pinned
  tiles.
- **`ShadowPass/`** — the one place shadow depth is drawn, for cascades and local views alike.
- **`Shadows.cpp`** — the per-frame loop that drives them, plus the spot and point views, the tile
  cache and the statistics.

**Where each kind runs is not the same, deliberately** (`Pipeline3D`):
- **Local views** (spot, point) are built and rendered in `Pipeline3D::Prepare()`, **once per
  frame** — `Shadows::PrepareLocalViews` / `RenderLocalViews`. Their maps do not depend on the
  viewer, so doing this per context would render every spot light twice with an editor viewport and
  a game camera both live.
- **Cascades** are built from the camera frustum, so they stay **per context**, inside that
  context's prepass — `Shadows::NewFrame(camera)` then `RenderPassLight(light, ...)` per light.

**Terrain is drawn into every view** alongside the object batch, inside `ShadowPass::Render`. It is not in
the batch, so it needs its own call, and it culls its own chunks against the view's frustum rather
than reading the view's `VisibilitySet` — which cannot hold them (see
[Visibility & culling](#visibility--culling)). Each view carries an `Origin` for this: the light for
a spot or point view, and the *scene camera* for a cascade, which has no origin of its own. That is
what makes a chunk cast the shadow of the silhouette it is actually drawn with, rather than of a
coarser level the main pass never shows.

⚠ **Terrain overrides the view's face culling and bias while it draws.** A height field is
single-sided — one surface per column, no far side — so the front-face culling a cascade uses to buy
its separation for free would discard the ground itself and leave only the chunk skirts writing
depth. Terrain draws with back faces culled and an explicit bias pair instead, which is the trade a
local view already makes for everything.

A cached tile is also invalidated when a terrain moves, is reshaped, or is added or removed
(`SceneProcessorContext::TerrainChanged`, written by `TerrainRenderer::Prepare`). Coarser than the
per-caster `MovedCasters` test next to it, deliberately: terrain changes are rare, and a chunk is
not something that list can hold. Without it, a tile drawn before a terrain was assigned to its
component would stay cached and the ground would never appear in it.

The cascades' near plane is fitted to the casters that can reach the cascade box (`FitCasterNearZ`),
and **terrain chunks are part of that fit**. Terrain is usually the tallest thing in a level, and a
ridge outside the box still throws a shadow across ground inside it.

With shadows switched off, `Pipeline3D` calls `Shadows::ClearLocalViews` rather than simply
skipping the work: a light still pointing at the tile it held would otherwise keep sampling a tile
nothing refreshes, freezing its shadow in place instead of removing it.

`Shadows::GetStatistics()` and `GetTileDebugInfo(slot)` back the editor's atlas debug view — tile
residency, importance and cache hits are invisible in the final image when they work and obvious
there when they don't.

The design reasoning, including what was rejected and why, is in
[`reports/spot-point-light-shadows-plan.md`](reports/spot-point-light-shadows-plan.md).

## Lighting

**Light slot layout.** An object gets a fixed set of light slots
(`Renderer3D::Specifications::ObjectLightSlots`): slots 0-4 are the nearest point lights, slots 5-6
the nearest two spots — `COUNT` is 7. Two spot slots rather than one so a hand-held light and a
world light can reach the same surface. The directional light is global and always light index 0.
`SceneLightsProcessor` assigns slots per object by distance and caches them until a light moves, is
added/removed, or changes type.

⚠ **`COUNT` is capped at 7 by the shader, not by anything in C++.** The generic shader's varying
block carries `lightDir[8]`, of which `[0]` is the directional light and `[1..7]` are these slots.
Going past 7 means growing that array and costs three interpolated floats per fragment on *every*
material, so 6→7 was free in a way 7→8 is not.

⚠ **The layout is asserted in several places with nothing tying them together**:
`Specifications.hpp`, `SceneLightsProcessing.cpp`, `Renderer3D::AddLight`, and the hand-unrolled
subscripts in both `generic.vertex.glsl` and `generic.fragment.glsl`. Change one and nothing tells
you about the others.

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

Slots live in a **`Renderer3D::LightSlotData`** (`Renderer3D/LightSlotData.hpp`), which is what
`AddInstance`/`RenderMesh` take and all that they read. A `ModelRenderer` carries one inside its
hint data; a terrain chunk carries one of its own. `SceneLightsProcessor::AssignSlots(context,
position, slots)` is the shared rule — it takes a point rather than the thing at that point, because
a chunk is not a component and has no transform to be lit at.

⚠ **Editor gizmos draw with no light hint data**, so they fall back to whatever `AddLight` left in
`Instances[0]` — an arbitrary global subset (first five point lights in pool order, last two spots)
and its shadow views with it. Terrain no longer does; see below.

## Terrain

Terrain does **not** go through the object batch. `Rendering/Features/TerrainRenderer/` draws it
directly, and `Pipeline3D` calls that feature from inside both `RenderDepthPrepass` and
`RenderScene` rather than from `RenderBatch`. The reason is the shader override: a shadow or depth
view renders the batch with `OverrideShader` set, and terrain has to honour that override the same
way, which it can only do if it is submitted while the override is in place.

`TerrainRenderer::Render` takes a **`TerrainView`** — a frustum, a point to measure detail from, and
somewhere to put the chunk counters — rather than a `RenderingContext`, because a shadow view is not
one. A context passes its camera and its own statistics; a shadow view passes its `Origin` and none.

The chunk meshes themselves are built by `TerrainRenderer::Prepare()`, called once per frame from
`Pipeline3D::Prepare()` — not from a draw pass, and not per context. A chunk mesh is derived from
the terrain's height field and from nothing about the viewer, so building it per context would
repeat identical work for every viewport, and building it mid-pass (as the old implementation did)
puts an unbounded amount of work inside the depth prepass. `Terrain` owns the meshes and rebuilds
only the chunks it has marked dirty; a terrain that no component references is never built at all.

A rebuild **re-uploads the existing mesh** rather than replacing it, whenever the vertex count is
unchanged — which it is for every rebuild a sculpting stroke causes, since a level's vertex and
index counts follow from the chunk's quad count alone. That is what `Mesh::UpdateVertices` and its
siblings are for, and it matters because a stroke redirties chunks every frame it is dragged:
building a fresh `Mesh` allocates a vertex array and five GPU buffers, and `IVertexArray` only frees
those when it is disposed. The index buffer is never re-uploaded at all — the triangles of a level
are fixed, so the same indices describe the moved vertices.

Every chunk carries **one mesh per detail level** (`Terrain::MaximumLodCount`, four at the default
64 quads per chunk). Level *l* keeps every 2^*l*-th sample, so a coarse mesh is a *subset* of the
fine one rather than a resampling of it — the chunk's edge samples survive to the coarsest level,
and two neighbours drawn at the same level meet exactly. Normals still come from the neighbouring
samples rather than from the level's own vertices, so ground does not change shade as it crosses a
switch distance. `TerrainRenderer::Render` picks the level from the distance to the chunk's bounds,
with the switch distances derived from `GetChunkSize()` so that coarse and fine terrains switch at
the same apparent size.

Neighbours at *different* levels do not meet, and the gap is closed with a **skirt**: a vertical rim
around each chunk hanging down by that chunk's own height range, carrying the edge vertices' normals
and uvs so it reads as a continuation of the ground rather than as a wall. The chunk's height range
is a provable bound — a coarse neighbour can only miss the shared edge by as much as that edge rises
and falls. The skirt is deliberately *not* included in the mesh AABB: bounds are what culling tests,
and a chunk whose rim is visible but whose ground is not has nothing worth drawing.

⚠ Terrain chunks are counted in `RenderingStatistics` as `Visible/CulledTerrainChunkCount`, separate
from the object counts — a whole terrain is one object, and folding its chunks in would make
"visible objects" mean something different for a level that has terrain in it. Both are cleared per
pass by `TerrainRenderer::BeginPass`, because terrain culls in each pass rather than once per frame.

**Lighting is per chunk.** `TerrainRenderer::Prepare` gives every chunk its own `LightSlotData`,
assigned from the centre of the chunk's box by the same `SceneLightsProcessor::AssignSlots` that
lights a model renderer. Per chunk rather than per terrain because a terrain is far too large to be
lit at one point — the whole ground would take the five lights nearest its middle and nothing else.
Chunk granularity is still coarse: a 64-unit chunk gets one set of five point and two spot lights.

The slots survive between frames and are recomputed when the light set changes
(`SceneProcessorContext::LightSetChanged`), when the terrain moves, or when a chunk's box moves —
the last raised from `Terrain::UpdateChunkBounds`, so the sculpting brush gets it without having to
remember. Terrain movement is compared against the position the slots were assigned at
(`TerrainRendererComponent::GetLightSlotOrigin`) rather than read off `Transform::IsDirty()`: no
pass calls `OnRender` on a terrain's transform, so that flag is never cleared.

⚠ The slots live on the chunk, which lives on the **asset**. Two entities sharing one terrain asset
would therefore light it from whichever placement was processed last. One terrain per placement is
the assumed case; `m_LightSlotOrigin` is on the component so that assumption is visible.

**The surface is four blended layers, not one material.** Every sample of the height field also
carries four weights (`Terrain::GetSampleWeights`), normalized so they sum to one, and each weight
belongs to one of `Terrain::MaximumLayerCount` layer slots holding an ordinary `Material`.
`Renderer3D::PrepareTerrainChunk` binds all four layers' diffuse, specular and normal maps at once —
`Specifications::Samplers` already reserves four units per texture type — writes their colours,
shininess and uv scale into `Properties[0..3]` of the material buffer, and selects
`engine/shaders/3d/terrain`, whose `CreateSurface()` is the only thing about it that differs from
`generic`. A layer with no material bound draws as plain white, and one with no normal map is bound
a flat default normal texture rather than being branched around.

The weights reach the shader as **one RGBA8 texture for the whole terrain**, one texel per sample,
rebuilt by `Terrain::RebuildDirtySplatMap()` from the same `Prepare()` that rebuilds the chunk
meshes. One texture rather than one per chunk for the same reason the height field is shared: two
neighbouring chunks read the same edge samples, so the blend filters across a chunk edge exactly as
it filters anywhere else, with no duplicated border texels to keep in step. `GetSplatTransform()` is
what maps a chunk mesh's terrain-local uv onto it, and it lands a sample on the centre of its own
texel — half a texel out and the ground shows a blend of the wrong two samples.

⚠ Repainting a terrain does **not** set `SceneProcessorContext::TerrainChanged`. That signal exists
to invalidate cached shadow tiles, which hold depth; weights change what the ground looks like and
not what shape it is.

See [`plans/terrain-system.md`](plans/terrain-system.md) for what the remaining units add — physics,
layer blending and the sculpting brush are done, layer painting is not.

### Verification

After building Editor with Ninja, run (requires Xvfb):

```sh
python3 Editor/src/DebugServer/Verification/verify-terrain-render.py --build build
python3 Editor/src/DebugServer/Verification/verify-terrain-lighting.py --build build
python3 Editor/src/DebugServer/Verification/verify-terrain-layers.py --build build
python3 Editor/src/DebugServer/Verification/verify-terrain-sculpt.py --build build
```

`/edit` has no TerrainRenderer operation and nothing creates an asset over HTTP, so this builds a
probe out of the Editor's own boot sequence — the `verify-physics-native.py` pattern — puts a
noise-filled terrain in a scene and then hands over to the normal main loop, so the rest of the
checks run over the debug server. In process it checks the chunk meshes, their index counts and
bounds, that an edited sample dirties the chunks on *both* sides of a chunk edge, and that
`GetHeightAt` interpolates across the same triangle the mesh generator builds. Over HTTP it checks
that the render path built the meshes, that the terrain fills the frame with no sky showing through
it, that its shading actually varies, and that every chunk draws in both passes.

The LOD and culling checks are numeric rather than visual. Terrain is the only thing drawing in that
scene, so `/stats` dividing `vertexCount` by `drawCalls` says which level the chunks were drawn at:
backed off past the last switch distance every draw has to cost exactly the coarsest level, and
standing on the terrain the frame has to cost more than an entirely level-1 one. Culling is checked
in both directions — every chunk visible looking at the terrain, every chunk culled and *zero draw
calls* looking away, which is what separates real culling from a counter that is merely reported.
A second script, `verify-terrain-lighting.py`, covers the lighting and the shadows. It builds the
same kind of probe over a *flat* terrain with one square plateau on it — flat because the shadow it
looks for has to be attributable, and on noise a dark patch could just as easily be shading. It puts
eight lamps over five point slots, then checks that every chunk kept exactly the five nearest to its
own box and that moving a lamp moves the slots with it. The shadows are checked by turning the sun's
`CastShadows` off and counting the pixels that brighten: terrain is the only thing in that scene, so
a frame that does not change is a terrain that never reached the shadow pass. The count has an upper
bound as well as a lower one — ground that comes back uniformly darker is acne, not a shadow. The
same A/B then runs for a point light, which reaches the atlas through the cached local-view path.

The skirts are checked by looking for sky below the skyline in a ground-level frame: a height field
seen from above its surface has ground under every pixel once a column has hit it, so a sky pixel
down there is a chunk edge showing through. Removing the skirt makes that frame fail, which is the
control that keeps the check from being vacuous.

It uses the engine's default material and no project assets, so it runs against a bare project
directory. The shading check is a block-averaged local contrast rather than a brightness range:
per-pixel grain hides a gradient and the vignette imitates one, and flat normals — the bug this
whole rewrite starts from — sit comfortably inside a naive threshold.

A third script, `verify-terrain-layers.py`, covers the blending. It paints the weight field as a
*bilinear* function of position — layer 0 owning one corner, 1, 2 and 3 the other three — over
**flat** ground lit straight down, so that the only thing varying anywhere in the frame is the
blend. A gradient rather than four painted quadrants, deliberately: four blocks would still look
plausible if the splat lookup were scaled, offset or mirrored, because the blocks would simply land
elsewhere and nothing in the picture would say so. A field that varies everywhere cannot survive
that, and the script re-derives the expected colour at a point rather than looking for a shape.

It projects its probe points with the `viewMatrix`/`projectionMatrix` that `/observe` reports for
the very frame it decoded, rather than guessing the mapping from where the camera was put — the
point of naming positions is to catch a lookup landing in the wrong place, which a hand-guessed
mapping could hide. The pixels are compared as an *ordering* of channels rather than as colours:
everything between the blend and the pixel (the light, the ambient term, the sRGB encode, the
vignette) is monotonic per channel, so which channel is brighter survives it all while the values do
not. The control that keeps this from passing on any picture at all: a terrain that ignored the
splat map would carry all four layers evenly and come out grey, failing every comparison. Two
further probes sit inside a *single* chunk and must disagree, which is what "four layers blend
across a chunk" actually asks for.

### Skirts and shadow views

A chunk's skirt is drawn in every pass that has to agree with what is on screen, and in **no** pass
that decides what light reaches the ground. `TerrainView::DrawSkirts` is what says which, and the
shadow views are the only caller that turns it off; the draw then covers
`Terrain::GetChunkGroundIndexCount(level)` indices instead of the whole mesh, which works because
the skirt is appended *after* the ground in the index buffer.

The reason is that a skirt is a vertical rim hanging below a chunk's edge, as deep as that chunk's
own height range, whose only job is to hide the crack between two neighbours drawn at different
detail levels. It is not ground. Letting it write depth turns every chunk boundary into a wall, and
a terrain lit from a low angle then shows a hard dark band along every chunk edge — a cross over a
2x2 terrain — which is exactly what it looked like in practice before this was fixed.

What that costs is the crack the skirt was hiding, now in the shadow map rather than on screen: a
thin seam where two neighbours at different levels meet, letting a little light through. Shadow
views pick their detail levels from the same origin the main pass does, so neighbours differ by at
most one level and the seam is correspondingly small. A seam is a far better trade than a wall.

### The brush overlay

The ring under the editor's sculpting brush is drawn by the terrain's own fragment shader, behind
`VERSION_BRUSH` — a shader version nothing in a built game ever requests, so it is never compiled
there. `TerrainRenderer::SetBrushOverlay` names the terrain, a terrain-local centre and a radius;
`PrepareTerrainChunk` picks the variant and uploads them. The distance is measured in the xz plane,
which is the same thing the brush uses to decide which samples it covers — so what is highlighted is
what will move, and it follows uneven ground because the ground's own fragments are what draw it.

One overlay for the whole renderer rather than one per `RenderingContext`, because there is one
cursor. A Game viewport open beside the Level viewport therefore shows the ring too: a cosmetic
oddity in an editor-only path, and not worth a field on every context.

A fourth script, `verify-terrain-sculpt.py`, covers the brush. Its native half is the part with no
HTTP surface: `Terrain::Raycast` against `GetHeightAt` straight down over the whole field, obliquely
from a ring of thirty-two directions, and aimed exactly at grid vertices — the case where a triangle
test with no edge tolerance is rejected by all eight triangles sharing the corner and the ray falls
through the ground. It also round-trips a height rectangle, because "undo restores the ground
exactly" is that round trip and nothing else.

Its HTTP half sculpts **flat** ground, so that how far the brush moved something is measured against
zero rather than against noise that already varies by more than the stroke does. It checks the
brush's shape (falling off from the centre at full falloff, flat topped with none, radially
symmetric, and nothing outside the radius including the corners of the rectangle the brush reads),
each of the four modes, and that one stroke is one undo step whose undo restores the probed heights
*identically* rather than approximately. A twenty-four point drag checks the part most likely to be
wrong — a stroke growing its recorded region must keep the heights it first recorded, not re-read
ground it has already moved — and a stroke off the terrain has to record nothing at all, or it would
swallow the author's next undo. Smoothing is checked on a one-sample spike in an untouched corner,
and has to *both* bring the peak down and raise the ground beside it: a brush that only pushed
samples down would pass half of that.

The overlay gets its own pair of frames: one over the ring and one over ground far from it,
counting strongly warm pixels in each. On this scene the ring reaches a red-blue difference of 38
while the brightest thing anywhere else reaches 5, so the threshold sits clear of both and the
second frame is the control that stops it passing on a merely warm picture. Before that, natively,
the check that actually pins the shader machinery down: the `brushRing` uniform has to exist in the
variant and **not** exist in the default one. A version that was never registered still compiles —
into a second program built from unchanged source — so a shader that silently lost `VERSION_BRUSH`
would draw a perfectly good terrain and no ring, which no screenshot would obviously fail on.

The cursor ray gets a native check too, since nothing over HTTP can move a mouse: a camera is aimed
at a raised half of the terrain, and the ray through the centre of the viewport has to land where
the camera's own forward meets the step — position and height, not just height. Rays right of and
below centre have to move the hit the way the cursor moved. Together those catch a flipped y or a
transposed matrix, which otherwise show up only as the brush landing somewhere other than under the
cursor.

The last check is that the drawn mesh followed the field. A sculpted terrain and a flat one have
identical vertex counts, so what says the meshes were rebuilt is that no chunk is still marked
dirty — `Prepare` clears that only once it has rebuilt the chunk — and that some chunk's bounding
box has grown taller.

## Notes
- Shaders, materials, meshes and models are all **assets** (see [assets.md](assets.md)); the renderer pulls them from the asset system rather than owning GPU resources directly.
- To add a screen-space effect, add a pass under `Rendering/Features/` and wire it into the pipeline setup, rather than editing `RenderManager` directly.

Related: [world-ecs.md](world-ecs.md) · [assets.md](assets.md)

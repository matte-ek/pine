# Terrain system — current state and a plan for the rewrite

Status: report / proposal. Nothing here is implemented.

**Recommendation up front: rewrite the implementation, keep the architecture.** The five seams the
current terrain code occupies (a `Terrain` asset, a `TerrainRenderer` component, a
`Rendering/Features/` pass, a `PhysicsTerrain` cooker, a `Collider` hook) are the right shape and
match how every other subsystem in Pine is put together. What sits inside them — the data model,
the mesh generation, the draw path and the physics coupling — is not salvageable, and several parts
of it are actively broken in ways that explain why the result looked bad rather than merely
unfinished.

The good news for the rewrite: **there is no migration burden.** No `.ter` source files exist under
`data/`, no terrain `.passet` exists in any project, and the modular `Terrain-*.glb` tile kit the
`gm` project used to build its ground from has been removed (see [Decisions](#decisions) 3). The
serialized format can change freely. Worth confirming against any project you keep outside the
repo.

---

# Part 1 — What Pine does today

## The pieces

| Piece | Path | Size |
|---|---|---|
| `Terrain` asset | `Engine/src/Pine/Assets/Terrain/` | 445 lines |
| `TerrainRendererComponent` | `Engine/src/Pine/World/Components/TerrainRenderer/` | 64 lines |
| Render feature | `Engine/src/Pine/Rendering/Features/TerrainRenderer/` | 90 lines |
| PhysX cooking | `Engine/src/Pine/Physics/Physics3D/PhysicsTerrain/` | 80 lines |
| Collider hook | `Collider.cpp:201-246` | — |
| Editor UI | `AssetPropertiesRenderer.cpp:506`, `ComponentPropertiesRenderer.cpp:735`, `AssetDialogs.cpp:66` | — |

Registration is complete and correct: `AssetType::Terrain` with the `.ter` import extension
(`Assets.cpp:62`), `ComponentType::TerrainRenderer` with a data block of 32
(`Components.cpp:121`), and the mirrored enum entry in `ScriptRuntime/World/Component.cs:10`. There
is **no** debug-server support for terrain.

## The data model

```cpp
constexpr int TERRAIN_CHUNK_SIZE         = 64;    // world units per chunk edge
constexpr int TERRAIN_CHUNK_VERTEX_COUNT = 256;   // samples per chunk edge
constexpr int TERRAIN_SQUARE_SIZE        = (256 + 2) * (256 + 2);   // 66564

struct TerrainChunk
{
    Vector2i Position;
    std::array<float, TERRAIN_SQUARE_SIZE> HeightData;   // 260 KB, by value
    AssetHandle<Material> ChunkMaterial;
    Mesh* ChunkMesh = nullptr;
    Mesh* ChunkMeshLowPoly = nullptr;
    TerrainChunkPhysicsData PhysicsData;
    bool IsReady = false;
};

std::vector<TerrainChunk> m_Chunks;   // Terrain.hpp:66
```

Three things to notice about this, because they drive most of what follows:

- **A chunk is 260 KB inline in a vector.** `CreateChunk` push_backs by value, so adding a chunk
  reallocates and memcpys every existing chunk — including its raw `Mesh*` and PhysX pointers,
  which nothing owns safely.
- **Persisted and runtime state are mixed in one struct.** The serializer
  (`Terrain.hpp:76-80`) writes `Position` and `HeightData` and nothing else — so the chunk
  material you pick in the editor is silently lost on reload.
- **The `+ 2` border is never used.** Every writer and every reader indexes with stride
  `TERRAIN_CHUNK_VERTEX_COUNT` (256), not 258. The border was presumably meant to carry neighbour
  samples for seamless normals; it is 2064 dead floats per chunk instead.

At 256 samples per 64-unit edge, vertex spacing is 0.25 units: 65,536 vertices and 130,050
triangles per chunk, roughly 3.6 MB of GPU buffers per chunk before the low-poly copy. That is very
dense for a general-purpose engine.

## What is broken

These are read off the code, not inferred from behaviour.

**1. Every terrain normal is straight up.** This is the big one.

```cpp
// Terrain.cpp:129-142
template<size_t TerrainSize>
float GetHeight(const std::array<float, TerrainSize>& heightMap, int x, int z)
{
    ...
    int arrayIndex = z * TerrainSize + x;              // TerrainSize is 66564, not 258
    if (arrayIndex >= heightMap.size())
        arrayIndex = heightMap.size() - 1;             // so this clamp fires for every z >= 1
    return heightMap[arrayIndex];
}
```

`TerrainSize` is deduced from the array's **element count**, not its row width. `ComputeNormal`
(`Terrain.cpp:145`) is always called with `z + 1 >= 1`, so all four neighbour samples clamp to the
same last element, every difference is zero, and the function returns `normalize(0, 2, 0)` — flat
up — for every vertex on the terrain. Lighting cannot show any shape at all. If the terrain looked
like a flat-shaded blob regardless of the heights, this is why.

**2. The LOD meshes do not tile, and the terrain sits one unit off its transform.** The
full-resolution chunks *do* tile. That is worth stating plainly, because the three constants
involved read as though they disagree:

- Perlin sampling advances `TERRAIN_CHUNK_VERTEX_COUNT - 1` = 255 samples per chunk
  (`Terrain.cpp:319`)
- vertices span 0..64 world units at 64/255 spacing (`Terrain.cpp:174-176`)
- the renderer places chunks at `Position * TERRAIN_CHUNK_SIZE - 1` (`TerrainRenderer.cpp:23-26`) —
  which by precedence is `64n - 1`, so 64 units apart, not 63

Chunk *n*'s sample 255 and chunk *n+1*'s sample 0 are both Perlin coordinate `255(n+1)`, and both
land at the same world position. The high-detail edges agree exactly. What is actually wrong is
smaller, but still has to be fixed:

- **The `- 1` is a constant shift of the whole terrain.** Almost certainly a typo for
  `(TERRAIN_CHUNK_SIZE - 1)`. Because it applies to every chunk equally it produces no seam; it just
  puts the terrain one unit off its entity's transform on both axes. Had it been written the way it
  looks, *that* would have been a genuine tiling bug.
- **The low-poly meshes crack at chunk edges.** `DownscaleHeightMap` averages 4x4 blocks
  (`Terrain.cpp:114-125`), so chunk *n*'s last LP sample averages high-detail samples 252-255 while
  chunk *n+1*'s first LP sample averages its own 0-3. Those are different numbers, and nothing
  reconciles them.
- **The LP mesh also slides under the high-detail one.** LP vertex *x* sits at `x * 64/63`, while
  the block it averages is centred on `(4x + 1.5) * 64/255`. The two agree near the middle of a
  chunk and drift apart towards its edges, so the ground shifts whenever a chunk changes LOD.

Underneath all three: each chunk owns a private copy of its samples, so nothing structurally
guarantees agreement. The high-detail case lines up because two independently written expressions
happen to evaluate to the same number.

**3. Terrain is effectively unlit.** `RenderChunk` calls `Renderer3D::RenderMesh(transform)` with no
hint data, so the instance light indices are whatever the previous batch left in `Instances[0]`.
[`rendering.md`](../rendering.md#lighting) already flags this: terrain gets an arbitrary global
subset of lights, identical across the whole terrain, and now arbitrary shadow views with it.

**4. Terrain casts no shadows and is never culled.** `RenderTerrain()` runs in the depth prepass and
the main scene pass only (`Pipeline3D.cpp:74,119`), never in `Shadows::RenderLocalViews` or
`RenderPassLight`. And `RenderCulling::Cull` walks `ModelRenderer` only, so every chunk of every
terrain is submitted every frame in every context with no frustum test.

**5. LOD switches at 55 units.** `distance2(...) > 3000.f` (`TerrainRenderer.cpp:31`) is a squared
distance, so the low-poly mesh takes over at √3000 ≈ 54.8 units — less than one chunk width away
from the camera.

**6. A crash path when shadows are off.** `TerrainRenderer::NewFrame(context.SceneCamera)` is called
inside `if (m_Configuration.RenderShadows)` (`Pipeline3D.cpp:329-332`), but `Render()` runs
unconditionally and does `assert(m_SceneCamera)` then dereferences it. Turn shadows off and terrain
asserts in debug, null-derefs in release.

**7. Mesh generation runs from inside the draw pass.** `TerrainRenderer::Render` sees
`!chunk.IsReady` and calls `terrain->GenerateMesh()` (`TerrainRenderer.cpp:64`), which regenerates
*every* chunk — dispose + `new` both meshes, rebuild all vertex data, cook a PhysX heightfield,
upload to the GPU — in the middle of the depth prepass. Any chunk edit re-does all of it.

**8. The PhysX heightfield leaks and is half-destroyed.**

```cpp
// PhysicsTerrain.cpp:61-65
void Physics3D::Terrain::Destroy(TerrainChunkPhysicsData* data)
{
    free(data->PhysicsHeightFieldData);
    data->PhysicsHeightFieldData = nullptr;
}
```

The `PxHeightField` itself is never `release()`d and `PhysicsHeightField` is never nulled — so
`Terrain::Dispose`'s `if (PhysicsHeightField != nullptr)` guard tests a dangling non-null pointer.
Also `static PxHeightFieldSample samples[65536]` (`PhysicsTerrain.cpp:13`) is a 256 KB function-local
static, so cooking is not re-entrant.

**9. The collider binds exactly one chunk, in the wrong place.**

```cpp
// Collider.cpp:208-213
for (const auto& chunk : terrainChunks)
    geometry.heightField = static_cast<physx::PxHeightField*>(chunk.PhysicsData.PhysicsHeightField);
```

The loop overwrites; the last chunk wins. Its `localPose` is `(-32, 0, -32)` (`Collider.cpp:242`)
while the mesh spans 0..64, and `rowScale`/`columnScale` are `64/256` against the mesh's `64/255` —
so even that one chunk's collision does not line up with what you see.

**10. Leftovers from the July "Beginning terrain rework" commit (`ca2edaa`).**

- The editor's "Generate All" button does nothing but mark the asset modified: its body is a
  commented-out call to a `Terrain::LoadHeightMapData()` that no longer exists anywhere in the tree
  (`AssetPropertiesRenderer.cpp:533`).
- The `GetHeightmapData` helper (`Terrain.cpp:19`) is never called.
- `m_HeightMap` is a public member, not serialized, and the editor's picker for it discards its
  result: `const auto HeightMap = Widgets::AssetPicker(...)` with nothing reading `HeightMap`
  (`AssetPropertiesRenderer.cpp:510`).
- `generic.ih` declares a `VERSION_TERRAIN` shader version with bit value `2`, which no GLSL
  `#ifdef` consumes and which **collides with** `ShaderVersions::Generic::PerformanceFast`
  (`Specifications.hpp:88`).

**11. One discrepancy worth resolving, not a bug.** The comment above `RenderTerrain`
(`Pipeline3D.cpp:33-37`) says terrain must stay out of `RenderBatch` because it "ignores
Renderer3D's shader override". That does not match `Renderer3D::PrepareMesh`, which honours both
`OverrideShader` and `SkipMaterialInitialization` on exactly the path terrain uses — the depth
prepass depends on it. The reason that *does* hold is the second one in the comment: terrain picks
its LOD from `m_SceneCamera`, so drawing it from a shadow view would pick the viewer's LOD. Worth
getting straight when terrain joins the shadow passes.

## Verdict

Keep: the asset/component/feature/physics decomposition, the `AssetType` and `ComponentType`
registrations, and the idea of chunks with LOD levels.

Replace: the height representation, mesh and normal generation, chunk placement, the draw path, the
material story (there isn't one), and the whole physics coupling.

---

# Part 2 — What the new system needs

## 2.1 The data model — the decision that is expensive to change later

This is the one thing worth getting right before any code is written, because it is what the asset
format, the sculpting tools and the seam behaviour all hang off.

**Recommendation: store the terrain as one shared height field, and make chunks views into it.**

```cpp
class Terrain : public Asset
{
    // --- persisted ---
    Vector2i m_ChunkCount   = { 4, 4 };   // chunks in x / z
    Vector2i m_ChunkOrigin  = { 0, 0 };   // grid coord of chunk [0,0]; see "growable" below
    int      m_ChunkQuads   = 64;         // quads along a chunk edge
    float    m_ChunkSize    = 64.f;       // world units along a chunk edge
    float    m_HeightMin    = -64.f;      // the range m_Heights maps onto
    float    m_HeightMax    =  64.f;

    // (m_ChunkCount.x * m_ChunkQuads + 1) * (m_ChunkCount.y * m_ChunkQuads + 1) samples
    std::vector<std::uint16_t> m_Heights;

    std::vector<TerrainLayer>  m_Layers;         // see 2.3
    std::vector<std::uint8_t>  m_LayerWeights;   // see 2.3

    // --- runtime only, not serialized ---
    std::vector<TerrainChunk>  m_Chunks;
};
```

Why one field rather than per-chunk arrays:

- **Seams become structurally impossible, at every LOD level.** Chunk *n* and chunk *n+1* read the
  same edge samples, so their edges agree by construction rather than by two independently written
  expressions happening to evaluate to the same number. Today's high-detail meshes get this right by
  luck and the LP meshes get it wrong (bug 2); a shared field removes the luck from both.
- **Normals at chunk edges are correct for free**, because the neighbour samples are simply there.
  No skirt array, no border to keep in sync — which is what the unused `+ 2` was reaching for.
- **Sculpting across a chunk boundary is trivial.** The brush writes samples; then you mark the
  chunks whose range the brush rect touched as dirty. Without a shared field, every stroke near an
  edge has to write two chunks and keep them consistent.

The cost is that the terrain is one allocation rather than many, which makes per-chunk streaming
from disk harder later. For an editor-authored, bounded terrain — which is what you're asking for —
that is the right trade. If infinite/streamed terrain ever becomes a goal, the field becomes a cache
of loaded tiles and the chunk-view code above it does not change.

**The terrain is growable.** Chunk rows can be added and removed at the edges rather than being
fixed at creation ([Decisions](#decisions) 1). Against a shared field that is a reallocate-and-copy,
which is cheap — but it has two consequences that are cheap now and expensive to retrofit, so they
belong in unit 1:

- **Persist `m_ChunkOrigin`.** Growing on the −x or −z edge renumbers the chunks. Without an origin,
  the chunk that was index 0 becomes index 1 and its world position moves by one chunk, so existing
  terrain visibly slides relative to its entity transform. Decrementing an origin instead keeps
  everything where the author put it. It costs one field and one add in the chunk → world mapping.
- **Nothing outside the asset may hold a flat sample index across a resize.** Growing in x changes
  the row stride and prepending in z shifts every row, so flat indices survive only growth at the
  +z edge. Brush dirty-rects and the undo records in 2.5e store grid coordinates and translate
  through the origin at the point of use.

**`uint16` rather than `float`.** PhysX's heightfield stores `int16` samples anyway
(`PxHeightFieldFormat::eS16_TM`), so float precision is thrown away at the collision boundary
regardless. A normalized `uint16` against a per-terrain `[HeightMin, HeightMax]` gives ~0.002 units
of precision over a 128-unit range, halves the file size, and makes the PhysX cook a straight
rescale. A 4×4 chunk terrain at 64 quads per chunk is a 257×257 field = 132 KB, against
4.2 MB for the current layout.

**Resolution.** 64 quads per 64-unit chunk (1 unit spacing) is a much more sensible default than
today's 0.25 units. Make both numbers per-terrain settings rather than `constexpr`, since a
courtyard and a valley want different densities and the constants currently leak into PhysX, the
collider and the mesh generator alike.

**Split runtime from persisted.** `TerrainChunk` becomes purely runtime — LOD meshes, world AABB,
PhysX handle, dirty flag — and is never serialized. That alone removes the "chunk material silently
lost" class of bug, because there is nothing in the runtime struct that *should* have been saved.

**`GetHeightAt` belongs to the asset, in unit 1.** 2.5a treats it as a by-product of the sculpt
ray march, but the chunk AABBs, the physics scale and the `/terrain` endpoint all want it earlier,
and units 4 and 7 assert against it. Three things worth pinning down rather than discovering:

- **Terrain-local coordinates, in world units**, with the origin at the grid origin that
  `m_ChunkOrigin` tracks — so a given point keeps its coordinate when the terrain grows on the −x
  or −z edge. The component adds the entity transform for callers who want world space; the asset
  has no business knowing about entities.
- **Interpolate the way the mesh is triangulated, not bilinearly.** A quad is two triangles, so the
  rendered and simulated surface is not the bilinear patch. Pick the triangle by which side of the
  quad's diagonal the point falls on, then interpolate across it. This matters because the diagonal
  has to agree in *three* places — the mesh generator, this query, and PhysX's per-sample tessellation
  flag (`PxHeightFieldSample::setTessFlag`/`clearTessFlag`, which today's cooker always clears).
  Three independently written expressions that must agree is precisely the shape of bugs 2 and 9;
  decide the convention once and have all three read it.
- **`std::optional<float>`, empty outside the terrain.** Out-of-bounds is a normal case — the ray
  march queries past the edge on most casts — so it should be answerable rather than clamped into a
  wrong number.

**Seeding a terrain.** A freshly created terrain is flat, and the brush does not arrive until unit
7 — so without something in between, units 2 through 6 have nothing to look at, and unit 2's
criterion in particular ("visible and correctly lit") is untestable on a flat plane, because a flat
plane is exactly what bug 1 produced.

So unit 1 also carries a fill-from-noise on the asset. The existing `TerrainPerlinSettings` and its
three-layer octave code are fine and can be kept nearly as-is; they just write the shared field and
normalize into `[m_HeightMin, m_HeightMax]` instead of filling per-chunk arrays. Persist the
settings on the asset so re-rolling a seed is an editor button, and accept that regenerating
discards sculpted edits — that is what the undo stack is for.

This is a seeding and debugging convenience, not a procedural-generation feature. It does not need
to grow beyond the settings that already exist.

## 2.2 Rendering: chunks, LOD, culling, lighting

**LOD.** Generalise today's two fixed meshes into *L* levels built by sampling every 2^l-th sample.
With 64 quads per chunk, levels of 64/32/16/8 quads are natural. Select per chunk by distance to the
chunk's AABB (not its origin), with the switch distances derived from `m_ChunkSize` rather than a
magic `3000.f`.

**Cracks between adjacent LOD levels** are the classic problem here. Two answers:

- **Skirts** — a vertical rim around each chunk hanging down by the largest height delta in that
  chunk. A few hundred extra triangles per chunk, zero index-buffer complexity, invisible in
  practice.
- **Stitched index buffers** — per-edge index variants selected by the neighbour's LOD. Exact, no
  wasted geometry, and considerably more code and state.

**Recommend skirts.** The failure mode of a skirt is a sliver of ground-coloured geometry at a
chunk edge; the failure mode of a stitching bug is a hole through to the skybox.

**Culling.** Give each chunk a world AABB from its own height min/max. `Frustum::Intersects(min,
max)` already exists and does exactly this test. The wrinkle:
[`rendering.md`](../rendering.md#visibility--culling) is firm that visibility belongs to
`(object, frustum)` and lives in the context's `VisibilitySet`, indexed by
`Component::GetInternalId()` — but a terrain has many chunks under one component id, so it cannot
use that bitset. Two options:

- Cull chunks inline in `TerrainRenderer::Render` against the context's frustum. Simple, and chunk
  counts are in the hundreds, not the tens of thousands.
- Add a second per-context visibility structure for terrain chunks.

**Recommend inline culling**, with one supporting change: `RenderingContext` should carry its
`Frustum`. `Pipeline3D::Run` currently builds one in the prepass and throws it away
(`Pipeline3D.cpp:319-320`), so terrain would otherwise rebuild it. Storing it on the context is a
small change that anything else needing the viewer's frustum can reuse.

**Lighting.** Terrain must stop drawing with no hint data. Each *chunk* should get its own light
slots, assigned by the same `SceneLightsProcessor` logic that `ModelRenderer` uses. That function
is currently typed to `ModelRenderer*`
(`SceneLightsProcessing.cpp:122`); the clean move is to generalise its core to
`(position, hintData)` and let both callers use it, rather than growing a parallel terrain-only
copy. Chunk-granularity light slots are coarse — a 64-unit chunk gets one set of 5 point + 2 spot
lights — but that is enormously better than the current "whatever was in `Instances[0]`", and
finer granularity would mean splitting chunks, which is a later problem.

**Shadows.** Terrain needs to render into the shadow passes. Once the draw path honours
`OverrideShader` properly (it already can — see bug 11) the only real obstacle is LOD selection
using the scene camera. Fix: have `TerrainRenderer` take the view it is rendering for as a
parameter instead of holding `m_SceneCamera` in a file-scope global, and select LOD from that view's
origin. A shadow view can then ask for a fixed low LOD, which is what you want anyway.

## 2.3 Multiple materials — smaller than you think

**The approach: splat (weight) mapping.** N layers, each a material; per-texel weights; the fragment
shader blends them:

```glsl
vec4 w = texture(splatMap, vIn.uv);           // normalized, sums to 1
surface.diffuseColor = w.r * tex(layer0) + w.g * tex(layer1)
                     + w.b * tex(layer2) + w.a * tex(layer3);
```

Four layers is the natural first target: four weights fit one RGBA8 texture exactly, and four
diffuse + four normal maps fit the sampler budget comfortably.

**Pine's renderer is already half-prepared for this**, which is the genuinely good news here:

- `Specifications::Samplers` spaces the texture bases four apart —
  `BASE_DIFFUSE = 0`, `BASE_SPECULAR = COUNT (4)`, `BASE_NORMAL = COUNT * 2 (8)`. **Four units per
  texture type are already reserved**; `Renderer3D::PrepareMesh` binds only unit 0 of each, so
  slots 1–3 of every type are free and already carved out.
- The material UBO is `MaterialProperties matPropeties[8]` on both sides
  (`uniform-buffers.glsl`, `ShaderStorages.hpp:41`). Only `[0]` is ever written. **Per-layer
  colours, shininess and UV scale already have somewhere to live.**
- `generic.ih` already declares a `VERSION_TERRAIN` shader version bit, and the shader-version
  mechanism (`.ih` declares it, the importer compiles a variant with that `#define`) is exactly the
  right tool. The bit value needs fixing — it collides with `PerformanceFast` — but the machinery
  is there.

So the work is:

**(a) A terrain shader.** Prefer a separate `terrain.passet` (`terrain.vertex.glsl` /
`terrain.fragment.glsl`) over a `VERSION_TERRAIN` variant of `generic`. Both include
`shared/common.glsl` and `shared/lightning/lightning.glsl`, so ambient, directional, point, spot,
shadow and fog are shared code, not copies. Only `CreateSurface()` differs — it samples and blends
four layers instead of one. Bolting this onto `generic` would put a layer-count branch in the
shader every material in the game compiles.

⚠ Whatever it does, it must preserve `generic.fragment.glsl`'s rule about **never indexing
`vIn.lightDir[]` with a variable** — the hand-unrolled literal subscripts are there because dynamic
indexing of that varying returns garbage on some NVIDIA drivers.

**(b) Where the weights live.** Two candidates:

| | Vertex attribute (RGBA8/vertex) | Splat texture per chunk |
|---|---|---|
| New sampler | no | yes (one) |
| Resolution | tied to mesh density | independent |
| Survives LOD | **no** — lower LODs drop vertices and with them weight detail | yes |
| Sculpt-paint symmetry | weights edit alongside heights | separate array |
| New Mesh vertex stream | yes (`Mesh` currently exposes vertices/normals/uvs/tangents only) | no |

**Recommend the splat texture.** The LOD row is the decider: with vertex weights, a chunk visibly
changes its material blend when it drops an LOD level, which is exactly the artefact LOD is supposed
to hide. It needs one free sampler unit — 17 is the shadow atlas, 12–16 are unused.

**(c) A terrain-aware prepare call.** `Renderer3D::PrepareMesh` binds one material. Terrain needs
four plus a splat map. Add a sibling rather than complicating the existing function:

```cpp
void Renderer3D::PrepareTerrainChunk(Mesh* mesh,
                                     const std::array<Material*, 4>& layers,
                                     Graphics::ITexture* splatMap);
```

It writes `Properties[0..3]`, binds diffuse 0–3 / specular 4–7 / normal 8–11, binds the splat map,
and selects the terrain shader — while still honouring `OverrideShader` and
`SkipMaterialInitialization` so the depth prepass and the shadow passes work unchanged.

**(d) Four layers, but keep the door open.** Four is the decided target
([Decisions](#decisions) 4). Still store layers as `std::vector<TerrainLayer>` on the asset rather
than a fixed array, so the cap is a shader limit and not a format one: raising it later is then a
shader + binding change (or a `Texture2DArray` with per-chunk layer indices), not a data migration
of every saved terrain. The array path is explicitly not being built now.

**Scale of the renderer change: one new shader, one new `Renderer3D` entry point, one sampler
binding, and generalising the light-slot assignment.** Everything else in `Rendering/` is untouched
and no existing path changes behaviour. That is much less than "big updates to the renderer".

## 2.4 Physics

Straightforward once the data model is right, but all of it needs replacing:

- **One `PxHeightField` for the whole terrain**, not one per chunk, built from `m_Heights` with a
  single scale onto `PxHeightFieldSample::height`. Chunks exist for rendering LOD and culling;
  collision needs neither, and PhysX runs its own broadphase over the heightfield. This is what
  keeps `CreateShape` returning a single shape, so neither `Collider` nor `RigidBody` has to learn
  about multi-shape actors, and it reduces `localPose` to one offset derived from `m_ChunkOrigin`
  and `m_ChunkSize` instead of the per-chunk arithmetic that currently disagrees three ways.

  The trade is that a sculpt commit re-cooks the whole field rather than the touched chunks. At the
  sizes in 2.1 a 257² field is 66k samples, which is a millisecond-scale cook and a non-issue. That
  stops holding somewhere past roughly 1025², which is the point at which chunked collision and
  multi-shape actors would be worth revisiting.
- **`Destroy` must `release()` the `PxHeightField`** and null the pointer, and the cooking scratch
  buffer must stop being a function-local `static`.
- **`Collider` keeps owning the shape** ([Decisions](#decisions) 2), and creates exactly one. The
  loop at `Collider.cpp:208-213` that overwrites `geometry.heightField` once per chunk and leaves
  the last one winning simply goes away with the whole-terrain field above.
- **`rowScale`/`columnScale`/`localPose` must be derived from the same numbers the mesh uses.**
  They are currently three independently-written expressions and none of them agree.
- **Cooking is far too slow to run per sculpt stroke.** Rebuild collision on mouse-up or on save,
  not per frame.

## 2.5 Editor tooling — raise/lower brush

The most tractable of the three, and the parts Pine is missing are small.

**(a) Cursor → terrain point.** There is no ray-cast utility in the editor today.
`EntitySelection::Pick` is a colour-ID framebuffer readback, which gives you an entity but no
position. Two options:

- Read back the depth prepass. `Pipeline3D::GetPositionTexture()` already exposes an RGBA16F
  attachment. But it is a fixed 1920×1080 internal buffer belonging to the pipeline, and needs a
  GPU→CPU readback per frame while dragging.
- **March a ray against the height field analytically.** Build a ray from the editor camera and the
  viewport-relative cursor, DDA it across the chunk grid, then do a bilinear height test per cell.

**Recommend the analytic march.** It is around 60 lines, exact, needs no GPU readback, works when
the terrain isn't currently drawn, and lands directly in grid coordinates — which is what the brush
needs anyway. It builds on `Terrain::GetHeightAt(x, z)`, which 2.1 puts in unit 1 for the benefit of
the AABBs, the physics scale and the debug server ("put this entity on the ground"); by the time the
march is written, the per-cell height test it needs already exists.

**(b) The brush.** Radius, strength, falloff curve, and a mode (raise / lower / smooth / flatten to
a reference height). On drag, for each sample within the radius:
`height += strength * falloff(distance) * deltaTime`, sign by mode. Then mark every chunk whose
range the brush rect touched as dirty. With the shared field, edge samples are shared, so
neighbouring chunks stay watertight with no special case — this is the payoff of the 2.1 decision.

**(c) Incremental rebuild.** Today's `GenerateMesh()` rebuilds everything. The brush needs "rebuild
these 1–4 chunks", and the rebuild should update the existing GPU buffers rather than dispose and
recreate. `IVertexBuffer::UploadData(data, size, offset)` already exists, so the Graphics interface
needs nothing new — but `Mesh` only exposes whole-array `SetVertices`/`SetNormals`, so it needs a
way to hand back its buffers or to update a range.

**(d) Brush visualisation.** A ring projected onto the terrain surface. The terrain fragment shader
can do it directly through the existing `#shader hooks` / `postFragment` mechanism, given a brush
centre and radius uniform — tint fragments within the radius. Put it behind its own shader version
bit so the game never compiles it. The alternative, a debug line ring, is simpler but floats badly
over uneven ground.

**(e) Undo.** `Editor/src/Other/Actions/` plus `Gui/Shared/Commands/` is the existing mechanism, and
the debug server's `History` builds on it. One stroke = one command. Snapshotting the whole field
per stroke is too much (a 257² field is 132 KB); store the **bounding rect of samples touched** plus
its before/after contents, which is typically a few KB. Store that rect in grid coordinates rather
than flat indices, for the resizing reason in 2.1. A resize is its own command with a different
shape: it snapshots the rows it removes, not a bounded rect.

**(f) Painting layers is the same tool.** Build the brush against an abstract "apply to these
samples in this rect" step and the layer-paint tool is nearly free — it writes weights instead of
heights, with a normalize step afterwards. Worth structuring for from the start; it is the most
likely next feature and costs nothing now.

**(g) Panel placement.** A `Gui/Panels/TerrainTools/` panel (mode, layer, radius, strength,
falloff), plus a hook in `LevelViewportPanel` that suppresses ImGuizmo and entity picking while
terrain edit mode is active. Follows the existing panel convention; registering it in the `Gui`
panel loop is the only wiring.

---

# Part 3 — Suggested sequencing

Eight units, each independently reviewable and each ending in something you can actually check. The
sizes are deliberately uneven: the early ones are small because that is where a mistake is hardest
to attribute to a cause.

## What verification looks like here

Worth settling before the table, because it is what the unit boundaries are drawn around.

`Editor/src/DebugServer/Verification/` already holds fifteen `verify-*.py` scripts that drive the
editor over HTTP and assert on what comes back — `verify-physics.py`, `verify-history.py`,
`verify-components.py` and the rest. That is this repo's integration suite, and terrain should
extend it rather than invent anything. A unit that ends in a `verify-terrain-*.py` is genuinely
checked; a unit that ends in "open the editor and look at it" is not.

Two consequences for the ordering:

- **A read-only `GET /terrain` belongs in unit 1, not at the end.** Chunk count, field dimensions,
  height at a coordinate, per-chunk ready/dirty state. It is a small handler against the existing
  `AddRoute` pattern, and it is what turns each later unit from a screenshot into an assertion.
- **`EngineCli --dump` prints any `.passet` as JSON**, with no graphics context and no project. That
  makes the asset and its serializer checkable on their own, which is why they get a unit to
  themselves.

## The units

| # | Unit | Done when | Checked by |
|---|---|---|---|
| **0** | Data model in 2.1, plus the four decisions | Settled — see [Decisions](#decisions) | — |
| **1** | Asset, storage, queries, noise fill, `/terrain` | A terrain saves, reloads, fills from noise and answers `GetHeightAt` | `EngineCli --dump` |
| **2** | Mesh generation and the draw path, one LOD | Terrain is visible and correctly lit | `/viewport.png` |
| **3** | LOD levels, skirts, culling | Cost drops with distance, no cracks at chunk edges | `/stats` visible/culled counts |
| **4** | Physics | A body dropped from above lands at `GetHeightAt` | `verify-terrain-physics.py` |
| **5** | Lighting and shadows | Terrain takes nearby lights and casts into the shadow atlas | `/viewport.png` |
| **6** | Multiple materials | Four layers blend across a chunk | `/viewport.png` |
| **7** | Sculpting | A stroke raises ground; undo restores it exactly | `verify-terrain-sculpt.py` |
| **8** | Layer painting | The same brush writes weights | extends unit 7's script |

## Why the boundaries fall there

**1 and 2 are separate because unit 1 is the only part verifiable without a window.** The new
`Terrain` fields, the `uint16` field, the serializer, `GetHeightAt`, the chunk-view accessors and the
per-chunk AABBs need no graphics context at all. Landing them first means that when unit 2 renders
nothing, the data underneath is already known good. Unit 1 also carries all of the deletion: the
`+ 2` border, `TERRAIN_SQUARE_SIZE`, `GetHeightmapData`, `m_HeightMap` and the dead "Generate All"
button.

It carries two things that look like they belong later and do not: `GetHeightAt` and the
fill-from-noise, both in 2.1. The fill is what makes unit 2 testable at all — a flat terrain lit
correctly and a flat terrain lit by bug 1 look identical — and `GetHeightAt` is the assertion target
for units 4 and 7.

This inverts §2.5(a), which treats `GetHeightAt` as a freebie falling out of the ray march. Build it
in unit 1 as the primitive — the AABBs, the physics scale and `/terrain` all want it — and let the
ray march consume it in unit 7.

**2 and 3 split on "can I see it" versus "is it fast and seamless".** Unit 2's whole job is making
the bug 1 fix visible: correct normals, one LOD, no culling, one material. It also carries the two
structural fixes that need no new design — moving `GenerateMesh` out of the draw pass (bug 7), and
passing the view into `TerrainRenderer` as a parameter instead of the file-scope `m_SceneCamera`,
which retires bug 6 on the way past. Unit 3 is then purely additive, and each piece of it is
separately observable.

**5 splits out of what 2.3 bundles into a single stage.** Per-chunk light slots and shadow-pass
participation have nothing to do with splat mapping, and the order matters in one direction: unit
6's `terrain.fragment.glsl` includes `shared/lightning/lightning.glsl` and calls the same lighting
path. Building a new four-layer shader on top of lighting that is known broken means debugging the
blend and the lighting simultaneously. Get lighting right on the one-material shader first.

**4 is worth doing earlier than it looks, because it is the only unit with a numeric pass/fail.**
Bug 9 — collision not lining up with what you see — is invisible in a screenshot and is a one-line
assert against `GetHeightAt`. It is also fully independent of 5 and 6.

## Dependencies

`0 → 1 → 2 → 3`, and then **4, 5 and 6 are mutually independent**. Unit 7 needs 1–3 only, not 4–6.
Unit 8 needs 6 and 7. Unit 3 is therefore a real fan-out point: after it, physics, lighting and
sculpting can proceed without touching each other's files.

## Two cautions

**Units 3 and 5 are the only ones that touch shared code.** Giving `RenderingContext` its own
`Frustum` changes a struct every pipeline and feature sees, and generalising
`SceneLightsProcessor::ProcessModelRenderer` to `(position, hintData)` changes a function
`ModelRenderer` depends on. Everything else stays inside terrain's own files. Those two are worth
agreeing on specifically before they are written.

**Replace in place; do not build a parallel `Terrain2`.** There are no `.ter` files, no terrain
`.passet`, and nothing under `data/` references `TerrainRenderer` — so there is no data and no user
to keep working. Accepting that terrain is broken between units 1 and 2 is far cheaper than
maintaining two asset types through six units.

Launching the editor to check any of this is explicitly allowed here — see
[`leave-to-the-user.md`](../../agent-guidelines/leave-to-the-user.md).

---

# Decisions

All four questions that gated this work are settled. Recorded with their reasoning, because the rest
of the document assumes them.

1. **Terrain size — growable.** Chunk rows can be added and removed at the edges rather than being
   fixed at creation. Two consequences, both landing in unit 1: the persisted `m_ChunkOrigin`, and
   the rule that nothing outside the asset holds a flat sample index across a resize. Both are in
   2.1.

2. **`ColliderType::HeightField` survives.** It stays the way a terrain gets collision, with
   `Collider` sourcing the geometry from the sibling terrain component.

   Two reasons. First, the alternative — the terrain owning its own PhysX actor — means
   `TerrainRendererComponent`, a *renderer*, quietly creates physics actors. That is the weirder of
   the two shapes, and it hides collision somewhere nobody would look for it.

   Second, `Collider` reading geometry from a sibling is not a terrain special case. `ColliderType`
   already declares `ConvexMesh` and `ConcaveMesh`, both currently unimplemented, and both of which
   will have to source their mesh from the sibling `ModelRenderer` in exactly this way —
   `Collider.cpp` already includes `ModelRenderer.hpp` in anticipation, unused. Dropping
   `HeightField` would not remove the pattern from `Collider`; it would only postpone it until
   someone implements convex meshes.

   The complexity this question was really about — one shape per chunk on a single actor — is
   avoided by not chunking collision at all. See 2.4.

3. **Heightfield terrain is the terrain system**, not ground beneath a tile kit. The `gm` project's
   modular `Terrain-*.glb` kit has been removed: no terrain `.passet` remains under
   `data/projects/gm/assets/`, and `ravenholm.passet` carries no terrain references. Eighteen
   orphaned `.glb` sources are still sitting in `data/projects/gm/content/` — nothing imports them,
   but a batch import over that directory would bring them back.

   The accepted consequence: a heightfield gives every column exactly one surface, so overhangs,
   caves and true vertical cliff faces cannot be represented. Those are modelled meshes placed on
   the terrain, which is roughly what the tile kit was doing anyway — just for the exceptional
   pieces rather than for all the ground. Per-quad holes, the escape hatch Unity added for cave
   mouths and cheap here against PhysX's `PxHeightFieldMaterial::eHOLE`, were considered and
   deliberately left out of scope.

   This is also what makes units 6 and 8 load-bearing rather than optional: with no kit, the
   terrain's own material *is* the entire ground surface.

4. **Four layers.** One RGBA8 splat texture, four diffuse and four normal maps, which fits the
   sampler budget and the `Properties[0..3]` slots exactly. The asset still stores layers as a
   `vector` (2.3d) so the limit lives in the shader rather than in the format, but the
   `Texture2DArray` path is not being built.

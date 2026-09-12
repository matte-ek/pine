# Review: `357fa72` — "Add better frustum culling and handle lights better"

Read-only review of one commit, at the state it landed in. Where a later commit already moved
something on, that's noted — the point is what the commit did to the renderer and which of its
assumptions are still load-bearing today.

## What actually landed

The message names two things. There are five, and three of them change behaviour outside the
renderer.

**1. Culling was replaced, not improved.** The old `RunFrustumCulling` built one AABB around the
camera's frustum corners, then compared `distance²(objectCenter, boxCenter) < distance²(min, max)`
— a sphere-vs-point test against a box diagonal, in the wrong units, with the object's own extent
never entering the comparison. It was a distance cut-off wearing a frustum's name. The replacement
is a real Gribb-Hartmann plane extraction (`Core/Math/Frustum/`) plus a proper AABB-vs-6-planes
test.

The structural move matters more than the math: visibility moved off the component
(`HasPassedFrustumCulling`) into a `VisibilitySet` owned by the `RenderingContext`. That is what
made shadow culling possible two commits later — `Shadows.cpp` now culls each `ShadowView` against
its own frustum through the exact same `Cull()` call, with no second visibility concept. Taking a
bitset indexed by `GetInternalId()` rather than a per-object flag was the decision that paid for
itself; it is ~512 bytes per frustum at the default 4096 objects, so N live frustums is free.

Bounds, by contrast, stayed on the object (`ModelRendererHintData::BoundsMin/Max`) — correctly, since
bounds are a property of the object and visibility is a property of (object, frustum). The eight-corner
rotation in `UpdateWorldBounds` is what the old code got wrong for anything not axis-aligned.

**2. Light slots were re-laid-out and given a name.** The magic `6` scattered across
`Renderer3D`, `SceneLightsProcessing`, `ModelRendererHintData` and the debug panel became
`Specifications::ObjectLightSlots` (5 point + 1 spot, `COUNT` 6 — since grown to 7). The candidate
selection was rewritten from a hand-unrolled 3-deep insertion into a `NearestLights<Count>` template,
which is both shorter and actually correct: the old version tracked four candidates against three
distances and could not have ranked the fourth.

**3. A genuine lighting bug fix, unannounced.** `AddLight`'s point-light loop had no `break`, so one
light filled *every* free fallback slot. And `CalculateSpotLights`/`CalculatePointLights` in
`generic.fragment.glsl` had their bodies swapped — the function named "spot" called
`CalculateSpotLight`… on the five point-light slots. Both are fixed here. Neither is in the message.

**4. Ambient light was pulled out of the per-light path.** Previously every light returned
`max(1 - diffuseFactor, 0) * ambient` as part of its result and those were summed, so scene
brightness scaled with how many lights happened to reach a surface, ambient escaped both attenuation
and the cone mask, and it churned as light slots changed while an object moved. Now
`CalculateAmbientLight` is called once per fragment. This is the right shape, and it is also a
visible look change to every lit scene — lit areas get brighter, and the old inverse-diffuse
modulation (which was doing an accidental, crude ambient occlusion) is gone.

**5. `Transform::GetForward/GetRight/GetUp` now use world rotation.** Changed from `m_LocalRotation`
to `GetRotation()`. This is a bug fix — a camera childed to a player reported its direction relative
to the player — but it is a *public scripting API* change (`Transform.Forward` et al. via
`ScriptInterfaceComponent`), it affects the editor fly-camera, and it is in a commit about culling.
Anyone with a parented entity relying on the old (wrong) answer breaks silently.

## Findings

### HIGH CONFIDENCE — the transform dirty flag never fires in the editor

`HasSlotInputChanged` (`SceneLightsProcessing.cpp`) is documented as: *"The transform flag covers
movement (slots are picked by distance)"*. In editor mode it covers nothing.

`RenderManager::Run` does this, in this order, in one function:

```cpp
if (!engineConfig.m_ProductionMode)
{
    for (auto& transform : Components::Get<Transform>(true))
        transform.OnRender(fDeltaTime);          // -> CalculateTransformationMatrix() -> m_IsDirty = false
}

Pipeline3D::Prepare();                            // -> SceneProcessor -> HasSlotInputChanged()
```

Every transform is cleaned immediately before the only code that reads the flag. Nothing sets it in
between — all user edits, script moves and physics run outside `RenderManager::Run`. So in the
editor `GetTransform()->IsDirty()` is *always* false at that point, and light-slot invalidation falls
back entirely to `Entity::IsDirty()`.

That fallback is not complete. `LevelViewportPanel` sets `selectedEntity->SetDirty(true)` on gizmo
manipulation, but `ComponentPropertiesRenderer::RenderTransform` calls only `transform->SetDirty()` —
no entity flag. So dragging an entity's Position field in the Properties panel does not re-pick its
light slots; dragging the same entity with the gizmo does. In production mode the transform loop
doesn't run, the flag survives, and everything works — which is the worst shape for this bug, since
the editor is where you'd notice.

The "Invalidate object lightning data" button in `DebugPanel` suggests this staleness was already
being worked around by hand.

Cheapest honest fix is to make the editor's properties-panel path set the entity flag like the gizmo
path does; the more robust one is to stop clearing transform dirty before the scene processor has
read it.

### MEDIUM CONFIDENCE — the light-slot cache collapses under a single moving light

`Lights::Prepare` invalidates `HasComputedData` for **every** `ModelRenderer` if *any* light changed.
`ProcessModelRenderer` then costs O(lights) per object. So one moving light — a torch, a flashlight,
a swinging lamp, the single most likely thing for a light to do — puts the whole scene back to
O(objects × lights) every frame, permanently. The cache only helps scenes whose lights are all
static.

This is fine as today's requirement and worth knowing before the object count grows. The obvious next
step is to invalidate by proximity (only objects within the moved light's range) rather than globally,
which needs a light range — and `Light::m_Range` exists at HEAD.

### MEDIUM CONFIDENCE — `dynamic_cast` → `static_cast` in `ComponentHandle::Get()`

Unrelated to the commit's subject and unmentioned. It's safe for correct callers — `Get()` resolves
via `m_Type` and verifies the `UId` — but `operator=` takes a bare `const Component*` with no
relation to `T`, so `ComponentHandle<Light> h = someModelRenderer;` used to yield `nullptr` and now
yields undefined behaviour. Was the cast a measured win? If so it's worth an
`assert(m_Type == ComponentTypeOf<T>)` in `operator=` to keep the guard in debug builds.

### MEDIUM CONFIDENCE — silent data migration on the spotlight rename

`SpotlightRadius`/`SpotlightCutoff` (0–1 cosine-ish values) became `SpotlightOuterAngle`/
`SpotlightInnerAngle` (degrees), and the *serializer field names* changed with them. The serializer is
name-keyed and `DataPrimitive::Read` respects `m_Populated`, so old levels degrade gracefully to the
new defaults (45°/30°) rather than loading 1.0 as one degree — that part is genuinely fine, and it's
worth knowing that's why.

But it is silent: every spot light in every saved level and blueprint quietly changes cone on load,
and the old authored value is dropped for good on the next save. For a personal project with few spot
lights that's likely the correct trade; it just wasn't a stated one.

### LOW CONFIDENCE / QUESTION — leftovers and asymmetries

- **`Renderer3D::FrameReset` still clears with a literal `8`** while the same commit replaced every
  other magic number in that file with `ObjectLightSlots::*`. Eight is the physical `ivec4[2]` size
  rather than `COUNT`, so it isn't wrong — but it's the one literal left in the code being tidied, and
  it's the one that silently over-clears if `COUNT` ever drops. Intentional?
- **`Light::LoadData` writes `m_LightType` directly**, bypassing the `SetLightType` that this commit
  taught to mark the entity dirty. Load is covered incidentally (the light count changes, which
  invalidates everything), so it works — but the new invariant is setter-only, and the next caller
  that sets the field directly won't get it.
- **`Frustum::FromViewProjection` assumes GL clip space.** `Near = rowW + rowZ` is the z ∈ [-1,1]
  convention; a Vulkan backend (the enum stub exists) needs `Near = rowZ`. One sentence in the header
  would save the eventual debugging session — the comment currently says the extraction "works for
  perspective and orthographic alike", which is true and reads as broader than it is.
- **`normalize(lights[0].directionToLight)` with no directional light in the scene** is
  `normalize(vec3(0))` → NaN, multiplied by a zero colour. Pre-existing (it was `rotation` before) and
  benign under GLSL's `max(NaN, 0.0)` → `0.0`, so this is a note, not a bug.
- **`NearestLights::Get`'s comment** says "Returns nullptr if fewer than 'index' lights were
  inserted" — should be `index + 1`.
- **`Light.cpp` lost its trailing newline.**

## On code quality, against the guidelines that now exist

The engine-facing work here is the strong part and would pass the current guidelines comfortably:
the frustum/visibility split is the right seam, it's named for what it is, and it took the weight of
shadow culling two commits later without changing shape. `NearestLights` and `ObjectLightSlots`
both replace duplicated magic with something a reader can check.

Two habits are worth naming, since both recur:

**Bundling.** Five changes under a two-item message, including a public scripting-API change
(`GetForward`) and a type-safety change (`ComponentHandle`) that have nothing to do with either
stated item. The bug fixes (the missing `break`, the swapped shader functions) are worth knowing
about on their own; buried here, they read as refactor noise.

**Comments asserting invariants the code doesn't hold.** This is the one to watch, because the
comments here are otherwise very good — they explain *why*, they record what was rejected, they warn
about the NVIDIA varying-indexing trap. That's exactly the density the guidelines ask for. But
`HasSlotInputChanged`'s comment states an invariant that is false in half the engine's run modes, and
because it reads as authoritative it's now harder to spot the bug than if there had been no comment.
A comment claiming a guarantee is worth a moment checking the guarantee actually holds on both paths.

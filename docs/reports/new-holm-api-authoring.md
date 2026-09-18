# New Holm: Editor API authoring report

2026-09-18 · Project `gm` · Level `levels/new-holm`

The API now supports a complete static environment authoring session: discover
assets, assemble architecture, dress it, light it, inspect it, and save it. The
remaining friction was mostly deciding what assets look like and understanding
the rendered result, rather than being unable to place objects.

This session produced 565 scene entities: 488 ModelRenderers, 17 Lights,
69 Colliders and one selected scene Camera. After save/reload, hierarchy paths,
active/static flags, component membership and writable properties matched
(floating-point values compared to five decimal places). Camera selection also
survived, the Level reported no unsaved changes, and the final observation had
no new log messages since the pre-save cursor. This verifies authoring and
persistence; gameplay and physics simulation were not tested.

**What worked well**

- **Schema and documentation made the API usable without probing by failure.**
  Property names, quaternion format, light conventions, collider half-extents,
  and Pine's unusual parent-transform semantics were explicit. The latter kept
  me from using scaled parents as conventional building-layout transforms.
- **Batch editing handled a substantial scene comfortably.** Architecture,
  dressing and lighting were separate batches under named groups. Returned
  entity/component IDs supported later adjustments. The 128-operation limit
  was sufficient; increasing it would not be my next priority.
- **Measured placement was a substantial improvement.** Asset bounds informed
  module dimensions. `entity.place` grounded 75 props using their model bounds,
  including rotated saw blades and assets with off-center pivots. Two
  `entity.aim` operations oriented the arrival camera and a spotlight. This
  removed a lot of manual pivot compensation.
- **Spatial queries and batched readback closed the loop.** Fresh world bounds
  supported simple prop collision volumes. A stopped-mode raycast checked a
  centerline sightline without starting physics. Six entity-query batches per
  snapshot made a full persistence comparison practical. The raycast was only
  a line check, not proof of player clearance.
- **Camera control, captures and explicit saving completed the workflow.**
  I inspected both street-height and elevated views, selected a persistent Game
  camera, and used an ordered `/observe` capture after the final reload.

**Where the API got in the way**

| Priority for another environment build | Actual friction | Useful addition |
| --- | --- | --- |
| 1. Visual asset discovery | The catalog contained 1,093 models with often cryptic names. I built and deleted a temporary scene palette to see wall and roof variants. Bounds and material names helped, but did not reliably reveal appearance. | Asset thumbnails or isolated previews, plus compact batched bounds/material summaries. Search is useful, but seeing candidates would save more effort. |
| 2. Atmosphere controls | I could read ambient light, fog and post-processing settings, but could not author them through the API. The existing night sky suited this scene; otherwise I would have been constrained. | Typed, undoable Level settings for ambient light, fog, skybox and exposure, followed by the existing save/readback workflow. |
| 3. Lighting diagnostics | Tuning involved changing light intensity/color and looking again. Strong angular shadows appeared in intermediate views and were less evident in the final reloaded view. The captures were not a controlled comparison, so this is an investigation lead, not a diagnosed bug. | Frame-associated model light-slot assignments and shadow allocation/cache information; optional atlas and light-volume captures. |
| 4. Independent captures | Every inspection angle moved the user's editor camera. Capture resolution also depended on the visible viewport. | Render from a supplied pose and size without changing editor navigation or requiring a visible tab. |
| 5. Collision and play verification | I generated conservative, separate box colliders from world bounds. Those can overestimate rotated props. I could not run an API-driven traversal or simulation check. | Collider overlays and a model-local box-fitting helper; then play/stop and controlled stepping for runtime checks. These would not by themselves make ladders or traps interactive. |

Two smaller conveniences would also help: allow references to earlier creations
as targets of placement/aim operations, and spawn existing Blueprint assets.
The first would combine today's create → collect IDs → place sequence; the
second would make a dressed building or lamp assembly reusable across sessions.
Within this build, ordinary scripted batches were sufficient for repetition.

**What I would change in my own workflow**

I initially printed too much of the catalog; caching it and showing a shortlist
worked better. Most intermediate screenshots used `/viewport.png`, even though
`/observe` already provides stronger ordering guarantees. Future sessions should
use ordered observations consistently, especially for lighting comparisons.
Neither issue requires a new endpoint. The initial Python connection failure
was a client sandbox restriction, not a Pine API failure.

Undo history was reported as recorded, but I did not exercise undo/redo. Relative
placement, capture picking, duplication, imports and terrain editing were also
not exercised. I would not infer their quality from this session, or prioritize
uneven-ground placement based on this deliberately flat scene.

My recommendation is to prioritize **asset previews, atmosphere authoring and
lighting diagnostics** for the next visual level-building pass. The existing
creation, placement and persistence tools already handled the core job well.
These recommendations are point-in-time feedback from that authoring pass; the
[HTTP reference](../debug-server.md) is authoritative for what the API does today.

**Follow-up: fixture light types**

The user's initial inspection identified an authoring mistake: shaded wall lamps
should emit downward spotlights, rather than the omnidirectional point lights I
originally assigned. Spotlight editing and aiming were already available, so this
was not missing API functionality. Light type and emitter placement should be
checked before interpreting unusual shadows as a renderer problem.

The user converted the first three street lamps. I preserved those settings and
converted the remaining three street lamps, churchyard gate lamp and chapel door
light in one recorded edit, aiming them downward. The three new street emitters
were also lowered just below their housings. Ordered before/after captures at the
same churchyard view showed reduced upward spill; an entrance capture checked the
street result. No new messages appeared in the conversion observation. This does
not establish that every shadow artifact is fixed. The changes remain unsaved
alongside the user's pre-existing adjustments, and the user's camera was restored.

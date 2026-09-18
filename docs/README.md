# Pine docs

One-page orientation docs per subsystem — key files, how the pieces fit, and the gotchas.
They point you to the right place; they are intentionally shallow, not exhaustive manuals.
Start with the root [`AGENTS.md`](../AGENTS.md) for the big picture, then dive into the
relevant page below.

For operating the running Editor, start with the [Editor API guide](debug-server.md):
all HTTP routes, a scene-building workflow, practical examples and troubleshooting.

## Engine subsystems

| Doc | Covers |
|-----|--------|
| [engine boot & main loop](../AGENTS.md#architecture-the-parts-that-span-multiple-files) | Lifecycle / boot order (in `AGENTS.md`); read `Engine/src/Pine/Engine/Engine.cpp` |
| [world-ecs.md](world-ecs.md) | Custom pool-based ECS: entities, components, storage, the enum-ordering gotcha |
| [assets.md](assets.md) | Asset manager, the `.passet` format, the serializer, Level/Blueprint |
| [rendering.md](rendering.md) | Graphics wrapper → Rendering → Renderer3D; pipelines, features, contexts |
| [scripting.md](scripting.md) | The Mono/C# bridge and the `ScriptRuntime/` project |
| [physics.md](physics.md) | Physics2D / Physics3D (PhysX) and the component coupling |
| [core.md](core.md) | Core utilities: Math, File, Serialization, UId, Log, WindowManager |
| [editor.md](editor.md) | The Editor executable: boot, panels, viewports, play mode, tooling |
| [data-and-projects.md](data-and-projects.md) | The `data/` tree, project structure, asset path resolution |

## Editor debug server

Start with the guide; the pages under it are the per-area contracts it links into.

| Doc | Covers |
|-----|--------|
| [debug-server.md](debug-server.md) | Complete Editor HTTP API reference and efficient scene-authoring workflow |
| [debug-server-inspection.md](debug-server-inspection.md) | Filtered and batched entity reads, component properties, transforms and spatial selection |
| [debug-server-spatial.md](debug-server-spatial.md) | Bounds and transform measurement, raycasts and bounds-overlap queries without a viewport |
| [debug-server-editing.md](debug-server-editing.md) | Batch operations, component properties, references, validation and readback |
| [debug-server-placement.md](debug-server-placement.md) | Surface placement, relative bounds alignment and offsets, history and verification |
| [debug-server-physics.md](debug-server-physics.md) | Collider and RigidBody authoring, creation on play, history and verification |
| [debug-server-camera.md](debug-server-camera.md) | Editor-camera state, look-at controls and framing model bounds |
| [debug-server-scene-camera.md](debug-server-scene-camera.md) | Perspective Camera editing, game-camera selection, history and persistence |
| [debug-server-observation.md](debug-server-observation.md) | Rendered captures, matching camera metadata, entity observations, and incremental logs |
| [debug-server-picking.md](debug-server-picking.md) | Reading the model surface under a pixel of a retained capture |
| [debug-server-import.md](debug-server-import.md) | Local file imports, source copies, live loading and re-imports |
| [debug-server-history.md](debug-server-history.md) | Batch undo/redo, restoration rules, level save/save-as and unsaved changes |
| [debug-server-requests.md](debug-server-requests.md) | Mutation identities, retained results, request status, cancellation and deadlines |
| [debug-server-todo.md](debug-server-todo.md) | Prioritized tracker for 3D scene authoring and observation through the debug server |

## Plans and reports

Point-in-time documents, kept for the reasoning behind a design rather than as current
reference. Where one describes shipped work, the contract docs above are authoritative.

| Doc | Covers |
|-----|--------|
| [reports/new-holm-api-authoring.md](reports/new-holm-api-authoring.md) | Practical API feedback from building and verifying a 565-entity environment |
| [plans/debug-server.md](plans/debug-server.md) | The original transport and binary→JSON design for the debug server — **implemented**, superseded by the guide above |
| [plans/terrain-system.md](plans/terrain-system.md) | The terrain rewrite proposal, plus a record of the implementation it replaced — **all eight units implemented** |
| [reports/review-ffaa33c-shadow-atlas.md](reports/review-ffaa33c-shadow-atlas.md) | Read-only review of the spot/point shadow commit: load-bearing assumptions and suggested follow-up |
| [reports/review-357fa72-culling-and-lights.md](reports/review-357fa72-culling-and-lights.md) | Read-only review of the frustum-culling and light-handling commit |

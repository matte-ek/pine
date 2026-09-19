# Pine docs

One-page orientation docs per subsystem — key files, how the pieces fit, and the gotchas.
They point you to the right place; they are intentionally shallow, not exhaustive manuals.
Start with the root [`AGENTS.md`](../AGENTS.md) for the big picture, then dive into the
relevant page below.

For operating a running Editor over HTTP, see the two debug-server pages below.

## Engine subsystems

| Doc | Covers |
|-----|--------|
| [engine boot & main loop](../AGENTS.md#architecture-the-parts-that-span-multiple-files) | Lifecycle / boot order (in `AGENTS.md`); read `Engine/src/Pine/Engine/Engine.cpp` |
| [world-ecs.md](world-ecs.md) | Custom pool-based ECS: entities, components, storage, the enum-ordering gotcha |
| [assets.md](assets.md) | Asset manager, the `.passet` format, the serializer, Level/Blueprint |
| [rendering.md](rendering.md) | Graphics wrapper → Rendering → Renderer3D; pipelines, features, contexts |
| [scripting.md](scripting.md) | The Mono/C# bridge and the `ScriptRuntime/` project |
| [audio.md](audio.md) | Device layer, the voice pool, the components, mono vs. `Spatial` |
| [physics.md](physics.md) | Physics2D / Physics3D (PhysX) and the component coupling |
| [core.md](core.md) | Core utilities: Math, File, Serialization, UId, Log, WindowManager |
| [editor.md](editor.md) | The Editor executable: boot, panels, viewports, play mode, tooling |
| [data-and-projects.md](data-and-projects.md) | The `data/` tree, project structure, asset path resolution |

## Editor debug server

The Editor exposes a localhost HTTP API for inspecting, editing, capturing and saving a
scene without a human at the keyboard. Two pages, no overlap:

| Doc | Covers |
|-----|--------|
| [debug-server.md](debug-server.md) | **Reference.** Every route and its contract: enablement, the main-thread execution model, shared conventions, observation tokens, retry identities, and the full `POST /edit` operation and component-property specification. |
| [debug-server-workflow.md](debug-server-workflow.md) | **Guide.** How to build a scene with it: orientation reads, asset discovery, measuring, batching edits, placement, capturing the result, saving, recovery and troubleshooting. Also how to verify a change to the debug server itself. |

## Plans and reports

Point-in-time documents, kept for the reasoning behind a design rather than as current
reference. Where one describes shipped work, the contract docs above are authoritative.

| Doc | Covers |
|-----|--------|
| [reports/new-holm-api-authoring.md](reports/new-holm-api-authoring.md) | Practical API feedback from building and verifying a 565-entity environment |
| [plans/terrain-system.md](plans/terrain-system.md) | The terrain rewrite proposal, plus a record of the implementation it replaced — **all eight units implemented** |
| [reports/review-ffaa33c-shadow-atlas.md](reports/review-ffaa33c-shadow-atlas.md) | Read-only review of the spot/point shadow commit: load-bearing assumptions and suggested follow-up |
| [reports/review-357fa72-culling-and-lights.md](reports/review-357fa72-culling-and-lights.md) | Read-only review of the frustum-culling and light-handling commit |

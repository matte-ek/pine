# Pine docs

One-page orientation docs per subsystem — key files, how the pieces fit, and the gotchas.
They point you to the right place; they are intentionally shallow, not exhaustive manuals.
Start with the root [`AGENTS.md`](../AGENTS.md) for the big picture, then dive into the
relevant page below.

For operating the running Editor, start with the [Editor API guide](debug-server.md):
all HTTP routes, a scene-building workflow, practical examples and troubleshooting.

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
| [debug-server.md](debug-server.md) | Complete Editor HTTP API reference and efficient scene-authoring workflow |
| [debug-server-editing.md](debug-server-editing.md) | Batch operations, component properties, references, validation and readback |
| [debug-server-camera.md](debug-server-camera.md) | Editor-camera state, look-at controls and framing model bounds |
| [debug-server-requests.md](debug-server-requests.md) | Mutation identities, retained results, request status, cancellation and deadlines |
| [debug-server-import.md](debug-server-import.md) | Local file imports, source copies, live loading and re-imports |
| [debug-server-history.md](debug-server-history.md) | Batch undo/redo, restoration rules, level save/save-as and unsaved changes |
| [debug-server-observation.md](debug-server-observation.md) | Rendered captures, matching camera metadata, entity observations, and incremental logs |
| [debug-server-scene-camera.md](debug-server-scene-camera.md) | Perspective Camera editing, game-camera selection, history and persistence |
| [debug-server-physics.md](debug-server-physics.md) | Collider and RigidBody authoring, creation on play, history and verification |
| [debug-server-todo.md](debug-server-todo.md) | Prioritized tracker for 3D scene authoring and observation through the debug server |
| [data-and-projects.md](data-and-projects.md) | The `data/` tree, project structure, asset path resolution |

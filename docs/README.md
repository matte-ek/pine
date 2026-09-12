# Pine docs

One-page orientation docs per subsystem — key files, how the pieces fit, and the gotchas.
They point you to the right place; they are intentionally shallow, not exhaustive manuals.
Start with the root [`AGENTS.md`](../AGENTS.md) for the big picture, then dive into the
relevant page below.

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

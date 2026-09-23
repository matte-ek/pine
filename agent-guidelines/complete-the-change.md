# Complete the change

Before reporting code work as done, trace the change through the integration points the project actually uses, and ask: **What would a developer most likely forget when implementing this?**

## Trace the feature end to end

Where they apply to the change, check that:

- **Registrations** exist, so the new piece is reachable through its intended path. Most of Pine's registration is written by hand. A component needs its `ComponentType` entry, its `CreateComponentDataBlock<T>()` call in `Components::Setup`, and the managed enum in `ScriptRuntime/World/Component.cs` (the full list is in [`docs/world-ecs.md`](../docs/world-ecs.md)). An engine function C# calls needs a `Bindings::Register` in `Script/Interfaces/` and the matching field in `ScriptRuntime/Core/Bindings/`. Check for convention-based discovery before concluding that a registration is missing: `CMakeLists.txt` globs its sources, for example.
- **Serialized state** round-trips. A new component or asset field is declared in its `Serializer`, written in `SaveData` and read back in `LoadData`. Levels and Blueprints saved before the change still load.
- **Lifecycles** are closed. Whatever `OnCreated`, `Setup` or a lazy first use acquires is released in `OnDestroyed`, `Shutdown` or `Dispose`. Pooled components never run their destructors (see [`docs/world-ecs.md`](../docs/world-ecs.md)).
- **Configuration** is complete. A new `EngineConfiguration` field has a sensible default, and each app that needs a different value sets it in its own `Application.cpp`.
- **Related surfaces** are updated. That includes the editor's properties renderer for a new component field, the C# API when scripts should see the change, affected callers, and the subsystem's page in `docs/`.

Let the project and the feature decide which of these apply. Not every change needs every one.

## Finish patterns you started

- When a pattern applies across comparable cases, apply it to all of them. Every existing `OnDestroyed` override calls `Component::OnDestroyed()` first, which releases the paired managed object. A new override that skips that call needs a reason.
- Update paired and mirrored operations together. A field written in `SaveData` needs its read in `LoadData`. A value that crosses into C# needs both the native binding and the managed side that uses it.
- Check related branches, variants, and consumers so the change is not left partially applied. For example, code that branches on `m_ProductionMode` has both an editor path and a game path. The Editor and GameHost also each configure the engine in their own `Application.cpp`.

Establish the applicable pattern from comparable cases rather than assuming every difference is wrong. When a difference looks deliberate but unexplained, raise it with the user instead of quietly changing it.

## Verify what you changed

Build the project and run whatever verification covers the area you touched. Report the outcome, including failures you did not fix and anything you could not run. "Build and verify" in [`AGENTS.md`](../AGENTS.md) says what verification means here.

Match the verification to the change. Prefer the narrowest run that would actually catch a mistake over the full suite, and re-run what your change could plausibly have broken.

Add tests only when the project already has them. Follow the existing tests' structure, naming, and assertion style, and cover the behavior you added or changed. In a project with no tests, do not introduce a test framework as part of another change; say what you would test instead. The debug server's `verify-<area>.py` recipes are the exception in Pine: when you change a route, add to or extend its recipe (see [`docs/debug-server-workflow.md`](../docs/debug-server-workflow.md)).

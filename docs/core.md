# Core utilities

Foundational, engine-wide building blocks in `Engine/src/Pine/Core/`. These are used
everywhere; skim them before writing new low-level code so you reuse rather than reinvent.

## What's here
- **`Math/`** — vectors/matrices/quaternions (`Pine::Vector2/3/4`, `Vector2i`, etc.), built on GLM. `Math.hpp` is the umbrella include.
- **`File/`** — file IO, including `File::ReadCompressed` / `File::WriteCompressed`, which back the `.passet` format (see [assets.md](assets.md)).
- **`Serialization/`** — the reflective binary serializer (`Serializer` + `PINE_SERIALIZE_*` macros) plus a JSON variant under `Serialization/Json/`. Documented in [assets.md](assets.md) since it's mostly used for assets/components.
- **`UId/`** — unique IDs used to reference entities, components and assets across (de)serialization.
- **`Log/`** — logging. Use the macros `PInfo/PWarning/PError/PFatal/PVerbose(msg)` (fmt-formatted), not the functions directly.
- **`WindowManager/`** — the GLFW window + graphics context (`WindowManager::Internal::CreateWindow`, `IsWindowOpen`).
- **`Color/`**, **`String/`**, **`Timer/`**, **`Span/`** (`ByteSpan` for raw asset payloads), **`Assert/`**.

## Conventions worth knowing
- Everything is `namespace Pine` (subsystems get a nested namespace mirroring the folder). Engine-internal entry points sit in a nested `Internal` namespace.
- Prefer these utilities over ad-hoc STL/GLM usage so serialization, IDs and logging stay consistent across the engine.

Related: [assets.md](assets.md) · [rendering.md](rendering.md)

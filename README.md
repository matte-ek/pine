# Pine Engine

A 3D/2D game engine written in C++ (C++17), with C# gameplay scripting on .NET.

The repo builds one `Engine` static library plus three executables that link it:
- **Editor** — the ImGui-based editor.
- **GameHost** — standalone runtime for shipping a game.
- **EngineCli** — command-line tooling.

## Build

Pine builds with **CMake** (the author develops in CLion). Dependencies are a mix of
system packages and libraries bundled in `third-party/` (`imgui`, `material-icons`,
`nvtt`, `perlin-noise`, `physx`).

### Dependencies

Install the system packages through your package manager.

#### Arch
`glfw glew glm assimp stb nlohmann-json fmt freetype2 dotnet-sdk openal libjpeg-turbo libpng`

#### Ubuntu
`libglfw3-dev libglew-dev libglm-dev libassimp-dev libstb-dev nlohmann-json3-dev libfmt-dev libfreetype-dev dotnet-sdk-10.0 libopenal-dev libjpeg-turbo8-dev libpng-dev zlib1g-dev`

#### Windows
You'll have to figure it out yourself. :-)

### PhysX

3D physics uses **PhysX 5**. The prebuilt static libraries are *not* committed to the
repo — they need to live in `third-party/physx/lib/` (`.a`) and `third-party/physx/include/`.
On Arch, `./setup-env.sh` downloads and builds PhysX and installs it there for you.

### Configure & build

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug -j
```

Build a single target with e.g. `--target Editor`.

## Running

The executables load assets relative to the current working directory, so run them from
the `data/` directory and pass a project name (projects live in `data/projects/`):

```bash
cd data
../cmake-build-debug/Editor/Editor <project_name>
```

GameHost runs from the same directory but takes no project name — it reads `data/game/game.json`,
which the Editor's Game Properties panel writes, to find the game's assets and startup level:

```bash
cd data
../cmake-build-debug/GameHost/GameHost
```

Set `PINE_X11=1` to force GLFW onto X11/XWayland (useful on Wayland, e.g. for RenderDoc).

## Scripting runtime

The C# runtime lives in `ScriptRuntime/` and targets **.NET 10**. The engine hosts CoreCLR
itself through `hostfxr`, so a .NET runtime has to be installed to run the editor or a game,
and the SDK to build either side of the C# code. Build `Pine.dll` with the `dotnet` CLI:

```bash
cd ScriptRuntime
dotnet build -c Release
```

The Release build outputs `Pine.dll` (plus `Pine.runtimeconfig.json` and `Pine.deps.json`)
to `data/engine/script/`, where the engine loads it from — the runtimeconfig is what the
host starts the runtime from, so it has to sit beside the DLL.

Each project owns its game assembly: `data/projects/<name>/runtime/Game.csproj` compiles the
`.cs` files in the project's `assets/` tree and outputs `Game.dll` to
`data/projects/<name>/runtime-bin/`, which the Editor loads once a project is open. It builds
the same way (`dotnet build -c Release`), and new projects inherit the csproj from
`data/projects/project-template/runtime/`. GameHost uses `data/game/runtime` instead.

Engine-side C# (`Pine.dll`) is loaded once at boot, so changing it needs an editor restart;
a rebuilt `Game.dll` is hot-reloaded when the editor regains focus. An IDE such as Rider is
recommended for working on the C# side.

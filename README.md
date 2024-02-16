# Pine Engine

A 3D/2D game engine written in C++.

## Build
Install the required dependencies through your package manager.

#### Arch
Install `glfw-x11/glfw-wayland glew glm assimp stb nlohmann-json reactphysics3d fmt freetype2 mono`.

#### Ubuntu
Install `libstb-dev libglew-dev libglfw3-dev nlohmann-json3-dev libglm-dev libfreetype-dev libfmt-dev libassimp-dev libmono-2.0-dev`

#### Windows
You'll have to figure it out yourself. :-)

### Initial Setup / Scripting Engine
In order to get the scripting engine to work, you need to do the following:
* Remove/rename your previous `game` directory in `/asssets`.
* Create a copy of the template `game-template` in `/assets` and rename it to `game`.
* Install mono and msbuild for mono
  * On Arch you can install the packages `mono` and `mono-msbuild`.
* Build the engine and the editor/sandbox
* Build the engine scripting runtime
  * Run `msbuild -t:Build -p:Configuration=Release` in the `/ScriptRuntime` directory.
* Optional: Build the game runtime
  * Run `msbuild -t:Build -p:Configuration=Release` in `/assets/game/runtime` directory.

*Note: To make developing/building the C# libraries easier, an IDE such as Rider is recommended.*
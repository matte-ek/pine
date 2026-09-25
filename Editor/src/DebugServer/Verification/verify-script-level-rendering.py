"""Drive Level.Active.Rendering from C# through the Editor's boot sequence, and check that play mode restores it; needs a Ninja Editor build, the .NET SDK and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import tempfile

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-script-level-rendering.'))
print('Level rendering binding verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# deterministic checks.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/World.hpp"
'''
body = Path(__file__).with_name('script-level-rendering.inc').read_text()
(root / 'probe.cpp').write_text(includes + source.replace(marker, body))
command = shlex.split(entry['command'])
command[command.index('-o') + 1] = str(root / 'probe.o')
command[-1] = str(root / 'probe.cpp')
subprocess.run(command, cwd=build, check=True)

link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'Editor'], text=True).splitlines()[-1]
command = shlex.split(link.removeprefix(': && ').removesuffix(' && :'))
command[command.index('Editor/CMakeFiles/Editor.dir/src/Application.cpp.o')] = str(root / 'probe.o')
command[command.index('-o') + 1] = str(root / 'probe')
subprocess.run(command, cwd=build, check=True)

data = root / 'data'
assets = data / 'projects/levelrendering/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# Reads every property back, then writes a different value to each, so a binding wired in only one
# direction fails here. Level.Active is the untitled level the editor boots into, which was never
# registered; "other" is a registered one, so both ways the engine finds a level are exercised.
(assets / 'LevelRenderingTest.cs').write_text('''using Pine.Assets;
using Pine.Math;
using Pine.World.Components;

namespace Game
{
    public class LevelRenderingTest : Script
    {
        public bool ReadBackOk;

        public void OnStart()
        {
            var level = Level.Active;
            var other = AssetManager.Get<Level>("other");

            if (level == null || other == null)
            {
                return;
            }

            var rendering = level.Rendering;

            ReadBackOk =
                ReferenceEquals(level, Level.Active) &&
                ReferenceEquals(rendering, level.Rendering) &&
                rendering.AmbientColor.X == 0.1f && rendering.AmbientColor.Y == 0.2f && rendering.AmbientColor.Z == 0.3f &&
                rendering.FogColor.X == 0.4f && rendering.FogColor.Y == 0.5f && rendering.FogColor.Z == 0.6f && rendering.FogColor.W == 1.0f &&
                rendering.FogDensity == 0.045f &&
                rendering.FogHeight == 2.5f &&
                rendering.FogHeightFalloff == 0.25f &&
                rendering.Exposure == 1.5f &&
                rendering.BloomThreshold == 2.0f &&
                rendering.BloomIntensity == 0.75f &&
                rendering.GrainStrength == 0.125f &&
                rendering.VignetteStrength == 0.625f &&
                other.Rendering.FogDensity == 0.4f;

            rendering.AmbientColor = new Vector3(0.05f, 0.04f, 0.03f);
            rendering.FogColor = new Vector4(0.02f, 0.02f, 0.03f, 1.0f);
            rendering.FogDensity = 0.12f;
            rendering.FogHeight = -4.0f;
            rendering.FogHeightFalloff = -0.5f;
            rendering.Exposure = 0.8f;
            rendering.BloomThreshold = 3.0f;
            rendering.BloomIntensity = 0.2f;
            rendering.GrainStrength = -0.5f;
            rendering.VignetteStrength = 0.9f;

            other.Rendering.FogDensity = 0.7f;
        }
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. A throwaway SDK project rather
# than the one the real projects carry: a single source file against a single reference needs
# nothing else, and the properties below are the ones the engine's loader depends on - an output
# path with no framework segment in it, and no local copy of Pine.dll for the game's load context
# to find a second time.
runtime = data / 'projects/levelrendering/runtime-bin'
runtime.mkdir(parents=True)
(runtime / 'Game.csproj').write_text(f'''<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <AssemblyName>Game</AssemblyName>
    <OutputPath>./</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="{assets / 'LevelRenderingTest.cs'}" />
    <Reference Include="Pine">
      <HintPath>{data / 'engine/script/Pine.dll'}</HintPath>
      <Private>false</Private>
    </Reference>
  </ItemGroup>
</Project>
''')
subprocess.run(['dotnet', 'build', '-c', 'Release', '--nologo', '--verbosity', 'quiet',
                str(runtime / 'Game.csproj')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(headless_command(root / 'probe', 'levelrendering'), cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=120)
    finally:
        # Also clean up the virtual display when a failing probe exits unexpectedly.
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
print(log_path.read_text()[-4000:])
assert result == 0, 'Level rendering binding verification failed; see ' + str(log_path)

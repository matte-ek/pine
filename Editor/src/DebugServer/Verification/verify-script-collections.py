"""Walk every C# API that hands back a set of objects, through the Editor's boot sequence; needs a Ninja Editor build, the .NET SDK and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import tempfile


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-script-collections.'))
print('Script collection verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# deterministic checks.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-collections.inc').read_text()
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
assets = data / 'projects/scriptcollections/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# Reads each collection and reports what it found through public int fields, which the probe reads
# back natively. Counts alone would pass on an array of nulls, so each one also checks that the
# objects it got back are the right ones and still usable.
(assets / 'CollectionTest.cs').write_text('''using Pine.Math;
using Pine.Physics;
using Pine.World;
using Pine.World.Components;

namespace Game
{
    public class CollectionTest : Script
    {
        public int ChildCount;
        public int ChildrenNamed;
        public int ColliderCount;
        public int CollidersUsable;
        public int TaggedCount;
        public int TaggedNamed;
        public int AllCount;
        public int AllFoundHost;
        public int FoundSelf;
        public int FoundSelfAsScript;
        public int RayCastHits;

        public void OnStart()
        {
            var namedChildren = 0;

            foreach (var child in Parent.Children)
            {
                ChildCount++;

                if (child != null && (child.Name == "ChildA" || child.Name == "ChildB"))
                {
                    namedChildren++;
                }
            }

            ChildrenNamed = namedChildren == ChildCount ? 1 : 0;

            // Two of them, so the count is neither zero nor one, and added from C# so that
            // AddComponent<T> goes through the same type resolution as everything else here.
            Parent.AddComponent<Collider>();
            Parent.AddComponent<Collider>();

            var colliders = Parent.GetComponents<Collider>();
            var usableColliders = 0;

            foreach (var collider in colliders)
            {
                if (collider != null && collider.Parent != null)
                {
                    usableColliders++;
                }
            }

            ColliderCount = colliders.Length;
            CollidersUsable = usableColliders == colliders.Length ? 1 : 0;

            var tagged = EntityList.Find(4ul);
            var taggedNames = 0;

            foreach (var entity in tagged)
            {
                if (entity != null && (entity.Name == "Host" || entity.Name == "Tagged"))
                {
                    taggedNames++;
                }
            }

            TaggedCount = tagged.Length;
            TaggedNamed = taggedNames == tagged.Length ? 1 : 0;

            var all = EntityList.GetAll();

            foreach (var entity in all)
            {
                if (entity != null && entity.Name == "Host")
                {
                    AllFoundHost = 1;
                }
            }

            AllCount = all.Length;

            // A game script class is a Script, so asking for one by its own type has to find the
            // script instance rather than whatever component happens to sit first.
            FoundSelf = Parent.GetComponent<CollectionTest>() != null ? 1 : 0;
            FoundSelfAsScript = Parent.GetScript<CollectionTest>() != null ? 1 : 0;

            RayCastHits = Physics3D.RayCast(new Vector3(0.0f, 10.0f, 0.0f), new Vector3(0.0f, -1.0f, 0.0f), 100.0f, -1).Length;
        }
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. A throwaway SDK project rather
# than the one the real projects carry: a single source file against a single reference needs
# nothing else, and the properties below are the ones the engine's loader depends on - an output
# path with no framework segment in it, and no local copy of Pine.dll for the game's load context
# to find a second time.
runtime = data / 'projects/scriptcollections/runtime-bin'
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
    <Compile Include="{assets / 'CollectionTest.cs'}" />
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
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptcollections'], cwd=data,
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
assert result == 0, 'Script collection verification failed; see ' + str(log_path)

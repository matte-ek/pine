"""Destroy an entity and a component a running script still holds, and check it can tell; needs a Ninja Editor build, the .NET SDK and Xvfb."""

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
root = Path(tempfile.mkdtemp(prefix='pine-script-lifetime.'))
print('Script lifetime verification:', root, flush=True)

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
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-lifetime.inc').read_text()
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
assets = data / 'projects/scriptlifetime/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# Holds an entity and one of its components past the point where the engine destroys them, and
# reports what each one looks like from C# afterwards. Its own entity is the control: it outlives
# the others, so a run where everything reads as gone fails too.
(assets / 'LifetimeTest.cs').write_text('''using Pine.World;
using Pine.World.Components;

namespace Game
{
    public class LifetimeTest : Script
    {
        public int WatchedSeen;
        public int WatchedValid;
        public int CameraActive;
        public int SelfValid;
        public int SelfIdValid;

        private Entity _watched;
        private Camera _camera;

        public void OnStart()
        {
            foreach (var entity in EntityList.GetAll())
            {
                if (entity != null && entity.Name == "Watched")
                {
                    _watched = entity;
                    _camera = entity.GetComponent<Camera>();
                }
            }

            WatchedSeen = _watched != null && _camera != null ? 1 : 0;

            Observe();
        }

        public void OnUpdate(float deltaTime)
        {
            Observe();
        }

        private void Observe()
        {
            WatchedValid = _watched != null && _watched.IsValid ? 1 : 0;
            CameraActive = _camera != null && _camera.Active ? 1 : 0;
            SelfValid = Parent != null && Parent.IsValid ? 1 : 0;
            SelfIdValid = Parent != null && Parent.Id.IsValid ? 1 : 0;
        }
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. A throwaway SDK project rather
# than the one the real projects carry: a single source file against a single reference needs
# nothing else, and the properties below are the ones the engine's loader depends on - an output
# path with no framework segment in it, and no local copy of Pine.dll for the game's load context
# to find a second time.
runtime = data / 'projects/scriptlifetime/runtime-bin'
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
    <Compile Include="{assets / 'LifetimeTest.cs'}" />
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
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptlifetime'], cwd=data,
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
assert result == 0, 'Script lifetime verification failed; see ' + str(log_path)

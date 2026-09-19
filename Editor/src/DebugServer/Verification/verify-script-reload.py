"""Hot reload the game assembly thirty times in a row and check every context unloads; needs a Ninja Editor build, the .NET SDK and Xvfb."""

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
parser.add_argument('--build', type=Path, default=repo / 'build')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-script-reload.'))
print('Script reload verification:', root, flush=True)

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
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-reload.inc').read_text()
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
assets = data / 'projects/scriptreload/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# Between them these four hold every kind of reference an unload has to get back: a mirror of an
# engine object (entity, component, asset), an instance of another class in this same assembly, a
# class that derives from another one here, and the stack trace of an exception thrown in it.
(assets / 'Holder.cs').write_text('''using Pine.Assets;
using Pine.World;
using Pine.World.Components;

namespace Game
{
    public class Holder : Script
    {
        public float Speed = 1.5f;
        public string Label = "unset";
        public Model Prop;
        public int Acquired;
        public int Ticks;

        private Entity _watched;
        private Camera _camera;
        private Model _asset;
        private Ticker _peer;

        public void OnStart()
        {
            Acquire();
        }

        public void OnUpdate(float deltaTime)
        {
            // A reload rebuilds this instance without running OnStart again, so each new one picks
            // its references up on its first frame instead.
            if (_watched == null)
            {
                Acquire();
            }

            Ticks++;
        }

        private void Acquire()
        {
            _watched = EntityList.Find("Watched");
            _camera = _watched?.GetComponent<Camera>();
            _asset = AssetManager.Get<Model>("engine/primitive/cube");

            // The one reference of the four that lives in this assembly rather than in Pine.dll,
            // and the only one that pins it if the reload misses it.
            _peer = EntityList.Find("Ticker")?.GetScript<Ticker>();

            Acquired = _watched != null && _camera != null && _asset != null && _peer != null ? 1 : 0;
        }
    }
}
''')

(assets / 'DerivedHolder.cs').write_text('''namespace Game
{
    public class DerivedHolder : Holder
    {
        public float Extra = 2.5f;
    }
}
''')

(assets / 'Ticker.cs').write_text('''using Pine.World.Components;

namespace Game
{
    public class Ticker : Script
    {
        public int Ticks;

        public void OnUpdate(float deltaTime)
        {
            Ticks++;
        }
    }
}
''')

# Throws once per instance, so every reload produces a fresh exception out of the new assembly.
# The counter is private and unreflected on purpose: a public one would be captured and restored
# across the reload, and the new instance would never throw at all.
(assets / 'Thrower.cs').write_text('''using System;
using Pine.World.Components;

namespace Game
{
    public class Thrower : Script
    {
        public int Attempts;

        private int _thrown;

        public void OnUpdate(float deltaTime)
        {
            Attempts++;

            if (_thrown > 0)
            {
                return;
            }

            _thrown++;

            throw new InvalidOperationException("Deliberate: the probe checks a thrown exception "
                                                + "does not keep the assembly loaded.");
        }
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. A throwaway SDK project rather
# than the one the real projects carry: a handful of source files against a single reference needs
# nothing else, and the properties below are the ones the engine's loader depends on - an output
# path with no framework segment in it, and no local copy of Pine.dll for the game's load context
# to find a second time.
runtime = data / 'projects/scriptreload/runtime-bin'
runtime.mkdir(parents=True)
sources = '\n'.join(f'    <Compile Include="{assets / name}" />'
                    for name in ['Holder.cs', 'DerivedHolder.cs', 'Ticker.cs', 'Thrower.cs'])
(runtime / 'Game.csproj').write_text(f'''<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <AssemblyName>Game</AssemblyName>
    <OutputPath>./</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
  </PropertyGroup>
  <ItemGroup>
{sources}
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
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptreload'], cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=600)
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
assert result == 0, 'Script reload verification failed; see ' + str(log_path)

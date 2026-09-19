"""Round-trip every stored script field type through the Editor's boot sequence; needs a Ninja Editor build, Mono and Xvfb."""

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
root = Path(tempfile.mkdtemp(prefix='pine-script-fields.'))
print('Script field verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# deterministic checks. Nothing here needs the debug server, so none of it is a production endpoint.
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
#include <vector>
#include <mono/metadata/object.h>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-fields.inc').read_text()
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

# A disposable data directory, so the authored level and the created assets never reach the repo.
data = root / 'data'
assets = data / 'projects/scriptfields/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# One public field of every type the engine stores, plus the two it deliberately does not: an
# Entity, which is reflected but has nowhere to be stored yet, and a double, which should not be
# reflected at all.
(assets / 'FieldTest.cs').write_text('''using Pine.Assets;
using Pine.Math;
using Pine.World;
using Pine.World.Components;

namespace Game
{
    public class FieldTest : Script
    {
        public bool Flag;
        public int Count;
        public float Speed;
        public Vector2 Offset;
        public Vector3 Origin;
        public Vector4 Tint;
        public string Label;
        public Model Prop;
        public Entity Target;

        public double Unsupported;
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. mcs rather than the project's
# msbuild file, because one source file against one reference does not need a csproj.
runtime = data / 'projects/scriptfields/runtime-bin'
runtime.mkdir(parents=True)
subprocess.run(['mcs', '-target:library', '-out:' + str(runtime / 'Game.dll'),
                '-r:' + str(data / 'engine/script/Pine.dll'), str(assets / 'FieldTest.cs')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptfields'], cwd=data,
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
assert result == 0, 'Script field verification failed; see ' + str(log_path)

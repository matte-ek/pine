"""Drive Light, Camera and Collider from C# through the Editor's boot sequence; needs a Ninja Editor build, Mono and Xvfb."""

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
root = Path(tempfile.mkdtemp(prefix='pine-script-components.'))
print('Script component binding verification:', root, flush=True)

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
#include <mono/metadata/object.h>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-components.inc').read_text()
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
assets = data / 'projects/scriptcomponents/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# Reads every bound property back, then writes to a different one of each, so a binding that is
# wired in only one direction fails here rather than looking fine.
(assets / 'ComponentTest.cs').write_text('''using Pine.Math;
using Pine.World.Components;

namespace Game
{
    public class ComponentTest : Script
    {
        public bool ReadBackOk;

        public void OnStart()
        {
            var light = Parent.GetComponent<Light>();
            var camera = Parent.GetComponent<Camera>();
            var collider = Parent.GetComponent<Collider>();

            ReadBackOk =
                light.LightType == LightType.SpotLight &&
                light.LightIntensity == 2.5f &&
                light.Range == 12.0f &&
                !light.CastShadows &&
                camera.FieldOfView == 80.0f &&
                camera.FarPlane == 500.0f &&
                collider.ColliderType == ColliderType.Sphere &&
                collider.IsTrigger &&
                collider.Layer == 4u;

            light.LightColor = new Vector3(0.25f, 0.5f, 0.75f);
            light.LightIntensity = 4.0f;
            light.SpotlightOuterAngle = 60.0f;

            camera.FieldOfView = 55.0f;
            camera.CameraType = CameraType.Orthographic;

            collider.Size = new Vector3(2.0f, 3.0f, 4.0f);
            collider.LayerMask = 12u;
        }
    }
}
''')

runtime = data / 'projects/scriptcomponents/runtime-bin'
runtime.mkdir(parents=True)
subprocess.run(['mcs', '-target:library', '-out:' + str(runtime / 'Game.dll'),
                '-r:' + str(data / 'engine/script/Pine.dll'), str(assets / 'ComponentTest.cs')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptcomponents'], cwd=data,
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
assert result == 0, 'Script component binding verification failed; see ' + str(log_path)

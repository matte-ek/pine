"""Build a native terrain physics probe with the Editor's boot sequence; requires a Ninja Editor build and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-terrain-physics.'))
print('Terrain physics verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its
# main-loop call with deterministic checks. No production test endpoint is needed.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
#include "physx/PxPhysicsAPI.h"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Other/TerrainSculpting/TerrainSculpting.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Physics/Physics3D/TerrainCollision/TerrainCollision.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-physics.inc').read_text()
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

# A disposable data directory, because the editor rewrites files under it. Copied with cp -a to
# keep timestamps: anything that looks newer than its source triggers a hot-reload re-import of
# every engine asset.
data = root / 'data'
(data / 'projects/terrain/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'terrain-physics.log'
with log_path.open('w') as log:
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'terrain'], cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=300)
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
assert result == 0, 'Terrain physics verification failed; see ' + str(log_path)

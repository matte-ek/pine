"""Build a native physics probe with the Editor's boot sequence; requires a Ninja Editor build and Xvfb."""

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
root = Path(tempfile.mkdtemp(prefix='pine-physics-native.'))
print('Native physics verification:', root, flush=True)

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
#include <stdexcept>
#include "DebugServer/Editing/Editing.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
'''
body = Path(__file__).with_name('physics-native.inc').read_text()
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

# Keep authored levels, editor settings and any rewritten assets in a disposable copy.
data = root / 'data'
(data / 'projects/physics/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)
environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'physics'], cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=60)
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
assert result == 0, 'Native physics verification failed; see ' + str(log_path)

"""Build a native blending probe and read a blend off the frame; requires a Ninja Editor build and Xvfb."""

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
root = Path(tempfile.mkdtemp(prefix='pine-blending-native.'))
print('Native blending verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe: a scene /edit cannot build - nothing creates a material over HTTP - and then the same
# Run(), because what is being checked only exists once frames have been rendered.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Rendering/DrawList/DrawList.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Rendering/RenderHandler.hpp"
'''
body = Path(__file__).with_name('blending-native.inc').read_text()
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

# A disposable copy of the engine and editor assets. The saved panel layout comes with it: without
# it the Level viewport has no size and there is no frame to read a pixel out of.
data = root / 'data'
(data / 'projects/blending/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)
shutil.copy2(repo / 'data/imgui.ini', data / 'imgui.ini')

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(headless_command(root / 'probe', 'blending'), cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=240)
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
assert result == 0, 'Native blending verification failed; see ' + str(log_path)

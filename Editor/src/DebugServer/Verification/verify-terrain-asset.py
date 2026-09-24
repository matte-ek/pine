"""Build a native terrain probe, then check the layout bounds and what a save and a load preserve; requires a Ninja Editor build and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-terrain-asset.'))
print('Terrain asset verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe. Everything here is plain CPU work over an asset, so the probe runs its checks and
# exits rather than going on to serve the debug server - but it still needs a booted engine, since
# creating and registering an asset goes through the asset manager.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
'''
body = Path(__file__).with_name('terrain-asset.inc').read_text()
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

# A disposable data directory, copied with cp -a to keep timestamps: anything that looks newer than
# its source triggers a hot-reload re-import of every engine asset. The project's asset directory
# has to exist before the probe creates an asset under it, or the write silently goes nowhere.
data = root / 'data'
(data / 'projects/terrain/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
result = subprocess.run(headless_command(root / 'probe', 'terrain'), cwd=data,
                        env=environment, capture_output=True, text=True, timeout=300)

if result.returncode != 0 or 'PASS(native)' not in result.stdout:
    print((result.stdout + result.stderr)[-4000:])
    raise SystemExit('FAIL: the terrain asset probe did not pass')

print(next(line for line in result.stdout.splitlines() if line.startswith('PASS(native)')).replace('(native)', ''))
print('\n'.join(line for line in result.stdout.splitlines()
                if line and not line.startswith('PASS(native)') and line.startswith('              ')))

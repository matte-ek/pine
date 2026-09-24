"""Build a native audio probe, then check the Audio subsystem drives listener and sources; requires a Ninja Editor build and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-audio-playback.'))
print('Audio playback verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe. Playback needs a booted engine and real time passing, so the probe drives Audio::Update()
# itself rather than relying on the main loop it is standing in for.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <AL/al.h>
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Audio/Audio.hpp"
#include "Pine/Audio/OpenAL/Source/ALSource.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('audio-playback.inc').read_text()
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
# its source triggers a hot-reload re-import of every engine asset.
data = root / 'data'
(data / 'projects/audio/assets').mkdir(parents=True)
(data / 'projects/audio/content').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

# The null OpenAL backend gives a real context, real sources and a mixer running at the real sample
# rate, with no output device - which is what lets playback be timed on a machine with no sound card.
environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
result = subprocess.run(['xvfb-run', '-a', str(root / 'probe'), 'audio'], cwd=data,
                        env=environment, capture_output=True, text=True, timeout=300)

if result.returncode != 0 or 'PASS(native)' not in result.stdout:
    print((result.stdout + result.stderr)[-4000:])
    raise SystemExit('FAIL: the audio playback probe did not pass')

print(next(line for line in result.stdout.splitlines() if line.startswith('PASS(native)')).replace('(native)', ''))
print('\n'.join(line for line in result.stdout.splitlines()
                if line and not line.startswith('PASS(native)') and line.startswith('              ')))

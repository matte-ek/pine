"""Build a native probe, then check that saving an already-imported asset keeps the bulk data it no longer holds in memory; requires a Ninja Editor build and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-asset-resave.'))
print('Asset re-save verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe. It re-saves assets the engine and editor ship with, so it needs a booted engine with
# those loaded - the whole point is the state of an asset that was loaded rather than just
# imported, since that is the one whose payload lives on the GPU or the audio device.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/File/File.hpp"
'''
body = Path(__file__).with_name('asset-resave.inc').read_text()
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
# its source triggers a hot-reload re-import of every engine asset, which would import the very
# assets this probe means to re-save without importing. It writes over the assets it is given, so
# it must never be pointed at the repository's own data directory.
data = root / 'data'
(data / 'projects/resave/assets').mkdir(parents=True)
(data / 'projects/resave/content').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

# The diffuse map of the model the probe imports: a hard alpha mask, left half clear and right half
# solid, so its material is imported into the discard pass rather than left at the default.
def png_chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))

size = 8
clear_pixel = bytes([255, 255, 255, 0])
solid_pixel = bytes([255, 255, 255, 255])
row = b'\x00' + clear_pixel * (size // 2) + solid_pixel * (size // 2)
header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
(data / 'projects/resave/content/probe.png').write_bytes(
    b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', header) + png_chunk(b'IDAT', zlib.compress(row * size)) +
    png_chunk(b'IEND', b''))

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
result = subprocess.run(headless_command(root / 'probe', 'resave'), cwd=data,
                        env=environment, capture_output=True, text=True, timeout=300)

if result.returncode != 0 or 'PASS(native)' not in result.stdout:
    print((result.stdout + result.stderr)[-4000:])
    raise SystemExit('FAIL: the asset re-save probe did not pass')

print(next(line for line in result.stdout.splitlines() if line.startswith('PASS(native)')).replace('(native)', ''))
print('\n'.join(line for line in result.stdout.splitlines()
                if line and not line.startswith('PASS(native)') and line.startswith('              ')))

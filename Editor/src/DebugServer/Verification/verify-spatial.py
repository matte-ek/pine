"""Check spatial queries and camera-framing compatibility in a disposable project; requires a Ninja Editor build and Xvfb."""

import argparse
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'build')
parser.add_argument('--port', type=int, default=19041)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-spatial-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-spatial.'))
url = 'http://127.0.0.1:%d' % args.port
print('Spatial verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe: scene setup /edit cannot express, then the same Run() so the debug server still serves.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "DebugServer/Spatial/Spatial.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
'''
body = Path(__file__).with_name('spatial-native.inc').read_text()
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

# Preserve asset timestamps to avoid unnecessary re-imports. Use the verification
# layout for the existing camera-framing checks, leaving the user's settings alone.
data = root / 'data'
(data / 'projects/terrain/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'terrain.log'
log = log_path.open('w')
process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'terrain'], cwd=data,
                           env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)


def shutdown():
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def request(path, body=None, expected=200, headers=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url + path, data=data,
                                 headers={'Content-Type': 'application/json', **(headers or {})})
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        return json.loads(payload)


def edit(operations):
    return request('/edit', {'version': 1, 'operations': operations})


def vector(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def measure(ids, **options):
    return request('/spatial/query', {'entities': [{'id': item} for item in ids], **options})


def close_vector(actual, expected):
    for axis, value in zip('xyz', expected):
        assert abs(actual[axis] - value) < 0.0001, (actual, expected)


try:
    deadline = time.monotonic() + 60
    while True:
        if process.poll() is not None:
            raise RuntimeError('Probe exited before the HTTP checks')
        try:
            request('/status')
            break
        except (OSError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(.1)

    created = edit([
        {'op': 'entity.create', 'ref': 'parent', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vector(10, 1, 0),
             'LocalScale': vector(2, 3, 1),
             'LocalRotation': {'x': 0, 'y': 0, 'z': 2**-.5, 'w': 2**-.5}}}]},
        {'op': 'entity.create', 'ref': 'box', 'parent': {'ref': 'parent'}, 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vector(1, 2, 3), 'LocalScale': vector(-1, 2, 3)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.create', 'ref': 'empty'},
        {'op': 'entity.create', 'ref': 'unassigned', 'components': [{'type': 'ModelRenderer'}]},
    ])
    parent, box, empty, unassigned = [created['refs'][name] for name in ['parent', 'box', 'empty', 'unassigned']]
    box_transform = created['results'][1]['entity']['components'][0]['id']
    baseline_camera = request('/camera')['state']
    baseline_history = request('/history')
    baseline_level = request('/level/status')
    baseline_entities = request('/entities')

    result = measure([parent, box, empty, unassigned])
    assert result['sceneGeneration'] == baseline_history['sceneGeneration']
    entries = result['entities']
    assert [entry['id'] for entry in entries] == [parent, box, empty, unassigned]
    assert entries[0]['bounds'] == entries[1]['bounds'] == result['combinedBounds']
    assert entries[2]['bounds'] is None and entries[3]['bounds'] is None
    close_vector(entries[1]['bounds']['dimensions'], (12, 4, 6))
    close_vector(entries[1]['bounds']['center'], (11, 3, 3))
    close_vector(entries[1]['localTransform']['position'], (1, 2, 3))
    close_vector(entries[1]['worldTransform']['position'], (11, 3, 3))
    close_vector(entries[1]['worldTransform']['scale'], (-2, 6, 3))
    close_vector(entries[1]['right'], (0, 1, 0))
    close_vector(entries[1]['up'], (-1, 0, 0))
    close_vector(entries[1]['forward'], (0, 0, -1))
    assert measure([parent], includeChildren=False)['combinedBounds'] is None
    assert measure([empty])['combinedBounds'] is None
    assert len(measure([box] * 128)['entities']) == 128

    # The read is bounded, strict, and rejects missing/stale/editor references as a whole.
    for body in [{}, {'entities': []}, {'entities': 'bad'}, {'entities': [{'id': box}] * 129},
                 {'entities': [{'id': box}], 'includeChildren': 1},
                 {'entities': [{'id': box}], 'unexpected': True},
                 {'entities': [{'id': box}, {'id': 'not-an-id'}]},
                 {'entities': [{'ref': 'box'}]}, {'entities': [{'id': None}]},
                 {'entities': [{'id': box, 'extra': True}]},
                 {'entities': [{'id': box}], 'extra': 'x' * 16384},
                 {'entities': [[[[[[[[[[[{'id': box}]]]]]]]]]]]}]:
        request('/spatial/query', body, expected=400)
    request('/spatial/query?extra=1', {'entities': [{'id': box}]}, expected=400)
    request('/spatial/query', {'entities': [{'id': box}]}, expected=400,
            headers={'Idempotency-Key': 'read', 'X-Pine-Session': request('/requests')['session']})
    temporary = next(item['id'] for item in baseline_entities['entities'] if item['temporary'])
    request('/spatial/query', {'entities': [{'id': temporary}]}, expected=400)
    assert request('/camera')['state'] == baseline_camera
    assert request('/history') == baseline_history
    assert request('/level/status') == baseline_level
    assert request('/entities') == baseline_entities

    # An immediate read sees edits, including inactive/static models; no capture or sleep.
    edit([{'op': 'entity.update', 'target': {'id': box}, 'properties': {'static': True, 'active': False}},
          {'op': 'component.update', 'target': {'id': box_transform},
           'properties': {'LocalPosition': vector(4, 5, 6)}}])
    changed = measure([box])['entities'][0]['bounds']
    close_vector(changed['center'], (14, 6, 6))
    framed = request('/camera/frame', {'entities': [{'id': box}], 'includeChildren': False})
    assert framed['framedBounds'] == {key: changed[key] for key in ['min', 'max']}
    request('/camera', baseline_camera)
    edit([{'op': 'entity.delete', 'target': {'id': empty}}])
    request('/spatial/query', {'entities': [{'id': box}, {'id': empty}]}, expected=400)

    # Exercise the existing framing recipe against the extracted shared calculation.
    subprocess.run(['python3', str(Path(__file__).with_name('verify-reparent.py')),
                    '--url', url, '--output', str(args.output / 'reparent')], check=True)
    assert 'PASS(native)' in log_path.read_text()
    print('PASS: batched bounds/transforms, hierarchy, terrain, validation, read-only state and framing compatibility.')
finally:
    shutdown()
    log.close()
    print('Editor log:', log_path)

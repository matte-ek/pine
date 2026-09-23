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
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
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
includes = '''#include <GL/glew.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "DebugServer/Spatial/Spatial.hpp"
#include "DebugServer/Spatial/Queries/Queries.hpp"
#include "DebugServer/Editing/Values/Values.hpp"
#include "DebugServer/Editing/Editing.hpp"
#include "DebugServer/Editing/History/History.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
'''
body = Path(__file__).with_name('spatial-native.inc').read_text()
body = body.replace(marker, Path(__file__).with_name('spatial-intersections-native.inc').read_text())
body = body.replace(marker, Path(__file__).with_name('placement-native.inc').read_text())
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

    ray = {'origin': vector(11, 3, 10), 'direction': vector(0, 0, -5), 'maxDistance': 10}
    hit = request('/spatial/raycast', ray)
    assert hit['sceneGeneration'] == result['sceneGeneration']
    assert hit['hit']['entity'] == box and hit['hit']['geometry'] == 'model-surface'
    close_vector(hit['hit']['position'], (11, 3, 6))
    close_vector(hit['hit']['normal'], (0, 0, 1))
    assert abs(hit['hit']['distance'] - 4) < .0001
    tiny_direction = request('/spatial/raycast', {**ray, 'direction': vector(0, 0, -1e-200)})
    assert tiny_direction['hit'] == hit['hit']
    assert request('/spatial/raycast', {**ray, 'maxDistance': 3})['hit'] is None
    assert request('/spatial/raycast', {**ray, 'maxDistance': 4})['hit'] is not None
    assert request('/spatial/raycast', {**ray, 'exclude': [{'id': parent}]})['hit'] is None
    inside = request('/spatial/raycast', {**ray, 'origin': vector(11, 3, 3)})['hit']
    close_vector(inside['position'], (11, 3, 0))
    close_vector(inside['normal'], (0, 0, 1))
    overlap = {'bounds': {'min': vector(10, 2, 2), 'max': vector(12, 4, 4)}}
    matches = request('/spatial/overlap', overlap)
    assert matches['geometry'] == 'world-bounds' and matches['total'] == 1 and not matches['truncated']
    assert matches['entities'][0]['id'] == box
    assert request('/spatial/overlap', {**overlap, 'exclude': [{'id': parent}]})['total'] == 0
    touching = {'bounds': {'min': vector(11, 3, 6), 'max': vector(11, 3, 6)}}
    assert request('/spatial/overlap', touching)['total'] == 1

    for path, payload in [('/spatial/raycast', ray), ('/spatial/overlap', overlap)]:
        for change in [{'unexpected': True}, {'includeInactive': 1}, {'exclude': 'bad'},
                       {'exclude': [{'id': temporary}]}, {'exclude': [{'id': 'missing'}]},
                       {'exclude': [{'ref': 'parent'}]}, {'exclude': [{'id': box}] * 129},
                       {'unexpected': 'x' * 16384}]:
            request(path, {**payload, **change}, expected=400)
        request(path, {}, expected=400)
        request(path + '?extra=1', payload, expected=400)
        request(path, payload, expected=400,
                headers={'Idempotency-Key': 'read', 'X-Pine-Session': request('/requests')['session']})
    for change in [{'direction': vector(0, 0, 0)}, {'direction': vector(True, 0, 1)},
                   {'origin': vector(1e13, 0, 0)}, {'origin': {'x': 0, 'y': 0}},
                   {'maxDistance': 0}, {'maxDistance': -1}, {'maxDistance': '10'}]:
        request('/spatial/raycast', {**ray, **change}, expected=400)
    for change in [{'limit': 0}, {'limit': 129}, {'limit': 1.5}, {'limit': True},
                   {'bounds': {'min': vector(1, 1, 1), 'max': vector(0, 0, 0)}}]:
        request('/spatial/overlap', {**overlap, **change}, expected=400)
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
    moved_ray = {**ray, 'origin': vector(14, 6, 12)}
    assert request('/spatial/raycast', moved_ray)['hit'] is None
    moved_hit = request('/spatial/raycast', {**moved_ray, 'includeInactive': True})['hit']
    close_vector(moved_hit['position'], (14, 6, 9))
    assert request('/spatial/overlap', overlap)['total'] == 0
    assert request('/spatial/overlap', {**overlap, 'includeInactive': True})['total'] == 1
    framed = request('/camera/frame', {'entities': [{'id': box}], 'includeChildren': False})
    assert framed['framedBounds'] == {key: changed[key] for key in ['min', 'max']}
    request('/camera', baseline_camera)
    edit([{'op': 'entity.delete', 'target': {'id': empty}}])
    request('/spatial/query', {'entities': [{'id': box}, {'id': empty}]}, expected=400)
    request('/spatial/raycast', {**ray, 'exclude': [{'id': empty}]}, expected=400)

    # Nearest surface selection and explicit truncation, with overlapping instances.
    extra = edit([{'op': 'entity.create', 'ref': str(i), 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(100, 0, i * 3)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]} for i in range(3)])
    nearest = request('/spatial/raycast', {'origin': vector(100, 0, 15), 'direction': vector(0, 0, -1), 'maxDistance': 20})
    assert nearest['hit']['entity'] == extra['refs']['2']
    many = request('/spatial/overlap', {'bounds': {'min': vector(98, -2, -2), 'max': vector(102, 2, 8)}, 'limit': 1})
    assert many['total'] == 3 and many['truncated'] and len(many['entities']) == 1
    edit([{'op': 'entity.delete', 'target': {'id': item}} for item in extra['refs'].values()])

    # Exercise the existing framing recipe against the extracted shared calculation.
    subprocess.run(['python3', str(Path(__file__).with_name('verify-reparent.py')),
                    '--url', url, '--output', str(args.output / 'reparent')], check=True)
    subprocess.run(['python3', str(Path(__file__).with_name('verify-schema.py')),
                    '--url', url, '--output', str(args.output / 'schema')], check=True)
    subprocess.run(['python3', str(Path(__file__).with_name('verify-placement.py')),
                    '--url', url, '--output', str(args.output / 'placement'), '--terrain'], check=True)
    subprocess.run(['python3', str(Path(__file__).with_name('verify-aim.py')),
                    '--url', url, '--output', str(args.output / 'aim')], check=True)
    assert 'PASS(native)' in log_path.read_text()
    assert 'PASS(intersections)' in log_path.read_text()
    assert 'PASS(placement)' in log_path.read_text()
    print('PASS: spatial measurements, raycasts, overlaps, hierarchy, terrain, validation, read-only state and framing compatibility.')
finally:
    shutdown()
    log.close()
    print('Editor log:', log_path)

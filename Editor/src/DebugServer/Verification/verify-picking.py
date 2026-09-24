"""Check capture surface picking in a disposable project; requires a Ninja Editor build and Xvfb."""

import argparse
import base64
import json
import math
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19042)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-picking-results'))
parser.add_argument('--check-expiry', action='store_true', help='Also wait for and verify the 120-second capture expiry.')
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-picking.'))
url = 'http://127.0.0.1:%d' % args.port
print('Picking verification:', root, flush=True)

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
#include "DebugServer/Picking/Picking.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
'''
body = Path(__file__).with_name('picking-native.inc').read_text()
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
# layout, floating Game separately so both camera contexts can render.
data = root / 'data'
(data / 'projects/picking/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
layout = Path(__file__).with_name('verification-layout.ini').read_text()
sections = layout.split('\n\n')
for index, section in enumerate(sections):
    if section.startswith('[Window]') and ' Game]' in section:
        sections[index] = '\n'.join(line for line in section.splitlines() if not line.startswith('DockId='))
(data / 'imgui.ini').write_text('\n\n'.join(sections))

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'picking.log'
log = log_path.open('w')
process = subprocess.Popen(headless_command(root / 'probe', 'picking'), cwd=data,
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
    data = body if isinstance(body, bytes) else None if body is None else json.dumps(body).encode()
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


def observe(name, width=501, **options):
    observation = request('/observe', {'width': width, 'picking': True, **options})
    assert observation['picking']['width'] == observation['image']['width']
    assert observation['picking']['height'] == observation['image']['height']
    (args.output / (name + '.png')).write_bytes(base64.b64decode(observation['image']['data']))
    metadata = {**observation, 'image': {k: v for k, v in observation['image'].items() if k != 'data'}}
    (args.output / (name + '.json')).write_text(json.dumps(metadata, indent=2))
    return observation


def pick(observation, x=None, y=None, expected=200):
    image = observation['image']
    return request('/pick', {'capture': observation['picking']['capture'], 'pixel': {
        'x': image['width'] // 2 if x is None else x,
        'y': image['height'] // 2 if y is None else y}}, expected=expected)


def components(created, index=0):
    return {component['type']: component['id'] for component in created['results'][index]['entity']['components']}


def dot(left, right):
    return sum(left[axis] * right[axis] for axis in 'xyz')


def check_wall_hit(result, entity, normal, plane_offset):
    hit = result['hit']
    assert hit['entity'] == entity, result
    assert hit['meshIndex'] == 0, result
    assert abs(dot(hit['position'], normal) - plane_offset) < .003, result
    assert dot(hit['normal'], normal) > .999, result
    assert abs(dot(hit['normal'], hit['normal']) - 1) < 1e-6, result


def check_projection(observation, result):
    point = [result['hit']['position'][axis] for axis in 'xyz'] + [1]
    for key in ['viewMatrix', 'projectionMatrix']:
        matrix = observation['camera'][key]
        point = [sum(matrix[column][row] * point[column] for column in range(4)) for row in range(4)]
    x = (point[0] / point[3] + 1) * observation['image']['width'] / 2 - .5
    y = (1 - point[1] / point[3]) * observation['image']['height'] / 2 - .5
    assert abs(x - result['pixel']['x']) < .001, (x, result)
    assert abs(y - result['pixel']['y']) < .001, (y, result)

try:
    deadline = time.monotonic() + 90
    while True:
        if process.poll() is not None:
            raise RuntimeError('Probe exited before the HTTP checks; see ' + str(log_path))
        try:
            request('/status')
            break
        except (OSError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(.1)

    angle = math.radians(25)
    normal = vector(math.sin(angle), 0, math.cos(angle))
    created = edit([
        {'op': 'entity.create', 'ref': 'parent', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vector(1, 2, 0),
             'LocalScale': vector(2, 1.5, 1),
             'LocalRotation': {'x': 0, 'y': math.sin(angle / 2), 'z': 0, 'w': math.cos(angle / 2)}}}]},
        {'op': 'entity.create', 'ref': 'wall', 'parent': {'ref': 'parent'}, 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vector(-1, -2, 0), 'LocalScale': vector(2, 2, .25)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.create', 'ref': 'light', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vector(2, 4, 5)}},
            {'type': 'Light', 'properties': {'Type': 'PointLight', 'Range': 30, 'Intensity': 30}}]},
    ])
    wall = created['refs']['wall']
    ids = components(created, 1)
    camera = request('/camera', {'position': vector(0, 0, 8), 'lookAt': vector(0, 0, 0), 'nearPlane': .1, 'farPlane': 100})
    baseline = {path: request(path) for path in ['/camera', '/history', '/level/status', '/entities']}
    first = observe('wall', after=camera['observationToken'])
    centre = pick(first)
    check_wall_hit(centre, wall, normal, .25)
    check_projection(first, centre)
    assert centre['frame'] == first['frame']
    assert centre['hit']['component'] == ids['ModelRenderer']
    for dx, dy in [(35, 0), (-35, 0), (0, -35), (0, 35)]:
        result = pick(first, first['image']['width'] // 2 + dx, first['image']['height'] // 2 + dy)
        check_wall_hit(result, wall, normal, .25)
        check_projection(first, result)
        if dx:
            assert (result['hit']['position']['x'] - centre['hit']['position']['x']) * dx > 0
        if dy:
            assert (result['hit']['position']['y'] - centre['hit']['position']['y']) * dy < 0
    assert pick(first, 0, 0)['hit'] is None
    for path, before in baseline.items():
        assert request(path) == before, 'Picking mutated ' + path

    # A separate pass must not disturb the color render or the camera/renderer state.
    plain = request('/observe', {'width': 501})
    (args.output / 'plain-after-picking.png').write_bytes(base64.b64decode(plain['image']['data']))
    (args.output / 'plain-after-picking.json').write_text(json.dumps({key: value for key, value in plain.items() if key != 'image'}, indent=2))
    assert plain['picking'] is None
    assert plain['image'] == first['image'], 'Picking changed the following viewport render'
    smaller = observe('small-wall', width=251)
    check_wall_hit(pick(smaller), wall, normal, .25)
    check_projection(smaller, pick(smaller))

    # Retained answers use the captured view, after both camera and scene changes.
    request('/camera', {'position': vector(0, 0, -8), 'lookAt': vector(0, 0, 0)})
    assert pick(first) == centre
    back = observe('back-wall')
    check_wall_hit(pick(back), wall, {axis: -normal[axis] for axis in 'xyz'}, .25)
    request('/camera', camera['state'])
    edit([{'op': 'component.update', 'target': {'id': ids['Transform']},
           'properties': {'LocalPosition': vector(-1, -2, -2)}}])
    assert pick(first) == centre
    moved = observe('moved-wall')
    check_wall_hit(pick(moved), wall, normal, .25 - 2 * normal['z'])

    # The nearest model surface wins; no Collider or live physics objects exist.
    occluder = edit([{'op': 'entity.create', 'ref': 'block', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(0, 0, 2), 'LocalScale': vector(.4, .4, .4)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]}])
    block = occluder['refs']['block']
    covered = observe('occluded-wall')
    hit = pick(covered)['hit']
    assert hit['entity'] == block and abs(hit['position']['z'] - 2.4) < .003, hit
    pick(first, expected=409)  # Fifth retained capture evicted the first.
    edit([{'op': 'entity.update', 'target': {'id': block}, 'properties': {'active': False}}])
    uncovered = observe('inactive-block')
    assert pick(uncovered)['hit']['entity'] == wall
    assert pick(covered)['hit'] == hit  # Historical state, including activation.

    # Captured identities stay historical after deletion, never alias another object.
    edit([{'op': 'entity.delete', 'target': {'id': block}}])
    assert pick(covered)['hit'] == hit
    valid = {'capture': uncovered['picking']['capture'], 'pixel': {'x': 1, 'y': 1}}
    bad_bodies = [None, [], {}, {'capture': valid['capture']}, {**valid, 'extra': 1},
                  {**valid, 'capture': 'bad'}, {**valid, 'pixel': {'x': 1}},
                  {**valid, 'pixel': {'x': 1, 'y': 1, 'extra': True}},
                  {**valid, 'pixel': {'x': -1, 'y': 1}}, {**valid, 'pixel': {'x': True, 'y': 1}},
                  {**valid, 'pixel': {'x': 1.5, 'y': 1}}, {**valid, 'pixel': {'x': 2**64, 'y': 1}},
                  {**valid, 'pixel': {'x': 4095, 'y': 1}}, {**valid, 'pixel': {'x': 1, 'y': 4095}},
                  {**valid, 'extra': 'x' * 4096}, {'capture': [[[[[[[]]]]]]], 'pixel': {}}]
    for body in bad_bodies:
        # JSON null still needs a POST, not the helper's GET default.
        if body is None:
            body = b'null'
        request('/pick', body, expected=400)
    request('/pick?extra=1', valid, expected=400)
    request('/pick', valid, expected=400,
            headers={'Idempotency-Key': 'read', 'X-Pine-Session': request('/requests')['session']})
    request('/observe', {'picking': 1}, expected=400)
    request('/pick', {**valid, 'capture': '123456789abc-123456789abcdef0'}, expected=409)

    # Reload invalidates captures even when it loads the scene that produced them.
    request('/level/save-as', {'path': 'levels/picking'})
    request('/level/load', {'path': 'levels/picking'})
    pick(uncovered, expected=409)
    fresh = observe('reloaded-wall')
    assert pick(fresh)['hit'] is not None

    # Game renders before Level, with a different camera and aspect ratio. Its
    # snapshot must not accidentally use the Level camera prepared later that frame.
    scene_camera = edit([{'op': 'entity.create', 'ref': 'camera', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(0, 0, -8),
         'LocalRotation': {'x': 0, 'y': 1, 'z': 0, 'w': 0}}},
        {'type': 'Camera', 'properties': {'FieldOfView': 55, 'NearPlane': .1, 'FarPlane': 100}}]}])
    selected = request('/level/camera', {'target': {'id': scene_camera['refs']['camera']}})
    game = observe('game-wall', view='game', after=selected['observationToken'])
    assert game['camera']['id'] == components(scene_camera)['Camera']
    assert game['viewport']['view'] == 'game'
    game_hit = pick(game)
    check_projection(game, game_hit)
    check_wall_hit(game_hit, pick(fresh)['hit']['entity'],
                   {axis: -normal[axis] for axis in 'xyz'}, .25 + 2 * normal['z'])
    request('/level/camera', {'target': None})
    request('/observe', {'view': 'game', 'picking': True}, expected=409)
    assert pick(game) == game_hit

    # Run the existing observation workflow against the changed capture path too.
    subprocess.run(['python3', str(Path(__file__).with_name('verify-observation.py')),
                    '--url', url, '--output', str(args.output / 'observation')], check=True)
    assert 'PASS(native)' in log_path.read_text()
    (args.output / 'hit.json').write_text(json.dumps(centre, indent=2))
    if args.check_expiry:
        expiring = observe('expiry')
        retention = expiring['picking']['retentionSeconds']
        start = time.monotonic()
        print('Checking capture expiry after %d seconds, including a read halfway through.' % retention, flush=True)
        while time.monotonic() < start + retention / 2:
            time.sleep(.25)
        pick(expiring)
        while time.monotonic() < start + retention + 1:
            time.sleep(.25)
        pick(expiring, expected=409)
        print('PASS: capture expires without refresh on reads.', flush=True)
    print('PASS: capture picking, normals, pixel axes/resizing, parent transforms, historical state, occlusion, eviction, validation, scene replacement and observation regression.')
finally:
    shutdown()
    log.close()
    print('Editor log:', log_path)

"""Check filtered scene inspection in a disposable project; requires a Ninja Editor build and Xvfb."""

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
parser.add_argument('--port', type=int, default=19047)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-inspection-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-inspection.'))
url = 'http://127.0.0.1:%d' % args.port
print('Inspection verification:', root, flush=True)

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
#include "DebugServer/Inspection/Inspection.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/NativeScript/NativeScript.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
'''
body = Path(__file__).with_name('inspection-native.inc').read_text()
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
# layout for the existing observation readback checks, leaving the user's settings alone.
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


def request(path, body=None, expected=200, headers=None, raw=None):
    data = raw if raw is not None else None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url + path, data=data,
                                 headers={'Content-Type': 'application/json', **(headers or {})})
    try:
        response = urllib.request.urlopen(req, timeout=30)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        if path == '/entities/query':
            assert len(payload) <= 1024 * 1024
            assert response.headers.get_content_type() == 'application/json'
        return json.loads(payload)


def edit(operations):
    return request('/edit', {'version': 1, 'operations': operations})


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def query(**fields):
    return request('/entities/query', fields)


def ids(result):
    return [entity['id'] for entity in result['entities']]


def batch(references, **fields):
    return query(entities=[{'id': item} for item in references], **fields)


try:
    deadline = time.monotonic() + 60
    while True:
        if process.poll() is not None:
            raise RuntimeError('Probe exited before HTTP checks; see ' + str(log_path))
        try:
            request('/status')
            break
        except (OSError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(.1)

    created = edit([
        {'op': 'entity.create', 'name': 'Inspection group', 'ref': 'group', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(10, 3, 20),
             'LocalRotation': {'x': 0, 'y': 0, 'z': 1, 'w': 1}, 'LocalScale': vec(2, 3, 4)}}]},
        {'op': 'entity.create', 'name': 'Inspection Lamp', 'ref': 'lamp', 'parent': {'ref': 'group'}, 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(2, 0, 0)}},
            {'type': 'Light', 'properties': {'Type': 'SpotLight', 'Intensity': 42, 'Range': 8}}]},
        {'op': 'entity.create', 'name': 'Inspection nested lamp', 'ref': 'nested', 'parent': {'ref': 'lamp'},
         'components': [{'type': 'Light'}]},
        {'op': 'entity.create', 'name': 'Inspection crate', 'ref': 'crate', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(16, 3, 20), 'LocalScale': vec(-2, 0, 3)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}, 'MeshIndex': 0}}]},
        {'op': 'entity.create', 'name': 'Inspection lamp', 'ref': 'other', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(100, 0, 0)}}, {'type': 'Light'}]}
    ])
    group, lamp, nested, crate, other = [created['refs'][key] for key in ['group', 'lamp', 'nested', 'crate', 'other']]
    before = {path: request(path) for path in ['/history', '/level/status', '/camera', '/entities']}
    generation = query()['sceneGeneration']

    lights = query(filter={'component': 'Light'}, include={'properties': True, 'worldTransform': True,
                                                          'localTransform': True, 'components': ['Light']})
    assert set(ids(lights)) == {lamp, nested, other} and not lights['truncated']
    assert lights['sceneGeneration'] == generation
    lamp_state = next(item for item in lights['entities'] if item['id'] == lamp)
    assert lamp_state['parent'] == group
    assert lamp_state['worldTransform']['position'] == vec(12, 3, 20)
    assert lamp_state['localTransform']['position'] == vec(2, 0, 0)
    for item in lights['entities']:
        individual = request('/entity?id=' + item['id'])
        assert item['properties'] == individual['properties']
        assert len(item['components']) == 1 and item['components'][0]['type'] == 'Light'
        light = next(c for c in individual['components'] if c['type'] == 'Light')
        assert item['components'][0]['properties'] == light['properties']
        measured = request('/spatial/query', {'entities': [{'id': item['id']}], 'includeChildren': False})
        assert item['worldTransform'] == measured['entities'][0]['worldTransform']
        assert item['localTransform'] == measured['entities'][0]['localTransform']
        assert 'data' not in item['components'][0]
    (args.output / 'lights.json').write_text(json.dumps(lights, indent=2) + '\n')

    repeated = batch([other, lamp, other], sceneGeneration=generation)
    assert ids(repeated) == [other, lamp, other] and repeated['total'] == 3 and not repeated['truncated']
    assert 'properties' not in repeated['entities'][0] and 'worldTransform' not in repeated['entities'][0]
    assert batch([lamp], include={'components': []})['entities'][0]['components'] == []
    assert ids(query(filter={'name': {'value': 'Inspection Lamp'}})) == [lamp]
    assert ids(query(filter={'name': {'value': 'inspection lamp'}})) == []
    assert set(ids(query(filter={'name': {'value': 'lamp', 'match': 'contains'}}))) == {nested, other}
    hierarchy = {'root': {'id': group}}
    assert set(ids(query(filter={'hierarchy': hierarchy}))) == {lamp, nested}
    assert ids(query(filter={'hierarchy': {**hierarchy, 'mode': 'children'}})) == [lamp]
    assert set(ids(query(filter={'hierarchy': {**hierarchy, 'includeRoot': True}}))) == {group, lamp, nested}
    assert set(ids(query(filter={'hierarchy': hierarchy, 'component': 'Light',
                                 'name': {'value': 'nested', 'match': 'contains'}}))) == {nested}

    center = lamp_state['worldTransform']['position']
    spatial = {'test': 'pivot', 'radius': {'center': center, 'distance': 2}}
    nearby = query(filter={'spatial': spatial}, include={'worldTransform': True})
    assert set(ids(nearby)) == {group, lamp, nested}
    assert ids(query(filter={'spatial': {**spatial, 'test': 'bounds'}})) == [crate]
    assert ids(query(filter={'spatial': {**spatial, 'test': 'bounds'}, 'component': 'Light'})) == []
    # The flattened, mirrored cube reaches x=14; its pivot is x=16. Touching counts.
    box = {'min': vec(14, 3, 20), 'max': vec(14, 3, 20)}
    assert ids(query(filter={'spatial': {'test': 'pivot', 'bounds': box}})) == []
    assert ids(query(filter={'spatial': {'test': 'bounds', 'bounds': box}})) == [crate]
    zero = {'test': 'pivot', 'radius': {'center': center, 'distance': 0}}
    assert set(ids(query(filter={'spatial': zero}))) == {lamp, nested}
    truncated = query(filter={'component': 'Light'}, limit=1)
    assert truncated['total'] == 3 and len(truncated['entities']) == 1 and truncated['truncated']
    assert ids(truncated) == ids(query(filter={'component': 'Light'}, limit=1))
    empty = query(filter={'name': {'value': 'no such inspection entity'}})
    assert empty['total'] == 0 and not empty['truncated'] and empty['entities'] == []
    (args.output / 'nearby.json').write_text(json.dumps(nearby, indent=2) + '\n')

    temporary = next(item['id'] for item in before['/entities']['entities'] if item['temporary'])
    assert temporary not in ids(query())
    missing = '0000000000000001-0000000000000001'
    invalid = [
        {'entities': []}, {'entities': [{'id': lamp}] * 129}, {'entities': [{'ref': 'lamp'}]},
        {'entities': [{'id': lamp}, {'id': missing}]}, {'entities': [{'id': temporary}]},
        {'entities': [{'id': lamp}], 'filter': {}}, {'entities': [{'id': lamp}], 'limit': 1},
        {'filter': {'hierarchy': {'root': {'id': temporary}}}}, {'filter': {'component': 'Typo'}},
        {'filter': {'component': 'Collider2D'}}, {'filter': {'name': {'value': 'x', 'match': 'regex'}}},
        {'filter': {'hierarchy': {'root': {'id': group}, 'mode': 'ancestors'}}},
        {'filter': {'includeInactive': 1}}, {'filter': {'spatial': {'radius': {'center': center, 'distance': 1}}}},
        {'filter': {'spatial': {'test': 'pivot', 'bounds': box, 'radius': {'center': center, 'distance': 1}}}},
        {'filter': {'spatial': {'test': 'pivot', 'radius': {'center': center, 'distance': -1}}}},
        {'filter': {'spatial': {'test': 'bounds', 'bounds': {'min': vec(1, 0, 0), 'max': vec(0, 0, 0)}}}},
        {'filter': {'spatial': {'test': 'pivot', 'radius': {'center': vec(1e13, 0, 0), 'distance': 1}}}},
        {'include': {'worldTransform': 'yes'}}, {'include': {'components': ['Unknown']}},
        {'include': {'components': 'Light'}}, {'include': {'data': True}}, {'unknown': True},
        {'limit': 0}, {'limit': 129}, {'limit': True}, {'limit': 1.5}, {'sceneGeneration': -1},
        {'sceneGeneration': 1.2}
    ]
    for body in invalid:
        result = request('/entities/query', body, expected=400)
        assert 'path' in result and 'error' in result, (body, result)
    for raw in [b'{', b'{}' + b' ' * 16384, b'[' * 10 + b']' * 10]:
        request('/entities/query', raw=raw, expected=400)
    request('/entities/query?limit=1', {}, expected=400)
    request('/entities/query', {}, expected=400, headers={'Idempotency-Key': 'inspection-is-read-only'})
    request('/entities/query', {'sceneGeneration': generation + 1}, expected=409)
    for path, state in before.items():
        assert request(path) == state, ('Inspection changed state', path)

    edit([{'op': 'entity.update', 'target': {'id': group}, 'properties': {'active': False}},
          {'op': 'entity.update', 'target': {'id': other}, 'properties': {'active': False}}])
    assert set(ids(query(filter={'component': 'Light'}))) == {lamp, nested, other}
    assert set(ids(query(filter={'component': 'Light', 'includeInactive': False}))) == {lamp, nested}
    lamp_transform = next(c['id'] for c in request('/entity?id=' + lamp)['components'] if c['type'] == 'Transform')
    edit([{'op': 'component.update', 'target': {'id': lamp_transform}, 'properties': {'LocalPosition': vec(7, 0, 0)}}])
    assert batch([lamp], include={'worldTransform': True})['entities'][0]['worldTransform']['position'] == vec(17, 3, 20)
    assert query()['sceneGeneration'] == generation, 'Ordinary edits should not change generation'
    edit([{'op': 'entity.delete', 'target': {'id': other}}])
    request('/entities/query', {'entities': [{'id': lamp}, {'id': other}]}, expected=400)

    # More matches than the count cap, without downloading the complete hierarchy.
    for start in [0, 64, 128]:
        edit([{'op': 'entity.create', 'name': 'Inspection bulk ' + str(i)} for i in range(start, min(start + 64, 130))])
    bulk = query(filter={'name': {'value': 'Inspection bulk ', 'match': 'contains'}})
    assert bulk['total'] == 130 and len(bulk['entities']) == 128 and bulk['truncated']

    saved = request('/level/save-as', {'path': 'levels/inspection', 'overwrite': False})
    assert saved['fileWritten']
    request('/level/load', {'path': 'levels/inspection'})
    request('/entities/query', {'sceneGeneration': generation}, expected=409)
    assert query(filter={'component': 'Light'})['total'] == 2
    assert query()['sceneGeneration'] > generation

    # The shared readers must preserve individual and observation readback contracts.
    subprocess.run(['python3', str(Path(__file__).with_name('verify-readback.py')),
                    '--url', url, '--output', str(args.output / 'readback')], check=True)
    assert 'PASS(native)' in log_path.read_text()
    print('PASS: filtered inspection, batches, transforms, spatial semantics, limits, validation, generation and readback compatibility.')
finally:
    shutdown()
    log.close()
    print('Editor log:', log_path)

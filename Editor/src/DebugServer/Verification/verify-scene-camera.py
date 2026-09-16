"""Scene Camera HTTP recipe; run against a disposable project with the Game tab open."""

import argparse
import base64
import json
import math
from pathlib import Path
import urllib.error
import urllib.request
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19033')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-scene-camera-results'))
parser.add_argument('--reload-only', action='store_true', help='Verify saved files after restarting the Editor.')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def request(path, body=None, expected=200, key=None):
    headers = {'Content-Type': 'application/json'}
    if key:
        headers['Idempotency-Key'] = key
        headers['X-Pine-Session'] = request('/requests')['session']
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(args.url + path, data=data, headers=headers)
    try:
        response = urllib.request.urlopen(req, timeout=30)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:2000])
        return json.loads(payload)


def edit(operations, expected=200):
    return request('/edit', {'version': 1, 'operations': operations}, expected)


def component(entity_id, name='Camera'):
    entity = request('/entity?id=' + entity_id)
    return next(c for c in entity['components'] if c['type'] == name)


def select(entity_id, **kwargs):
    target = None if entity_id is None else {'id': entity_id}
    return request('/level/camera', {'target': target}, **kwargs)


def patch(camera_id, properties):
    return {'op': 'component.update', 'target': {'id': camera_id}, 'properties': properties}


def capture(result, camera_id, name):
    observation = request('/observe', {'view': 'game', 'after': result['observationToken'], 'width': 640})
    assert observation['camera']['id'] == camera_id, observation
    assert observation['viewport']['view'] == 'game'
    camera = observation['camera']
    projection = camera['projectionMatrix']
    expected_scale = 1 / math.tan(math.radians(camera['fieldOfView']) / 2)
    assert math.isclose(projection[1][1], expected_scale, rel_tol=1e-5), projection
    assert math.isclose(projection[1][1] / projection[0][0], 1920 / 1080, rel_tol=1e-5)
    pixels = base64.b64decode(observation['image']['data'])
    (args.output / (name + '.png')).write_bytes(pixels)
    observation['image'].pop('data')
    (args.output / (name + '.json')).write_text(json.dumps(observation, indent=2) + '\n')
    return pixels


def verify_reload(path, name):
    loaded = request('/level/load', {'path': path})
    selected = request('/level/camera')
    assert selected['target'] is not None, selected
    entity = request('/entity?id=' + selected['target']['id'])
    assert entity['name'] == 'Camera B', entity
    assert entity['parent']['name'] == 'Camera parent', entity
    camera = component(entity['id'])
    assert camera['properties']['FieldOfView'] == 55
    assert camera['properties']['NearPlane'] == 0.25
    assert camera['properties']['FarPlane'] == 80
    capture(loaded, camera['id'], name)
    assert request('/level/status')['unsavedChanges'] is False


manifest_path = args.output / 'saved-levels.json'
if args.reload_only:
    manifest = json.loads(manifest_path.read_text())
    verify_reload(manifest['selected'], 'restarted')
    request('/level/load', {'path': manifest['cleared']})
    assert request('/level/camera')['target'] is None
    request('/observe', {'view': 'game'}, expected=409)
    print('PASS: selected and cleared game cameras persist across Editor restart.')
    raise SystemExit

schema = request('/edit/schema')
assert schema['levelCamera']['set'] == '/level/camera'
assert schema['components']['Camera']['addable'] and schema['components']['Camera']['removable']
assert schema['components']['Camera']['properties']['Type']['values'] == ['Perspective']
assert 'Camera' in schema['entity']['duplicate']['supportedComponents']
assert request('/level/camera')['target'] is None
created = edit([
    {'op': 'entity.create', 'ref': 'a', 'name': 'Camera A', 'components': [
        {'type': 'Camera'}, {'type': 'Transform', 'properties': {'LocalPosition': {'x': 0, 'y': 0, 'z': 6}}}]},
    {'op': 'entity.create', 'ref': 'parent', 'name': 'Camera parent'},
    {'op': 'entity.create', 'ref': 'cube', 'name': 'Visible cube', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'ref': 'b', 'name': 'Camera B', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 1, 'y': 0, 'z': 6}}}]},
    {'op': 'entity.create', 'name': 'Camera light', 'components': [{'type': 'Light'}]}
])
a, b, parent, cube = (created['refs'][key] for key in ['a', 'b', 'parent', 'cube'])
ca = component(a)
assert ca['properties'] == schema['components']['Camera']['defaults']
cb = edit([{'op': 'component.add', 'target': {'id': b}, 'type': 'Camera',
            'properties': {'FieldOfView': 55, 'NearPlane': 0.25, 'FarPlane': 80}}])['results'][0]['component']
# Live scene order now differs from the depth-first order serialized by Blueprint.
edit([{'op': 'entity.reparent', 'target': {'id': b}, 'parent': {'id': parent}}])
key = str(uuid.uuid4())
selected_a = select(a, key=key)
image_a = capture(selected_a, ca['id'], 'camera-a')
selected_b = select(b)
image_b = capture(selected_b, cb['id'], 'camera-b')
assert image_a != image_b
assert select(a, key=key) == selected_a
assert request('/level/camera')['target'] == {'id': b}
select(b, key=key, expected=409)
request('/history/undo', {})
assert request('/level/camera')['target'] == {'id': a}
request('/history/redo', {})
assert request('/level/camera')['target'] == {'id': b}

# Rejections preserve both the scene and the history stack.
for properties in [
    {'Type': 'Orthographic'}, {'Type': 0}, {'FieldOfView': 0}, {'FieldOfView': 180},
    {'FieldOfView': True}, {'NearPlane': 0}, {'NearPlane': 1e-50}, {'NearPlane': 1e-45},
    {'NearPlane': 80}, {'FarPlane': 0.1}, {'FarPlane': 1e100},
    {'NearPlane': 1, 'FarPlane': 3e38},
    {'NearPlane': 1, 'FarPlane': 1.000000001}, {'OrthographicSize': 2}, {'unknown': 1}
]:
    before = component(b)
    history = request('/history')
    result = edit([{'op': 'entity.create', 'name': 'Must not exist'}, patch(cb['id'], properties)], expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0
    assert component(b) == before and request('/history') == history

for body in [{}, {'target': {'id': cube}}, {'target': {'id': cb['id']}},
             {'target': {'ref': 'b'}}, {'target': {'id': 'bad'}}, {'target': None, 'extra': True}]:
    history = request('/history')
    request('/level/camera', body, expected=400)
    assert request('/level/camera')['target'] == {'id': b}
    assert request('/history') == history

# Patches compose against preceding values, then undo restores the entire batch.
changed = edit([patch(cb['id'], {'FarPlane': 200}), patch(cb['id'], {'NearPlane': 100})])
assert component(b)['properties']['NearPlane'] == 100
capture(changed, cb['id'], 'clipped')
request('/history/undo', {})
assert component(b)['properties'] == cb['properties']
zoomed = edit([patch(cb['id'], {'FieldOfView': 30})])
assert capture(zoomed, cb['id'], 'zoomed') != image_b
request('/history/undo', {})

# Camera duplication copies properties without choosing the copy.
duplicated = edit([{'op': 'entity.duplicate', 'target': {'id': parent}, 'ref': 'copy'}])
assert request('/level/camera')['target'] == {'id': b}
copy_camera = next(c for node in duplicated['results'][0]['duplicatedEntities']
                   for c in node['entity']['components'] if c['type'] == 'Camera')
assert copy_camera['properties'] == cb['properties'] and copy_camera['id'] != cb['id']
request('/history/undo', {})
request('/history/redo', {})
assert request('/level/camera')['target'] == {'id': b}
request('/history/undo', {})

for operation in [
    {'op': 'component.remove', 'target': {'id': cb['id']}},
    {'op': 'entity.delete', 'target': {'id': parent}}
]:
    edit([operation])
    assert request('/level/camera')['target'] is None
    request('/observe', {'view': 'game'}, expected=409)
    restored = request('/history/undo', {})
    assert request('/level/camera')['component'] == {'id': cb['id']}
    capture(restored, cb['id'], 'restored-' + operation['op'])
    request('/history/redo', {})
    assert request('/level/camera')['target'] is None
    request('/history/undo', {})

path = 'verification/cameras-' + uuid.uuid4().hex
request('/level/save-as', {'path': path})
assert request('/level/status')['unsavedChanges'] is False
select(None)
assert request('/level/status')['unsavedChanges'] is True
request('/history/undo', {})
assert request('/level/status')['unsavedChanges'] is False
request('/history/redo', {})
request('/level/save-as', {'path': path + '-cleared'})
manifest_path.write_text(json.dumps({'selected': path, 'cleared': path + '-cleared'}))
verify_reload(path, 'reloaded')
request('/level/load', {'path': path + '-cleared'})
assert request('/level/camera')['target'] is None
request('/observe', {'view': 'game'}, expected=409)
print('PASS: Camera schema, creation/addition, validation, patches, selection, retry, history,')
print('      duplication/removal/deletion, Game captures, save/reload and cleared-camera persistence.')
print('Captures and restart manifest:', args.output)

"""Entity-aim HTTP recipe; run against a disposable project with an active Level viewport."""

import argparse
import base64
import itertools
import json
import math
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19041')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-aim-results'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def request(path, body=None, expected=200, headers=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(args.url + path, data=data,
                                 headers={'Content-Type': 'application/json', **(headers or {})})
    try:
        response = urllib.request.urlopen(req, timeout=30)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:2000])
        return json.loads(payload)


def edit(operations, **kwargs):
    return request('/edit', {'version': 1, 'operations': operations}, **kwargs)


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def xyz(value):
    return [value[axis] for axis in 'xyz']


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return [a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]]


def rotate(q, point):
    # Independent quaternion-vector formula, matching the engine's rotation convention.
    v = xyz(q)
    t = [2 * item for item in cross(v, point)]
    return add(point, add([q['w'] * item for item in t], cross(v, t)))


def close(actual, expected, tolerance=2e-4):
    assert all(abs(a-b) < tolerance for a, b in zip(actual, expected)), (actual, expected)


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def spatial(entity_id):
    return request('/spatial/query', {'entities': [{'id': entity_id}], 'includeChildren': False})['entities'][0]


def component(entity_id, name):
    return next(c for c in entity(entity_id)['components'] if c['type'] == name)


def update(entity_id, properties, name='Transform'):
    return {'op': 'component.update', 'target': {'id': component(entity_id, name)['id']}, 'properties': properties}


def capture(token, ids, name):
    observed = request('/observe', {'after': token, 'entities': ids, 'width': 640})
    assert observed['frame']['id'] > token['frame']
    assert observed['frame']['revision'] >= token['revision']
    assert observed['frame']['sceneGeneration'] == token['sceneGeneration']
    for state in observed['entities']:
        assert state['components'] == entity(state['id'])['components']
    image = base64.b64decode(observed['image'].pop('data'), validate=True)
    (args.output / (name + '.png')).write_bytes(image)
    (args.output / (name + '.json')).write_text(json.dumps(observed, indent=2) + '\n')
    return image


def aim(entity_id, point, **fields):
    return {'op': 'entity.aim', 'target': {'id': entity_id}, 'point': point,
            'forwardAxis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0), **fields}


def axis_vector(axis):
    result = [0, 0, 0]
    result['XYZ'.index(axis[1])] = 1 if axis[0] == '+' else -1
    return result


def unit(vector):
    length = math.sqrt(dot(vector, vector))
    return [value / length for value in vector]


def check_aim(operation):
    world = spatial(operation['target']['id'])['worldTransform']
    direction = unit([a-b for a, b in zip(xyz(operation['point']), xyz(world['position']))])
    close(rotate(world['rotation'], axis_vector(operation['forwardAxis'])), direction)
    up = unit(xyz(operation['up']))
    projected_up = unit([a-dot(up, direction)*b for a, b in zip(up, direction)])
    close(rotate(world['rotation'], axis_vector(operation['upAxis'])), projected_up)


schema = request('/edit/schema')
fields = schema['operationSchemas']['entity.aim']['fields']
assert fields['target']['forms'] == ['id']
assert fields['point']['space'] == fields['up']['space'] == 'world'
axes = fields['forwardAxis']['values']
assert set(axes) == {'+X', '-X', '+Y', '-Y', '+Z', '-Z'}
assert fields['upAxis']['values'] == axes

angle = math.radians(35)
parent_rotation = {'x': 0, 'y': math.sin(angle), 'z': 0, 'w': math.cos(angle)}
created = edit([
    {'op': 'entity.create', 'ref': 'parent', 'name': 'Aim parent', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(0, 80, 0),
         'LocalRotation': parent_rotation, 'LocalScale': vec(-2, 3, .5)}}]},
    {'op': 'entity.create', 'ref': 'lamp', 'name': 'Aim spotlight', 'parent': {'ref': 'parent'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(-2, 0, 6), 'LocalScale': vec(.2, 0, -.4)}},
        {'type': 'Light', 'properties': {'Type': 'SpotLight', 'Intensity': 8, 'Range': 20,
         'SpotlightInnerAngle': 12, 'SpotlightOuterAngle': 22, 'CastShadows': False}}]},
    {'op': 'entity.create', 'ref': 'wall', 'name': 'Aim wall', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(0, 80, 0), 'LocalScale': vec(4, 3, .2),
         'LocalRotation': {'x': 0, 'y': math.sin(.15), 'z': 0, 'w': math.cos(.15)}}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
])
parent, lamp, wall = [created['refs'][name] for name in ['parent', 'lamp', 'wall']]
watched = [parent, lamp, wall]
framed = request('/camera', {'position': vec(0, 80, 14), 'lookAt': vec(0, 80, 0), 'fieldOfView': 45})
observed = request('/observe', {'after': framed['observationToken'], 'width': 640, 'picking': True})
picked = request('/pick', {'capture': observed['picking']['capture'],
                          'pixel': {'x': observed['image']['width'] // 2, 'y': observed['image']['height'] // 2}})
assert picked['hit']['entity'] == wall
(args.output / 'picked-point.json').write_text(json.dumps(picked, indent=2) + '\n')
point = picked['hit']['position']

# Place an explicit pivot relative to the picked plane, then aim without changing that position.
placed = edit([{'op': 'entity.place', 'target': {'id': lamp},
               'surface': {'point': point, 'normal': picked['hit']['normal']},
               'anchor': {'type': 'localPoint', 'point': vec(0, 0, 0)}, 'clearance': 6}])
before = component(lamp, 'Transform')['properties']
capture(placed['observationToken'], [lamp, wall], 'before-aim')
operation = aim(lamp, point)
aimed = edit([operation])
check_aim(operation)
after = component(lamp, 'Transform')['properties']
assert after['LocalPosition'] == before['LocalPosition'] and after['LocalScale'] == before['LocalScale']
assert aimed['results'][0]['entity']['components'][0]['properties'] == after
aimed_image = capture(aimed['observationToken'], [lamp, wall], 'aimed')
undone = request('/history/undo', {})
assert component(lamp, 'Transform')['properties'] == before
assert capture(undone['observationToken'], [lamp, wall], 'undone') != aimed_image
redone = request('/history/redo', {})
assert component(lamp, 'Transform')['properties'] == after
check_aim(operation)
capture(redone['observationToken'], [lamp, wall], 'redone')

# Every perpendicular signed local axis pair fixes both direction and roll, ignoring scale.
for forward, up_axis in itertools.product(axes, axes):
    if forward[1] == up_axis[1]:
        continue
    arbitrary = aim(lamp, vec(3, 81, -4), forwardAxis=forward, upAxis=up_axis, up=vec(1, 3, 1))
    edit([arbitrary])
    check_aim(arbitrary)
    current = component(lamp, 'Transform')['properties']
    assert current['LocalPosition'] == before['LocalPosition'] and current['LocalScale'] == before['LocalScale']

# Proposed state includes new parents, reparenting, earlier position/rotation edits and repeated aims.
composed = edit([
    {'op': 'entity.create', 'ref': 'new-parent', 'components': [{'type': 'Transform', 'properties': {
        'LocalPosition': vec(4, 70, 2), 'LocalRotation': parent_rotation}}]},
    {'op': 'entity.reparent', 'target': {'id': lamp}, 'parent': {'ref': 'new-parent'}},
    update(lamp, {'LocalPosition': vec(-1, 4, 6)}),
    operation,
    update(lamp, {'LocalScale': vec(0, -2, 3)}),
    aim(lamp, vec(-3, 75, -8)),
    {'op': 'entity.duplicate', 'target': {'id': lamp}, 'ref': 'aimed-copy'},
])
check_aim(aim(lamp, vec(-3, 75, -8)))
assert composed['results'][3]['entity']['components'][0]['properties']['LocalRotation'] == composed['results'][4]['component']['properties']['LocalRotation']
assert component(lamp, 'Transform')['properties'] == component(composed['refs']['aimed-copy'], 'Transform')['properties']
request('/history/undo', {})
request('/history/redo', {})
check_aim(aim(lamp, vec(-3, 75, -8)))
request('/history/undo', {})

# Aim an ancestor before aiming its child, and place after aiming while retaining the rotation.
edit([aim(parent, vec(10, 82, -10)), operation,
      {'op': 'entity.place', 'target': {'id': lamp},
       'surface': {'point': vec(0, 80, 6), 'normal': vec(0, 0, 1)},
       'anchor': {'type': 'localPoint', 'point': vec(0, 0, 0)}}, operation])
check_aim(operation)
request('/history/undo', {})

# Whole-batch rejection preserves entities, history and persistence status.
temporary = next(item['id'] for item in request('/entities')['entities'] if item['temporary'])
position = spatial(lamp)['worldTransform']['position']
invalid = [
    {**operation, 'unknown': True}, {**operation, 'target': {'ref': 'lamp'}},
    {**operation, 'target': {'id': temporary}},
    {**operation, 'target': {'id': component(lamp, 'Transform')['id']}},
    {**operation, 'point': position}, {**operation, 'point': vec(1e13, 0, 0)},
    {**operation, 'point': vec(True, 0, 0)}, {**operation, 'point': None},
    {**operation, 'point': {'x': 1, 'y': 2}},
    {**operation, 'forwardAxis': 'Z'}, {**operation, 'upAxis': '+Z'},
    {**operation, 'forwardAxis': '+Y'}, {**operation, 'up': vec(0, 0, 0)},
    aim(lamp, vec(position['x'], position['y'] + 1, position['z'])),
    aim(lamp, vec(position['x'] + 1e-8, position['y'] + 1, position['z'])),
]
invalid += [{key: value for key, value in operation.items() if key != field}
            for field in ['point', 'forwardAxis', 'upAxis', 'up']]
for bad in invalid:
    states = [entity(item) for item in watched]
    history, level, tree = request('/history'), request('/level/status'), request('/entities')
    rejected = edit([update(parent, {'LocalScale': vec(1, 2, 3)}), bad], expected=400)
    assert rejected['phase'] == 'validation' and rejected['completed'] == 0 and rejected['operation'] == 1
    assert [entity(item) for item in watched] == states
    assert request('/history') == history and request('/level/status') == level and request('/entities') == tree

for prefix in [
    [{'op': 'entity.delete', 'target': {'id': parent}}],
    [update(lamp, {'LocalPosition': vec(0, 0, 0)})],
]:
    bad = operation if prefix[0]['op'] == 'entity.delete' else aim(lamp, spatial(parent)['worldTransform']['position'])
    states, history = [entity(item) for item in watched], request('/history')
    assert edit(prefix + [bad], expected=400)['completed'] == 0
    assert [entity(item) for item in watched] == states and request('/history') == history

headers = {'X-Pine-Session': request('/requests')['session'], 'Idempotency-Key': uuid.uuid4().hex}
first = edit([operation], headers=headers)
edit([aim(lamp, vec(10, 85, -10))])
changed = component(lamp, 'Transform')['properties']
assert edit([operation], headers=headers) == first
assert component(lamp, 'Transform')['properties'] == changed
edit([aim(lamp, vec(10, 85, -10))], headers=headers, expected=409)
edit([operation])

# Save/reload and rendered feedback retain the calculated rotation and spotlight settings.
saved_transform = component(lamp, 'Transform')['properties']
saved_light = component(lamp, 'Light')['properties']
saved = request('/level/save-as', {'path': 'verification/aim-' + uuid.uuid4().hex})
assert saved['fileWritten'] and not saved['unsavedChanges']
loaded = request('/level/load', {'path': saved['path']})


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node['children'])


by_name = {node['name']: node['id'] for node in walk(request('/entities')['entities'])}
lamp, wall = by_name['Aim spotlight'], by_name['Aim wall']
assert component(lamp, 'Transform')['properties'] == saved_transform
assert component(lamp, 'Light')['properties'] == saved_light
check_aim(aim(lamp, point))
capture(loaded['observationToken'], [lamp, wall], 'reloaded')
edit([operation], expected=400)
print('PASS: entity aiming, picked spotlight target, all axis pairs, parent transforms, ordered batches, validation, history, retries and save/reload.')
print('Inspect captures in', args.output)

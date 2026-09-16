"""3D physics authoring HTTP recipe; run only against a disposable Pine project."""

import argparse
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19034')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-physics-results'))
parser.add_argument('--reload-only', action='store_true')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def request(path, body=None, expected=200, key=None):
    headers = {'Content-Type': 'application/json'}
    if key:
        headers.update({'Idempotency-Key': key, 'X-Pine-Session': request('/requests')['session']})
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


def edit(operations, **kwargs):
    return request('/edit', {'version': 1, 'operations': operations}, **kwargs)


def component(entity_id, name):
    return next(c for c in request('/entity?id=' + entity_id)['components'] if c['type'] == name)


def patch(component_id, properties):
    return {'op': 'component.update', 'target': {'id': component_id}, 'properties': properties}


def vector(x, y, z):
    return {'x': x, 'y': y, 'z': z}


def authored():
    def walk(entities):
        for entity in entities:
            if not entity['temporary']:
                yield entity
                yield from walk(entity['children'])

    result = {}
    for entry in walk(request('/entities')['entities']):
        entity = request('/entity?id=' + entry['id'])
        result[entity['name']] = {
            'properties': entity['properties'],
            'parent': entity['parent']['name'] if entity['parent'] else None,
            'components': [{'type': c['type'], 'active': c['active'], 'properties': c['properties']}
                           for c in entity['components']]
        }
    return result


manifest_path = args.output / 'saved.json'
if args.reload_only:
    manifest = json.loads(manifest_path.read_text())
    request('/level/load', {'path': manifest['path']})
    assert authored() == manifest['scene']
    print('PASS: physics authoring persisted across Editor restart.')
    raise SystemExit(0)

schema = request('/edit/schema')
for name in ['Collider', 'RigidBody']:
    assert schema['components'][name]['addable'] and schema['components'][name]['removable']
    assert name in schema['entity']['duplicate']['supportedComponents']
    assert name in schema['entity']['delete']['supportedComponents']
assert schema['components']['Collider']['properties']['Type']['values'] == ['Box', 'Sphere', 'Capsule']
assert schema['components']['RigidBody']['properties']['PositionLock']['type'] == 'boolean3'

created = edit([
    {'op': 'entity.create', 'name': 'Physics floor', 'ref': 'floor', 'components': [
        {'type': 'Collider', 'properties': {'Size': vector(10, 0.5, 10)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'name': 'Physics body', 'ref': 'body', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(0, 4, 0)}},
        {'type': 'RigidBody'}, {'type': 'Collider'}]},
    {'op': 'entity.create', 'name': 'Physics trigger', 'ref': 'trigger', 'parent': {'ref': 'floor'},
     'components': [{'type': 'Collider', 'properties': {'Type': 'Sphere', 'IsTrigger': True}}]}
])
floor, body, trigger = (created['refs'][name] for name in ['floor', 'body', 'trigger'])
collider, rigid = component(body, 'Collider'), component(body, 'RigidBody')
assert collider['properties'] == schema['components']['Collider']['defaults']
assert rigid['properties'] == schema['components']['RigidBody']['defaults']
initial = authored()
request('/history/undo', {})
request('/history/redo', {})
assert authored() == initial
assert component(body, 'Collider')['id'] == collider['id']
assert component(body, 'RigidBody')['id'] == rigid['id']

# Invalid component values reject the entire batch without changing history.
invalid_colliders = [
    {'Type': 'ConvexMesh'}, {'Type': 'ConcaveMesh'}, {'Type': 'HeightField'}, {'Type': 'box'},
    {'Size': vector(0, 1, 1)}, {'Size': vector(1, -1, 1)}, {'Size': vector(1e-50, 1, 1)},
    {'Size': vector(1e100, 1, 1)}, {'Size': {'x': 1}}, {'Position': vector(True, 0, 0)},
    {'Layer': -1}, {'Layer': 4294967296}, {'LayerMask': 1.5}, {'LayerMask': True},
    {'TriggerMask': -1}, {'TriggerMask': 4294967296}, {'IsTrigger': 1}, {'Radius': 1}
]
invalid_bodies = [
    {'Type': 'dynamic'}, {'Type': 0}, {'Mass': 0}, {'Mass': -1}, {'Mass': True},
    {'Mass': 1e-50}, {'Mass': 1e-45}, {'Mass': 1e100}, {'GravityEnabled': 1},
    {'PositionLock': [True, False, False]}, {'PositionLock': {'x': True}},
    {'PositionLock': vector(1, False, False)}, {'RotationLock': {**vector(False, False, False), 'w': False}},
    {'MaxLinearVelocity': -1}, {'MaxAngularVelocity': 1e100}, {'Unknown': 1}
]
for target, invalid in [(collider, invalid_colliders), (rigid, invalid_bodies)]:
    for properties in invalid:
        before = authored()
        history = request('/history')
        rejected = edit([{'op': 'entity.create', 'name': 'Must not exist'}, patch(target['id'], properties)], expected=400)
        assert rejected['completed'] == 0 and rejected['phase'] == 'validation'
        assert authored() == before and request('/history') == history

# Ordered patches, full axis replacement, uint32 boundaries, all body types and primitive shapes.
for name in ['Static', 'Kinematic', 'Dynamic']:
    edit([patch(rigid['id'], {'Type': name})])
    assert component(body, 'RigidBody')['properties']['Type'] == name
changed = edit([
    patch(collider['id'], {'Type': 'Sphere', 'Size': vector(0.5, 1, 1)}),
    patch(collider['id'], {'Type': 'Capsule', 'Size': vector(0.75, 1.5, 1),
                          'Position': vector(0.25, 0, 0), 'Layer': 2147483648, 'LayerMask': 4294967295,
                          'IsTrigger': True, 'TriggerMask': 0}),
    patch(rigid['id'], {'Mass': 7, 'GravityEnabled': False, 'PositionLock': vector(True, False, True),
                       'RotationLock': vector(False, True, True), 'MaxLinearVelocity': 12, 'MaxAngularVelocity': 3}),
    patch(rigid['id'], {'Mass': 9})
])
assert changed['results'][-1]['component']['properties']['Mass'] == 9
modified = authored()
request('/history/undo', {})
assert authored() == initial
request('/history/redo', {})
assert authored() == modified

# Observation includes writable physics state, even though physics is inactive while stopped.
observed = request('/observe', {'after': changed['observationToken'], 'entities': [body], 'width': 320})
for name in ['Collider', 'RigidBody']:
    actual = next(c for c in observed['entities'][0]['components'] if c['type'] == name)
    assert actual['properties'] == component(body, name)['properties']

# Duplicate a physics hierarchy; history restores identities and independent state.
duplicated = edit([{'op': 'entity.duplicate', 'target': {'id': floor}, 'ref': 'copy'}])
copy_id = duplicated['refs']['copy']
assert component(copy_id, 'Collider')['properties'] == component(floor, 'Collider')['properties']
assert len(duplicated['results'][0]['duplicatedEntities']) == 2
request('/history/undo', {})
request('/history/redo', {})
assert component(copy_id, 'Collider')['id'] != component(floor, 'Collider')['id']
edit([{'op': 'entity.delete', 'target': {'id': copy_id}}])
request('/history/undo', {})
assert component(copy_id, 'Collider')['properties'] == component(floor, 'Collider')['properties']
request('/history/redo', {})

duplicate_body = edit([{'op': 'entity.duplicate', 'target': {'id': body}, 'ref': 'copy'}])['refs']['copy']
assert component(duplicate_body, 'RigidBody')['properties'] == component(body, 'RigidBody')['properties']
edit([patch(component(duplicate_body, 'RigidBody')['id'], {'Mass': 2})])
assert component(body, 'RigidBody')['properties']['Mass'] == 9
edit([{'op': 'entity.delete', 'target': {'id': duplicate_body}}])

# Removal can leave a dormant body; re-addition starts from defaults with a fresh identity.
# Retrying the same identified request must not remove/recreate it twice.
for name in ['Collider', 'RigidBody']:
    previous = component(body, name)
    operations = [{'op': 'component.remove', 'target': {'id': previous['id']}},
                  {'op': 'component.add', 'target': {'id': body}, 'type': name}]
    key = uuid.uuid4().hex
    result = edit(operations, key=key)
    added = result['results'][1]['component']
    assert added['id'] != previous['id'] and added['properties'] == schema['components'][name]['defaults']
    assert edit(operations, key=key) == result
    edit([patch(previous['id'], {})], expected=400)
    edit([{'op': 'component.add', 'target': {'id': body}, 'type': name}], expected=400)
    request('/history/undo', {})
    assert component(body, name)['id'] == previous['id']
    assert component(body, name)['properties'] == previous['properties']
    request('/history/redo', {})
    assert component(body, name)['id'] == added['id']
    request('/history/undo', {})

edit([{'op': 'component.remove', 'target': {'id': collider['id']}}])
assert component(body, 'RigidBody')['properties']['Mass'] == 9
request('/history/undo', {})
edit([{'op': 'component.remove', 'target': {'id': rigid['id']}}])
assert component(body, 'Collider')['properties']['Type'] == 'Capsule'
request('/history/undo', {})

path = 'verification/physics-' + uuid.uuid4().hex
saved_scene = authored()
request('/level/save-as', {'path': path})
assert not request('/level/status')['unsavedChanges']
edit([patch(rigid['id'], {'Mass': 11})])
assert request('/level/status')['unsavedChanges']
request('/history/undo', {})
assert not request('/level/status')['unsavedChanges']
manifest_path.write_text(json.dumps({'path': path, 'scene': saved_scene}, indent=2) + '\n')
request('/level/load', {'path': path})
assert authored() == saved_scene
print('PASS: physics schema, validation, creation/update/removal, readback/observation, retries,')
print('      duplication, history, dormant bodies, unsigned masks and save/reload.')

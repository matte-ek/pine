"""Entity property HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19026')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-entity-results'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
session = None


def request(path, body=None, expected=200, key=None):
    headers = {'Content-Type': 'application/json'}
    if key:
        headers.update({'X-Pine-Session': session, 'Idempotency-Key': key})
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(args.url + path, data=data, headers=headers)
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        return json.loads(payload)


def edit(operations, expected=200, key=None):
    return request('/edit', {'version': 1, 'operations': operations}, expected, key)


def update(entity_id, properties):
    return {'op': 'entity.update', 'target': {'id': entity_id}, 'properties': properties}


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def assert_properties(state, name, active, static):
    assert (state['name'], state['active'], state['static']) == (name, active, static), state


def reject_unchanged(operations, entity_ids, failing_operation, path):
    before = [entity(entity_id) for entity_id in entity_ids]
    before_entities = request('/entities')
    result = edit(operations, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['operation'] == failing_operation and result['path'] == path, result
    assert [entity(entity_id) for entity_id in entity_ids] == before
    assert request('/entities') == before_entities


def observe(token, entity_ids, name):
    result = request('/observe', {'after': token, 'entities': entity_ids, 'width': 640})
    assert result['frame']['id'] > token['frame']
    assert result['frame']['revision'] >= token['revision']
    assert result['frame']['sceneGeneration'] == token['sceneGeneration']
    png = base64.b64decode(result['image']['data'], validate=True)
    (args.output / (name + '.png')).write_bytes(png)
    for observed in result['entities']:
        current = entity(observed['id'])
        for field in ['name', 'active', 'static', 'parent', 'components']:
            assert observed[field] == current[field], (field, observed, current)
    return png


session = request('/requests')['session']
schema = request('/edit/schema')
assert 'entity.update' in schema['operations']
assert set(schema['entity']['properties']) == {'name', 'active', 'static'}
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

created = edit([
    {'op': 'entity.create', 'ref': 'parent'},
    {'op': 'entity.create', 'ref': 'cube', 'parent': {'ref': 'parent'}, 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}
    ]},
    {'op': 'entity.create', 'ref': 'lamp', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 3, 'y': 5, 'z': 6}}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': 5, 'Range': 30}}
    ]}
])
parent_id, cube_id, lamp_id = [created['refs'][name] for name in ['parent', 'cube', 'lamp']]
entity_ids = [parent_id, cube_id, lamp_id]
cube = created['results'][1]['entity']
assert_properties(cube, 'Entity', True, False)
transform_id = next(c['id'] for c in cube['components'] if c['type'] == 'Transform')
original_components = entity(cube_id)['components']

# Partial patches compose in operation order, even on inactive entities. Empty is a no-op.
renamed = 'Renamed cube — 松'
patched = edit([
    update(cube_id, {'name': renamed}), update(cube_id, {'active': False}),
    update(cube_id, {'static': True}), update(cube_id, {}), update(cube_id, {'active': True})
])
for result, active, static in zip(patched['results'], [True, False, False, False, True],
                                [False, False, True, True, True]):
    assert result['entity']['id'] == cube_id
    assert_properties(result['entity'], renamed, active, static)
assert entity(cube_id)['components'] == original_components
assert entity(cube_id)['parent']['id'] == parent_id
assert_properties(entity(cube_id), renamed, True, True)

# A later invalid operation prevents earlier valid updates, additions and creations.
invalid_properties = [({'name': value}, 'name') for value in ['', 'a\0b', None, 7, True]]
invalid_properties += [({flag: value}, flag) for flag in ['active', 'static']
                       for value in [None, 0, 1, 'true', [], {}]]
invalid_properties += [({'unknown': True}, 'unknown'), ({'Name': 'wrong case'}, 'Name')]
for properties, field in invalid_properties:
    reject_unchanged([update(cube_id, {'name': 'Must not change'}), update(cube_id, properties)],
                     entity_ids, 1, '/operations/1/properties/' + field)
for invalid, suffix in [
    (update(cube_id, None), '/properties'),
    (update(cube_id, []), '/properties'),
    ({'op': 'entity.update', 'target': {'id': cube_id}}, '/properties'),
    ({**update(cube_id, {}), 'extra': 1}, '/extra'),
    ({**update(cube_id, {}), 'target': {'ref': 'cube'}}, '/target/ref'),
    ({**update(cube_id, {}), 'target': {'id': cube_id, 'ref': 'cube'}}, '/target/ref'),
    (update(transform_id, {}), '/target/id'),
    (update('invalid', {}), '/target/id'),
    (update('ffffffffffffffff-ffffffffffffffff', {}), '/target/id')
]:
    reject_unchanged([
        {'op': 'entity.create', 'name': 'Must not exist'},
        {'op': 'component.add', 'target': {'id': parent_id}, 'type': 'Light'}, invalid
    ], entity_ids, 2, '/operations/2' + suffix)
temporary = next(e for e in request('/entities')['entities'] if e['temporary'])
reject_unchanged([update(temporary['id'], {'active': False})],
                 entity_ids + [temporary['id']], 0, '/operations/0/target/id')

# Duplicate names are valid. Retried mutations return their original results without
# overwriting a newer edit, and identity reuse with another payload is rejected.
key = uuid.uuid4().hex
operations = [update(parent_id, {'name': renamed, 'active': False, 'static': True})]
first = edit(operations, key=key)
edit([update(parent_id, {'name': 'Newer parent name'})])
assert edit(operations, key=key) == first
edit([update(parent_id, {'name': 'Different payload'})], expected=409, key=key)
assert_properties(entity(parent_id), 'Newer parent name', False, True)
assert_properties(entity(cube_id), renamed, True, True)

# An inactive parent does not hide an active child; the target's active flag does.
framed = request('/camera/frame', {'entities': [{'id': cube_id}], 'padding': 2.5,
                                  'direction': {'x': 0, 'y': -.2, 'z': -1}})
visible = observe(framed['observationToken'], entity_ids, 'visible-static-child')
hidden = edit([update(cube_id, {'active': False})])
invisible = observe(hidden['observationToken'], entity_ids, 'inactive-child')
assert visible != invisible
restored = edit([update(cube_id, {'active': True})])
visible_again = observe(restored['observationToken'], entity_ids, 'reactivated-child')
assert visible_again != invisible
assert entity(cube_id)['components'] == original_components

# Static entities remain editable. Exercise flag and component updates in one batch.
moved = edit([
    update(cube_id, {'static': False}),
    {'op': 'component.update', 'target': {'id': transform_id},
     'properties': {'LocalPosition': {'x': 1, 'y': 0, 'z': 0}}},
    update(cube_id, {'static': True})
])
shifted = observe(moved['observationToken'], entity_ids, 'moved-static-child')
assert shifted != visible_again
assert_properties(entity(cube_id), renamed, True, True)

print('PASS: entity patches, validation, identity, hierarchy semantics, retries and rendered effects.')
print('Inspect captures in', args.output)

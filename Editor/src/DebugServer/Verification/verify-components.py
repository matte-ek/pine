"""Component lifecycle HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19025')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-component-results'))
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


def add(entity_id, component_type, properties=None):
    operation = {'op': 'component.add', 'target': {'id': entity_id}, 'type': component_type}
    if properties is not None:
        operation['properties'] = properties
    return operation


def remove(component_id):
    return {'op': 'component.remove', 'target': {'id': component_id}}


def update(component_id, properties):
    return {'op': 'component.update', 'target': {'id': component_id}, 'properties': properties}


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def reject_unchanged(operations, entity_ids, failing_operation):
    before = [entity(entity_id) for entity_id in entity_ids]
    before_entities = request('/entities')
    result = edit(operations, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['operation'] == failing_operation, result
    assert result['path'].startswith('/operations/' + str(failing_operation)), result
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
        assert observed['components'] == entity(observed['id'])['components']
    return png


session = request('/requests')['session']
schema = request('/edit/schema')
assert {'component.add', 'component.remove'} <= set(schema['operations'])
for name in ['ModelRenderer', 'Light', 'Transform']:
    assert schema['components'][name]['addable'] == (name != 'Transform')
    assert schema['components'][name]['removable'] == (name != 'Transform')
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

created = edit([
    {'op': 'entity.create', 'ref': 'cube', 'name': 'Component lifecycle cube'},
    {'op': 'entity.create', 'ref': 'lamp', 'name': 'Component lifecycle lamp', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 3, 'y': 5, 'z': 6}}}
    ]}
])
cube_id, lamp_id = created['refs']['cube'], created['refs']['lamp']
entity_ids = [cube_id, lamp_id]
transform_id = created['results'][0]['entity']['components'][0]['id']

# Creation defaults, property conversion and per-operation identity readback.
added = edit([add(cube_id, 'ModelRenderer'), add(lamp_id, 'Light')])
model = added['results'][0]['component']
light = added['results'][1]['component']
assert model['properties'] == schema['components']['ModelRenderer']['defaults']
assert light['properties'] == schema['components']['Light']['defaults']
assert added['results'][0]['entityId'] == cube_id
assert added['results'][1]['entityId'] == lamp_id

# Protect Transform, exclude unsupported lifecycles, and reject wrong ID kinds/envelopes.
for invalid in [
    add(cube_id, 'Transform'), remove(transform_id),
    add(cube_id, 'Camera', { 'Type': 'Orthographic' }), add(cube_id, 'RigidBody2D'), add(cube_id, 'Collider2D'),
    add(model['id'], 'Light'), remove(cube_id), remove('invalid'),
    {**add(cube_id, 'Light'), 'target': {'ref': 'cube'}},
    {**remove(model['id']), 'properties': {}},
    {**remove(model['id']), 'target': {'id': model['id'], 'ref': 'cube'}},
    add(cube_id, 'Light', {'SpotlightInnerAngle': 80, 'SpotlightOuterAngle': 20}),
    add(cube_id, 'Light', {'unknown': 1}),
    {**add(cube_id, 'Light'), 'properties': None}
]:
    reject_unchanged([update(transform_id, {'LocalPosition': {'x': 7, 'y': 0, 'z': 0}}), invalid], entity_ids, 1)

# The editor's own temporary camera entity is outside the editing surface.
temporary = next(e for e in request('/entities')['entities'] if e['temporary'])
temporary_component = entity(temporary['id'])['components'][0]['id']
reject_unchanged([add(temporary['id'], 'Light')], entity_ids + [temporary['id']], 0)
reject_unchanged([remove(temporary_component)], entity_ids + [temporary['id']], 0)

# Membership is checked against the preceding operations, including removed IDs.
reject_unchanged([add(cube_id, 'ModelRenderer')], entity_ids, 0)
reject_unchanged([add(cube_id, 'Light'), add(cube_id, 'Light')], entity_ids, 1)
reject_unchanged([remove(model['id']), remove(model['id'])], entity_ids, 1)
reject_unchanged([remove(model['id']), update(model['id'], {})], entity_ids, 1)
reject_unchanged([remove(model['id']), add(cube_id, 'ModelRenderer'), update(model['id'], {})], entity_ids, 2)
reject_unchanged([{'op': 'entity.create', 'name': 'Must not exist'}, remove(transform_id)], entity_ids, 1)
reject_unchanged([remove(model['id']), add(cube_id, 'ModelRenderer', {'MeshIndex': 0})], entity_ids, 1)

# Replace in order. Old IDs stay invalid even when Pine reuses the component pool slot.
replacement_key = uuid.uuid4().hex
replacement_ops = [
    update(light['id'], {'Intensity': 2}), remove(light['id']),
    add(lamp_id, 'Light', {'Type': 'PointLight', 'Intensity': 5, 'Range': 30}),
    remove(model['id']),
    add(cube_id, 'ModelRenderer', {'Model': {'path': 'engine/primitive/cube'}})
]
replaced = edit(replacement_ops, key=replacement_key)
assert replaced['completed'] == 5
assert replaced['results'][0]['component']['properties']['Intensity'] == 2
assert replaced['results'][1]['removedComponent'] == {'id': light['id'], 'type': 'Light'}
assert replaced['results'][3]['removedComponent'] == {'id': model['id'], 'type': 'ModelRenderer'}
assert edit(replacement_ops, key=replacement_key) == replaced
old_model_id, old_light_id = model['id'], light['id']
model = replaced['results'][4]['component']
light = replaced['results'][2]['component']
assert model['id'] != old_model_id and light['id'] != old_light_id
assert set(model['properties']['Model']) == {'id'}
for stale_id in [old_model_id, old_light_id]:
    reject_unchanged([remove(stale_id)], entity_ids, 0)
    reject_unchanged([update(stale_id, {})], entity_ids, 0)

framed = request('/camera/frame', {'entities': [{'id': cube_id}], 'padding': 2.5,
                                  'direction': {'x': 0, 'y': -.2, 'z': -1}})
with_light = observe(framed['observationToken'], entity_ids, 'with-light')
removal_key = uuid.uuid4().hex
removed = edit([remove(light['id'])], key=removal_key)
assert removed['results'][0]['entityId'] == lamp_id
without_light = observe(removed['observationToken'], entity_ids, 'without-light')
assert without_light != with_light

# A retained removal retry cannot destroy a replacement in the same pool slot.
add_key = uuid.uuid4().hex
add_ops = [add(lamp_id, 'Light', light['properties'])]
restored = edit(add_ops, key=add_key)
restored_light = restored['results'][0]['component']
assert restored_light['id'] != light['id']
assert edit([remove(light['id'])], key=removal_key) == removed
assert edit(add_ops, key=add_key) == restored
assert [c['id'] for c in entity(lamp_id)['components'] if c['type'] == 'Light'] == [restored_light['id']]
restored_png = observe(restored['observationToken'], entity_ids, 'restored-light')
assert restored_png != without_light

removed_model = edit([remove(model['id'])])
without_model = observe(removed_model['observationToken'], entity_ids, 'without-model')
assert without_model != restored_png
assert [c['id'] for c in entity(cube_id)['components']] == [transform_id]
reject_unchanged([remove(model['id'])], entity_ids, 0)

print('PASS: component add/remove, defaults, dependencies, ordered validation, stale IDs, retries and rendered effects.')
print('Inspect captures in', args.output)

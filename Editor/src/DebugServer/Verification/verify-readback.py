"""Writable-state HTTP recipe; run only against a disposable Pine project."""

import argparse
import json
import math
from pathlib import Path
import urllib.error
import urllib.request


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19031')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-readback-results'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def request(path, body=None, expected=200):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(args.url + path, data=data,
                                 headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        return json.loads(payload)


def edit(operations):
    result = request('/edit', {'version': 1, 'operations': operations})
    assert result['completed'] == len(operations), result
    return result


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def component(state, component_type):
    return next(item for item in state['components'] if item['type'] == component_type)


def assert_close(actual, expected):
    # Reapplying a quaternion may introduce float32 normalization roundoff.
    if isinstance(expected, dict):
        assert actual.keys() == expected.keys(), (actual, expected)
        for key in expected:
            assert_close(actual[key], expected[key])
    elif isinstance(expected, list):
        assert len(actual) == len(expected), (actual, expected)
        for actual_item, expected_item in zip(actual, expected):
            assert_close(actual_item, expected_item)
    elif isinstance(expected, float):
        assert math.isclose(actual, expected, rel_tol=1e-6, abs_tol=1e-6), (actual, expected)
    else:
        assert actual == expected, (actual, expected)


def round_trip(state):
    operations = [{'op': 'entity.update', 'target': {'id': state['id']},
                   'properties': state['properties']}]
    for item in state['components']:
        if item['properties'] is not None:
            operations.append({'op': 'component.update', 'target': {'id': item['id']},
                               'properties': item['properties']})
    edit(operations)
    current = entity(state['id'])
    assert_close(current, state)
    return current


schema = request('/edit/schema')
readback = schema['readback']
assert readback['entityEndpoint'] == '/entity' and readback['observationEndpoint'] == '/observe'
assert readback['entityProperties'] == 'properties'
assert readback['componentProperties'] == 'components[].properties'
assert readback['unsupportedComponents'] is None and readback['temporaryHierarchies'] is None
assert readback['assetReferenceForm'] == 'id'
assert readback['availableDuringPlay'] and not readback['validatesCurrentValues']
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

created = edit([
    {'op': 'entity.create', 'name': 'Readback parent', 'ref': 'parent', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 20, 'y': 3, 'z': -7}}}
    ]},
    {'op': 'entity.create', 'name': 'Readback child', 'ref': 'child', 'parent': {'ref': 'parent'},
     'components': [{'type': 'Light'}, {'type': 'ModelRenderer'}]}
])
parent, child = created['refs']['parent'], created['refs']['child']
initial = entity(child)
assert initial['properties'] == {'name': 'Readback child', 'active': True, 'static': False}
assert set(initial['properties']) == set(schema['entity']['properties'])
for item in initial['components']:
    descriptor = schema['components'][item['type']]
    assert item['properties'] == descriptor['defaults']
    assert set(item['properties']) == set(descriptor['properties'])
    assert 'data' in item and 'active' in item
    response_item = component(created['results'][1]['entity'], item['type'])
    assert item['properties'] == response_item['properties']
assert component(initial, 'ModelRenderer')['properties']['Model'] is None
assert component(initial, 'ModelRenderer')['properties']['OverrideMaterial'] is None
assert request('/entity?internalId=' + str(initial['internalId'])) == initial
round_trip(initial)

transform_id = component(initial, 'Transform')['id']
light_id = component(initial, 'Light')['id']
renderer_id = component(initial, 'ModelRenderer')['id']
models = request('/assets?type=Model')['assets']
model = next(item for item in models if item['path'] == 'engine/primitive/cube')
materials = request('/assets?type=Material')['assets']
assert materials, 'The disposable project must include the engine materials.'
material = materials[0]

edit([
    {'op': 'entity.update', 'target': {'id': child},
     'properties': {'name': 'Readback café 🌲', 'active': False, 'static': True}},
    {'op': 'component.update', 'target': {'id': transform_id}, 'properties': {
        'LocalPosition': {'x': 1.25, 'y': -2, 'z': 4},
        'LocalRotation': {'x': 0, 'y': 1, 'z': 0, 'w': 2},
        'LocalScale': {'x': 2, 'y': 0.5, 'z': 3}}},
    {'op': 'component.update', 'target': {'id': renderer_id}, 'properties': {
        'Model': {'path': model['path']}, 'OverrideMaterial': {'path': material['path']}, 'MeshIndex': 0,
        'CastShadows': False, 'ReceiveShadows': False}},
    {'op': 'component.update', 'target': {'id': light_id}, 'properties': {
        'Type': 'SpotLight', 'Color': {'x': 0.25, 'y': 0.5, 'z': 1}, 'Intensity': 3.5,
        'Range': 22, 'CastShadows': False, 'SpotlightInnerAngle': 12, 'SpotlightOuterAngle': 35}}
])
configured = entity(child)
assert configured['properties'] == {'name': 'Readback café 🌲', 'active': False, 'static': True}
assert component(configured, 'Transform')['properties']['LocalPosition'] == {'x': 1.25, 'y': -2, 'z': 4}
rotation = component(configured, 'Transform')['properties']['LocalRotation']
assert math.isclose(sum(value * value for value in rotation.values()), 1, rel_tol=1e-6)
assert component(configured, 'ModelRenderer')['properties'] == {
    'Model': {'id': model['uid']}, 'OverrideMaterial': {'id': material['uid']}, 'MeshIndex': 0,
    'CastShadows': False, 'ReceiveShadows': False}
assert component(configured, 'Light')['properties'] == {
    'Type': 'SpotLight', 'Color': {'x': 0.25, 'y': 0.5, 'z': 1}, 'Intensity': 3.5,
    'Range': 22, 'CastShadows': False, 'SpotlightInnerAngle': 12, 'SpotlightOuterAngle': 35}
round_trip(configured)

# Every enum choice is a wire name, and full readback remains valid after partial edits.
for light_type in schema['components']['Light']['properties']['Type']['values']:
    edit([{'op': 'component.update', 'target': {'id': light_id}, 'properties': {'Type': light_type}}])
    state = entity(child)
    assert component(state, 'Light')['properties']['Type'] == light_type
    round_trip(state)

edit([{'op': 'component.update', 'target': {'id': renderer_id},
       'properties': {'Model': None, 'OverrideMaterial': None, 'MeshIndex': -1,
                      'CastShadows': True, 'ReceiveShadows': True}}])
cleared = round_trip(entity(child))
assert component(cleared, 'ModelRenderer')['properties'] == schema['components']['ModelRenderer']['defaults']

# Protected editor entities expose their inspection data without advertising writable patches.
temporary = [item for item in request('/entities')['entities'] if item['temporary']]
assert temporary
for item in temporary:
    state = entity(item['id'])
    assert state['properties'] is None
    assert all(c['properties'] is None and 'data' in c for c in state['components'])

# Observation reads the same live values, including intervening edits after an older token.
first = edit([{'op': 'component.update', 'target': {'id': light_id}, 'properties': {'Intensity': 7}}])
edit([{'op': 'component.update', 'target': {'id': light_id}, 'properties': {'Intensity': 9}}])
before_observation = entity(child)
observation = request('/observe', {'after': first['observationToken'], 'entities': [child], 'width': 64})
assert observation['entities'] == [before_observation]
assert observation['timing']['entities'] == 'post-render-before-ui'
assert component(observation['entities'][0], 'Light')['properties']['Intensity'] == 9
assert entity(child) == before_observation
round_trip(observation['entities'][0])

# Component membership and IDs track live state after removal and pool slot reuse.
edit([{'op': 'component.remove', 'target': {'id': light_id}}])
assert all(item['type'] != 'Light' for item in entity(child)['components'])
edit([{'op': 'component.add', 'target': {'id': child}, 'type': 'Light'}])
replacement = component(entity(child), 'Light')
assert replacement['id'] != light_id
assert replacement['properties'] == schema['components']['Light']['defaults']
round_trip(entity(child))

for name, state in [('defaults', initial), ('configured', configured), ('cleared', cleared)]:
    (args.output / (name + '.json')).write_text(json.dumps(state, indent=2) + '\n')
(args.output / 'observation.json').write_text(json.dumps(observation, indent=2) + '\n')

edit([{'op': 'entity.delete', 'target': {'id': parent}}])
request('/entity?id=' + child, expected=404)
print('PASS: defaults, local transforms, entity flags, all light enums, asset IDs/nulls, unchanged and edited')
print('round trips, protected entities, observation readback, component replacement and deleted targets.')

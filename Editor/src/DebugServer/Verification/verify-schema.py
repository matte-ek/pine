"""Schema discovery HTTP recipe; run only against a disposable Pine project."""

import argparse
import json
from pathlib import Path
import urllib.error
import urllib.request


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19030')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-schema-results'))
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


schema = request('/edit/schema')
(args.output / 'schema.json').write_text(json.dumps(schema, indent=2) + '\n')
operations = schema['operationSchemas']
version = schema['requestSchema']['fields']['version']['const']


def operation(operation_name, **fields):
    descriptor = operations[operation_name]
    result = {'op': descriptor['fields']['op']['const'], **fields}
    assert set(descriptor['required']) <= set(result)
    assert set(result) <= set(descriptor['fields'])
    return result


def batch(items):
    return {'version': version, 'operations': items}


def edit(items):
    result = request('/edit', batch(items))
    assert result['completed'] == len(items), result
    return result


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def reject(body, path):
    before = request('/entities')
    states = [entity(entity_id) for entity_id in watched]
    result = request('/edit', body, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['path'] == path, result
    assert request('/entities') == before
    assert [entity(entity_id) for entity_id in watched] == states


expected_operations = {
    'entity.create', 'entity.update', 'entity.reparent', 'entity.delete',
    'entity.duplicate', 'entity.place', 'entity.aim', 'component.add', 'component.update', 'component.remove'
}
assert set(operations) == set(schema['operations']) == expected_operations
assert schema['requestSchema']['additionalFields'] is False
assert schema['requestSchema']['fields']['operations']['minItems'] == 1
assert schema['requestSchema']['fields']['operations']['maxItems'] == schema['maxOperations']
assert schema['maxBodyBytes'] == 256 * 1024 and schema['maxJsonDepth'] == 32
assert schema['requiredPlayState'] == 'Stopped'
assert schema['undo'] is True and schema['rollback'] is False
assert schema['batchRules']['validation'] == 'wholeBatchBeforeMutation'

creation = operations['entity.create']['fields']
addition = operations['component.add']['fields']
entry = creation['components']['items']
assert set(entry['fields']['type']['values']) == set(schema['components'])
assert set(addition['type']['values']) == {
    name for name, component in schema['components'].items() if component['addable']
}
assert entry['fields']['properties']['selectedBy'] == addition['properties']['selectedBy'] == 'type'
assert addition['properties']['whenOmitted'] == 'defaults'
assert operations['component.update']['fields']['properties']['selectedBy'] == 'targetComponentType'
assert operations['entity.update']['fields']['properties']['type'] == 'entityProperties'
assert creation['parent']['forms'] == ['id', 'ref']
assert not creation['parent'].get('nullable', False)
assert creation['parent']['whenOmitted'] == 'sceneRoot'
assert operations['entity.reparent']['fields']['parent']['nullable'] is True
assert operations['entity.reparent']['fields']['parent']['whenNull'] == 'sceneRoot'

references = schema['referenceRules']
assert references['exactlyOneForm'] and not references['additionalFields']
assert references['batchRefs']['earlierOperationsOnly'] and references['batchRefs']['unique']
assert set(references['batchRefs']['acceptedBy']) == {'entity.create.parent', 'entity.reparent.parent'}
assert references['asset']['forms'] == ['id', 'path']
assert references['asset']['mustBeLoaded'] and references['asset']['responseForm'] == 'id'

# Build a hierarchy from discovered operation and component descriptors.
created = edit([
    operation('entity.create', ref='root'),
    operation('entity.create', ref='child', parent={'ref': 'root'}, components=[{'type': 'Light'}])
])
root, child = created['refs']['root'], created['refs']['child']
watched = [root, child]
assert created['results'][0]['entity']['name'] == creation['name']['default']
light = next(c for c in created['results'][1]['entity']['components'] if c['type'] == 'Light')
assert light['properties'] == schema['components']['Light']['defaults']

samples = {
    'entity.create': operation('entity.create'),
    'entity.aim': operation('entity.aim', target={'id': root}, point={'x': 0, 'y': 0, 'z': -10},
                            forwardAxis='-Z', upAxis='+Y', up={'x': 0, 'y': 1, 'z': 0}),
    'entity.place': operation('entity.place', target={'id': root},
                              surface={'point': {'x': 1, 'y': 2, 'z': 3}, 'normal': {'x': 0, 'y': 1, 'z': 0}},
                              anchor={'type': 'localPoint', 'point': {'x': 0, 'y': 0, 'z': 0}}),
    'entity.update': operation('entity.update', target={'id': root}, properties={'name': 'Schema root'}),
    'entity.reparent': operation('entity.reparent', target={'id': child}, parent=None),
    'entity.delete': operation('entity.delete', target={'id': child}),
    'entity.duplicate': operation('entity.duplicate', target={'id': root}, ref='copy'),
    'component.add': operation('component.add', target={'id': root}, type='Light'),
    'component.update': operation('component.update', target={'id': light['id']}, properties={'Intensity': 4}),
    'component.remove': operation('component.remove', target={'id': light['id']})
}

# Each advertised envelope rejects unknown fields and every missing required field.
# A valid preceding creation makes accidental partial mutation observable.
prefix = operation('entity.create', name='Must not be created')
for name, sample in samples.items():
    descriptor = operations[name]
    assert descriptor['type'] == 'object' and not descriptor['additionalFields']
    reject(batch([prefix, {**sample, 'unknown': True}]), '/operations/1/unknown')
    for field in descriptor['required']:
        incomplete = {key: value for key, value in sample.items() if key != field}
        reject(batch([prefix, incomplete]), '/operations/1/' + field)

    if 'target' in sample:
        target = descriptor['fields']['target']
        expected_kind = 'component' if name in ('component.update', 'component.remove') else 'entity'
        assert target['kind'] == expected_kind and target['forms'] == ['id']
        reject(batch([prefix, {**sample, 'target': {'ref': 'root'}}]), '/operations/1/target/ref')
        wrong_id = root if expected_kind == 'component' else light['id']
        reject(batch([prefix, {**sample, 'target': {'id': wrong_id}}]), '/operations/1/target/id')

for field in schema['requestSchema']['required']:
    incomplete = batch([prefix])
    del incomplete[field]
    reject(incomplete, '/' + field)
reject({**batch([prefix]), 'unknown': True}, '/unknown')
reject({'version': version + 1, 'operations': [prefix]}, '/version')
reject(batch([]), '/operations')
reject(batch([prefix] * (schema['maxOperations'] + 1)), '/operations')

for invalid_parent, suffix in [
    (None, ''), ({}, ''), ({'id': root, 'ref': 'root'}, ''),
    ({'ref': 'future'}, '/ref'), ({'path': 'root'}, '/path'),
    ({'id': '0-0000000000000001'}, '/id')
]:
    reject(batch([prefix, operation('entity.create', parent=invalid_parent)]), '/operations/1/parent' + suffix)
reject(batch([operation('entity.create', ref='same'), operation('entity.create', ref='same')]), '/operations/1/ref')
reject(batch([operation('entity.create', parent={'ref': 'future'}), operation('entity.create', ref='future')]),
       '/operations/0/parent/ref')
reject(batch([operation('entity.create', parent={'ref': 'root'})]), '/operations/0/parent/ref')
for field in entry['required']:
    incomplete = {'type': 'Light', 'properties': {}}
    del incomplete[field]
    reject(batch([operation('entity.create', components=[incomplete])]), '/operations/0/components/0/' + field)
reject(batch([operation('entity.create', components=[{'type': 'Light', 'unknown': True}])]),
       '/operations/0/components/0/unknown')
reject(batch([operation('entity.create', components=[{'type': 'Light'}, {'type': 'Light'}])]),
       '/operations/0/components/1/type')
reject(batch([{**samples['component.add'], 'properties': None}]), '/operations/0/properties')

# Exercise every operation with valid schema-built requests, including both parent forms.
updated = edit([samples['entity.update'], samples['component.add'], samples['component.update']])
assert updated['results'][0]['entity']['name'] == 'Schema root'
assert updated['results'][1]['component']['properties'] == schema['components']['Light']['defaults']
assert updated['results'][2]['component']['properties']['Intensity'] == 4
edit([samples['entity.place'], samples['entity.aim']])
edit([samples['entity.reparent']])
edit([operation('entity.reparent', target={'id': child}, parent={'id': root})])
duplicated = edit([samples['entity.duplicate'], operation('entity.create', parent={'ref': 'copy'})])
copy_id = duplicated['refs']['copy']
edit([operation('entity.duplicate', target={'id': root}, ref='new-parent'),
      operation('entity.reparent', target={'id': child}, parent={'ref': 'new-parent'})])
edit([samples['component.remove']])
edit([samples['entity.delete']])
watched = [root]
reject(batch([operation('entity.create', parent={'id': child})]), '/operations/0/parent/id')
edit([operation('entity.delete', target={'id': copy_id})])

# Asset properties use the advertised loaded-asset forms and normalize readback to IDs.
model = edit([operation('component.add', target={'id': root}, type='ModelRenderer',
                       properties={'Model': {'path': 'engine/primitive/cube'}})])['results'][0]['component']
asset = model['properties']['Model']
assert set(asset) == {'id'}
edit([operation('component.update', target={'id': model['id']}, properties={'Model': asset})])
assert schema['components']['ModelRenderer']['properties']['Model']['nullable']
edit([operation('component.update', target={'id': model['id']}, properties={'Model': None})])
reject(batch([operation('component.update', target={'id': model['id']},
                        properties={'Model': {**asset, 'path': 'engine/primitive/cube'}})]),
       '/operations/0/properties/Model')

print('PASS: schema discovery, all operation envelopes, defaults, references, assets and whole-batch rejection.')
print('Schema response saved in', args.output)

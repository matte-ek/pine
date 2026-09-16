"""Hierarchy deletion HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19028')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-deletion-results'))
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


def delete(entity_id):
    return {'op': 'entity.delete', 'target': {'id': entity_id}}


def reparent(entity_id, parent):
    return {'op': 'entity.reparent', 'target': {'id': entity_id}, 'parent': parent}


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def component_id(description, component_type):
    return next(c['id'] for c in description['components'] if c['type'] == component_type)


def reject_unchanged(operations, watched, index, suffix):
    before = [entity(target) for target in watched]
    tree = request('/entities')
    result = edit(operations, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['operation'] == index, result
    assert result['path'] == '/operations/' + str(index) + suffix, result
    assert [entity(target) for target in watched] == before
    assert request('/entities') == tree


def assert_removed(result, descriptions):
    expected = {e['id']: {c['id'] for c in e['components']} for e in descriptions}
    removed = result['removedEntities']
    assert len(removed) == len(expected), removed
    assert {e['id']: set(e['componentIds']) for e in removed} == expected, removed
    for entity_id in expected:
        request('/entity?id=' + entity_id, expected=404)


def observe(token, watched, name):
    result = request('/observe', {'after': token, 'entities': watched, 'width': 640})
    assert result['frame']['id'] > token['frame']
    assert result['frame']['revision'] >= token['revision']
    assert result['frame']['sceneGeneration'] == token['sceneGeneration']
    png = base64.b64decode(result['image']['data'], validate=True)
    (args.output / (name + '.png')).write_bytes(png)
    return png


session = request('/requests')['session']
schema = request('/edit/schema')
assert 'entity.delete' in schema['operations']
assert schema['entity']['delete']['descendants'] == 'delete'
assert schema['entity']['delete']['deletedCreationRefs'] is None
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

created = edit([
    {'op': 'entity.create', 'ref': 'survivor', 'name': 'Deletion survivor'},
    {'op': 'entity.create', 'ref': 'group', 'name': 'Deletion group', 'parent': {'ref': 'survivor'}},
    {'op': 'entity.create', 'ref': 'cube', 'name': 'Deleted cube', 'parent': {'ref': 'group'},
     'components': [{'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'ref': 'lamp', 'name': 'Deletion light', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 0, 'y': 3, 'z': 5}}},
        {'type': 'Light', 'properties': {'Intensity': 5, 'Range': 20}}]}
])
survivor, group, cube, lamp = [created['refs'][name] for name in ['survivor', 'group', 'cube', 'lamp']]
watched = [survivor, group, cube, lamp]
cube_description = created['results'][2]['entity']
transform = component_id(cube_description, 'Transform')
renderer = component_id(cube_description, 'ModelRenderer')
temporary = next(e['id'] for e in request('/entities')['entities'] if e['temporary'])

# Strict envelopes and targets; a bad operation also prevents a preceding deletion.
invalid = [
    ({'op': 'entity.delete'}, '/target'),
    ({**delete(group), 'children': False}, '/children'),
    ({'op': 'entity.delete', 'target': {'ref': 'group'}}, '/target/ref'),
    ({'op': 'entity.delete', 'target': None}, '/target'),
    ({'op': 'entity.delete', 'target': {}}, '/target/id'),
    (delete('invalid'), '/target/id'),
    (delete('ffffffffffffffff-ffffffffffffffff'), '/target/id'),
    (delete(transform), '/target/id'),
    (delete(temporary), '/target/id'),
]
for operation, suffix in invalid:
    reject_unchanged([delete(group), operation], watched + [temporary], 1, suffix)

# Every target/parent form must reject a removed ancestor or descendant before mutation.
for operation, suffix in [
    (delete(group), '/target/id'),
    (delete(cube), '/target/id'),
    ({'op': 'entity.update', 'target': {'id': cube}, 'properties': {'name': 'stale'}}, '/target/id'),
    ({'op': 'component.add', 'target': {'id': cube}, 'type': 'Light'}, '/target/id'),
    ({'op': 'component.update', 'target': {'id': transform}, 'properties': {}}, '/target/id'),
    ({'op': 'component.remove', 'target': {'id': renderer}}, '/target/id'),
    (reparent(cube, None), '/target/id'),
    (reparent(lamp, {'id': cube}), '/parent/id'),
    ({'op': 'entity.create', 'parent': {'id': group}}, '/parent/id'),
]:
    reject_unchanged([delete(group), operation], watched, 1, suffix)

reject_unchanged([
    {'op': 'entity.create', 'ref': 'doomed', 'parent': {'id': group}},
    delete(group), {'op': 'entity.create', 'parent': {'ref': 'doomed'}}
], watched, 2, '/parent/ref')
reject_unchanged([
    {'op': 'entity.create', 'ref': 'doomed', 'parent': {'id': group}},
    delete(group), reparent(lamp, {'ref': 'doomed'})
], watched, 2, '/parent/ref')

# Detach before deleting preserves the child; attach before deleting includes it.
detached = edit([reparent(cube, None), delete(group)])
assert_removed(detached['results'][1], [created['results'][1]['entity']])
assert entity(cube)['parent'] is None
group_created = edit([{'op': 'entity.create', 'ref': 'group', 'parent': {'id': survivor}}])
group = group_created['refs']['group']
group_slot = entity(group)['internalId']
edit([reparent(cube, {'id': group}),
      {'op': 'entity.update', 'target': {'id': group}, 'properties': {'active': False, 'static': True}},
      {'op': 'entity.update', 'target': {'id': cube}, 'properties': {'static': True}}])

framed = request('/camera/frame', {'entities': [{'id': cube}], 'padding': 2})
before_png = observe(framed['observationToken'], [cube, survivor], 'before-deletion')

# Component membership at deletion includes earlier additions and excludes removals.
# Named and unnamed creations under the hierarchy must be deleted and free their slots.
key = uuid.uuid4().hex
operations = [
    {'op': 'component.remove', 'target': {'id': renderer}},
    {'op': 'component.add', 'target': {'id': cube}, 'type': 'Light'},
    {'op': 'entity.create', 'ref': 'doomed', 'parent': {'id': cube}},
    {'op': 'entity.create', 'parent': {'ref': 'doomed'}, 'components': [{'type': 'Light'}]},
    delete(group),
    {'op': 'entity.create', 'ref': 'replacement', 'name': 'Replacement after deletion'},
    {'op': 'entity.update', 'target': {'id': survivor}, 'properties': {'name': 'Still here'}},
]
deleted = edit(operations, key=key)
assert deleted['refs']['doomed'] is None
assert deleted['results'][4]['entityId'] == group
new_light = deleted['results'][1]['component']['id']
assert_removed(deleted['results'][4], [
    group_created['results'][0]['entity'],
    {'id': cube, 'components': [{'id': transform}, {'id': new_light}]},
    deleted['results'][2]['entity'], deleted['results'][3]['entity'],
])
after_png = observe(deleted['observationToken'], [survivor, lamp], 'after-deletion')
assert after_png != before_png
assert entity(survivor)['name'] == 'Still here'
assert entity(survivor)['children'] == []
replacement = deleted['refs']['replacement']
assert replacement not in {group, cube}
assert entity(replacement)['internalId'] == group_slot

# Historical results are replayed verbatim; neither retry nor stale IDs delete the replacement.
replacement_before = entity(replacement)
assert edit(operations, key=key) == deleted
edit([delete(replacement)], expected=409, key=key)
assert entity(replacement) == replacement_before
for stale in [group, cube]:
    reject_unchanged([delete(stale)], [replacement, survivor], 0, '/target/id')
for stale in [transform, renderer, new_light]:
    reject_unchanged([{'op': 'component.update', 'target': {'id': stale}, 'properties': {}}],
                     [replacement, survivor], 0, '/target/id')
request('/observe', {'after': deleted['observationToken'], 'entities': [cube]}, expected=400)
request('/camera/frame', {'entities': [{'id': cube}]}, expected=400)
# Deletion keeps the scene generation, so an earlier token can still observe survivors.
observe(created['observationToken'], [survivor], 'surviving-reference')

# Delete a child first and its ancestor second; remove/add/delete counts must not double-free.
tree = edit([
    {'op': 'entity.create', 'ref': 'root'},
    {'op': 'entity.create', 'ref': 'child', 'parent': {'ref': 'root'}, 'components': [{'type': 'Light'}]}
])
root, child = tree['refs']['root'], tree['refs']['child']
child_light = component_id(tree['results'][1]['entity'], 'Light')
result = edit([
    {'op': 'component.remove', 'target': {'id': child_light}},
    {'op': 'component.add', 'target': {'id': child}, 'type': 'Light'},
    delete(child), delete(root), {'op': 'entity.create', 'components': [{'type': 'Light'}]}
])
assert len(result['results'][2]['removedEntities']) == 1
assert len(result['results'][3]['removedEntities']) == 1

print('PASS: hierarchy deletion, ordered references, validation, slot reuse, retries and rendered removal.')
print('Inspect captures in', args.output)

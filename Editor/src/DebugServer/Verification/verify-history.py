"""History and persistence HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19032')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-history-results'))
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


def walk(entities):
    for entity in entities:
        if not entity['temporary']:
            yield entity
            yield from walk(entity['children'])


def without_slots(value):
    if isinstance(value, dict):
        return {key: without_slots(item) for key, item in value.items() if key != 'internalId'}
    if isinstance(value, list):
        return [without_slots(item) for item in value]
    return value


def scene():
    return [without_slots(request('/entity?id=' + entity['id']))
            for entity in walk(request('/entities')['entities'])]


def counts():
    state = request('/history')
    return state['undoCount'], state['redoCount']


def component(entity, name):
    return next(item for item in entity['components'] if item['type'] == name)


def restore_pair(before, after):
    assert request('/history/undo', {})['applied']
    assert scene() == before
    assert request('/history/redo', {})['applied']
    assert scene() == after


def capture(token, entity_ids, name):
    observation = request('/observe', {'after': token, 'entities': entity_ids, 'width': 640})
    (args.output / (name + '.png')).write_bytes(base64.b64decode(observation['image']['data']))
    observation['image'].pop('data')
    (args.output / (name + '.json')).write_text(json.dumps(observation, indent=2) + '\n')


assert request('/edit/schema')['undo'] is True
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'
initial = scene()
initial_count = counts()[0]
created = edit([
    {'op': 'entity.create', 'name': 'History root', 'ref': 'root', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'name': 'History child', 'ref': 'child', 'parent': {'ref': 'root'}, 'components': [
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': 3, 'Range': 30}}]},
    {'op': 'entity.create', 'name': 'History sibling', 'ref': 'sibling', 'parent': {'ref': 'root'}}
])
assert created['history'] == 'recorded'
root, child, sibling = (created['refs'][name] for name in ['root', 'child', 'sibling'])
assert counts() == (initial_count + 1, 0)
after_create = scene()
restore_pair(initial, after_create)
root_state = request('/entity?id=' + root)
child_state = request('/entity?id=' + child)
transform_id = component(root_state, 'Transform')['id']
light_id = component(child_state, 'Light')['id']
renderer_id = component(root_state, 'ModelRenderer')['id']

# A batch groups changes to different entities and components into one step.
changed = edit([
    {'op': 'entity.update', 'target': {'id': root}, 'properties': {'name': 'Saved root', 'static': True}},
    {'op': 'entity.update', 'target': {'id': child}, 'properties': {'active': False}},
    {'op': 'component.update', 'target': {'id': transform_id},
     'properties': {'LocalPosition': {'x': 4, 'y': 1, 'z': -2}}},
    {'op': 'component.update', 'target': {'id': light_id},
     'properties': {'Type': 'SpotLight', 'Intensity': 7, 'Range': 18, 'SpotlightOuterAngle': 40}},
    {'op': 'entity.reparent', 'target': {'id': sibling}, 'parent': {'id': child}}
])
after_change = scene()
restore_pair(after_create, after_change)
# Property-only child history must not change sibling order.
edit([{'op': 'entity.update', 'target': {'id': child}, 'properties': {'name': 'Updated child'}}])
restore_pair(after_change, scene())
request('/history/undo', {})

# A rejected batch preserves redo and creates no undo step.
before_rejection = counts()
edit([{'op': 'entity.create'}, {'op': 'component.update', 'target': {'id': light_id},
      'properties': {'Intensity': -1}}], expected=400)
assert counts() == before_rejection
assert scene() == after_change

# A new edit after undo drops the redo branch. Component ID/order survive removal undo.
edit([{'op': 'component.remove', 'target': {'id': renderer_id}},
      {'op': 'component.add', 'target': {'id': root}, 'type': 'Light'},
      {'op': 'component.add', 'target': {'id': root}, 'type': 'ModelRenderer'}])
after_membership = scene()
assert counts()[1] == 0
restore_pair(after_change, after_membership)
request('/history/undo', {})

# Creation, duplication and destruction reuse the recorded persistent IDs on redo.
duplicated = edit([{'op': 'entity.duplicate', 'target': {'id': root}, 'ref': 'copy'}])
after_duplicate = scene()
restore_pair(after_change, after_duplicate)
copy_id = duplicated['refs']['copy']
edit([{'op': 'entity.delete', 'target': {'id': copy_id}},
      {'op': 'entity.delete', 'target': {'id': child}}])
after_delete = scene()
restore_pair(after_duplicate, after_delete)
request('/history/undo', {})

# Identified retries of undo must not apply another history step.
session = request('/requests')['session']
headers = {'X-Pine-Session': session, 'Idempotency-Key': 'history-' + uuid.uuid4().hex}
undone = request('/history/undo', {}, headers=headers)
after_undo = scene()
undo_counts = counts()
assert request('/history/undo', {}, headers=headers) == undone
assert scene() == after_undo and counts() == undo_counts
request('/history/redo', {})

for endpoint in ['/history/undo', '/history/redo']:
    before_invalid = counts()
    request(endpoint, {'unexpected': True}, expected=400)
    assert counts() == before_invalid

# Save after undo/redo, then verify both in-memory and disk-loaded state.
child_transform = component(request('/entity?id=' + child), 'Transform')['id']
edit([
    {'op': 'entity.update', 'target': {'id': child}, 'properties': {'active': True}},
    {'op': 'component.update', 'target': {'id': child_transform},
     'properties': {'LocalPosition': {'x': 3, 'y': 4, 'z': 6}}},
    {'op': 'component.update', 'target': {'id': light_id},
     'properties': {'Type': 'PointLight', 'Intensity': 7, 'Range': 40}}
])
path = 'verification/history-' + uuid.uuid4().hex
assert request('/level/status')['unsavedChanges'] is True
saved = request('/level/save-as', {'path': path})
assert saved['fileWritten'] and not saved['unsavedChanges']
assert request('/level/status')['unsavedChanges'] is False
saved_scene = scene()
saved_counts = counts()
request('/level/save-as', {'path': path}, expected=400)
assert counts() == saved_counts and scene() == saved_scene
assert request('/level/save-as', {'path': path, 'overwrite': True})['id'] == saved['id']

edit([{'op': 'component.update', 'target': {'id': transform_id},
       'properties': {'LocalPosition': {'x': 6, 'y': 2, 'z': 0}}}])
assert request('/level/status')['unsavedChanges'] is True
request('/history/undo', {})
assert request('/level/status')['unsavedChanges'] is False
request('/history/redo', {})
assert request('/level/status')['unsavedChanges'] is True
assert request('/level/save', {})['id'] == saved['id']
assert request('/level/status')['unsavedChanges'] is False
saved_asset = next(asset for asset in request('/assets?type=Level')['assets'] if asset['uid'] == saved['id'])
assert saved_asset['modified'] is False
saved_scene = scene()
framed = request('/camera/frame', {'entities': [{'id': root}], 'padding': 2,
                                    'direction': {'x': 0, 'y': -0.2, 'z': -1}})
capture(framed['observationToken'], [root, child], 'saved')

# Save-as creates a distinct Level, retains history and keeps the live entity IDs.
second_path = path + '-copy'
second = request('/level/save-as', {'path': second_path})
assert second['id'] != saved['id'] and scene() == saved_scene
assert request('/level/status')['path'] == second_path
for invalid in ['../escape', '/absolute', 'engine/primitive/cube', 'Uppercase', 'invalid.passet', 'trailing/']:
    request('/level/save-as', {'path': invalid}, expected=400)
request('/level/save', {'path': 'unexpected'}, expected=400)
request('/level/save-as', {'path': second_path, 'overwrite': 'yes'}, expected=400)

# Compare authored state after reload; a level load deliberately gives scene objects new IDs.
def authored(entities):
    result = []
    for entity in entities:
        result.append({'name': entity['name'], 'properties': entity['properties'], 'tags': entity['tags'],
                       'parent': entity['parent']['name'] if entity['parent'] else None,
                       'children': [child['name'] for child in entity['children']],
                       'components': [{'type': c['type'], 'active': c['active'], 'properties': c['properties']}
                                      for c in entity['components']]})
    return result

request('/level/load', {'path': second_path})
reloaded = scene()
assert authored(reloaded) == authored(saved_scene)
assert counts() == (0, 0)
assert request('/history/undo', {})['applied'] is False
assert request('/level/status')['unsavedChanges'] is False
new_root = next(entity['id'] for entity in reloaded if entity['name'] == 'Saved root')
framed = request('/camera/frame', {'entities': [{'id': new_root}], 'padding': 2,
                                    'direction': {'x': 0, 'y': -0.2, 'z': -1}})
capture(framed['observationToken'], [new_root], 'reloaded')
(args.output / 'saved-scene.json').write_text(json.dumps(saved_scene, indent=2) + '\n')
(args.output / 'reloaded-scene.json').write_text(json.dumps(reloaded, indent=2) + '\n')
print('PASS: grouped undo/redo, identities, hierarchy/order, all edit operations, retry deduplication,')
print('validation, save/save-as, overwrite rejection, unsaved changes and save/reload/capture.')

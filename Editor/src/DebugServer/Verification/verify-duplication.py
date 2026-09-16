"""Hierarchy duplication HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19029')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-duplication-results'))
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
        assert response.code == expected, (path, response.code, payload[:2000])
        return json.loads(payload)


def edit(operations, expected=200, key=None):
    return request('/edit', {'version': 1, 'operations': operations}, expected, key)


def duplicate(entity_id, **fields):
    return {'op': 'entity.duplicate', 'target': {'id': entity_id}, **fields}


def delete(entity_id):
    return {'op': 'entity.delete', 'target': {'id': entity_id}}


def reparent(entity_id, parent):
    return {'op': 'entity.reparent', 'target': {'id': entity_id}, 'parent': parent}


def update(entity_id, **properties):
    return {'op': 'entity.update', 'target': {'id': entity_id}, 'properties': properties}


def component_update(component_id, **properties):
    return {'op': 'component.update', 'target': {'id': component_id}, 'properties': properties}


def component(description, kind):
    return next(c for c in description['components'] if c['type'] == kind)


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def reject_unchanged(operations, watched, index, suffix):
    before = [entity(target) for target in watched]
    tree = request('/entities')
    result = edit(operations, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['operation'] == index, result
    assert result['path'] == '/operations/' + str(index) + suffix, result
    assert [entity(target) for target in watched] == before
    assert request('/entities') == tree


def verify_copy(result, sources):
    copies = result['duplicatedEntities']
    by_source = {entry['sourceId']: entry for entry in copies}
    assert set(by_source) == {source['id'] for source in sources}
    new_entities = {entry['entity']['id'] for entry in copies}
    source_components = {c['id'] for source in sources for c in source['components']}
    new_components = set()
    assert len(new_entities) == len(sources) and not new_entities.intersection(by_source)
    for source in sources:
        entry = by_source[source['id']]
        copied = entry['entity']
        for field in ['name', 'active', 'static']:
            assert copied[field] == source[field], (field, copied, source)
        parent = source['parent']
        expected_parent = {'id': by_source[parent['id']]['entity']['id']} if parent and parent['id'] in by_source else parent
        assert copied['parent'] == expected_parent
        assert [c['type'] for c in copied['components']] == [c['type'] for c in source['components']]
        for original, copy in zip(source['components'], copied['components']):
            assert copy['properties'] == original['properties'], (original, copy)
            assert entry['componentIds'][original['id']] == copy['id']
            assert copy['id'] not in source_components | new_components
            new_components.add(copy['id'])
    assert result['entity'] == copies[0]['entity']
    return by_source


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
assert 'entity.duplicate' in schema['operations']
assert schema['entity']['duplicate']['assetReferences'] == 'shared'
assert schema['entity']['duplicate']['supportedComponents'] == [
    'Transform', 'ModelRenderer', 'Light', 'Camera', 'Collider', 'RigidBody'
]
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

created = edit([
    {'op': 'entity.create', 'ref': 'parent', 'name': 'Copy parent'},
    {'op': 'entity.create', 'ref': 'root', 'name': 'Copy group', 'parent': {'ref': 'parent'}},
    {'op': 'entity.create', 'ref': 'cube', 'name': 'Copy cube', 'parent': {'ref': 'root'}, 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'ref': 'lamp', 'name': 'Copy lamp', 'parent': {'ref': 'root'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 0, 'y': 3, 'z': 5}}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': 8, 'Range': 20}}]},
])
parent, root, cube, lamp = [created['refs'][name] for name in ['parent', 'root', 'cube', 'lamp']]
sources = [r['entity'] for r in created['results'][1:]]
cube_transform = component(sources[1], 'Transform')['id']
renderer = component(sources[1], 'ModelRenderer')['id']
lamp_light = component(sources[2], 'Light')['id']
temporary = next(e['id'] for e in request('/entities')['entities'] if e['temporary'])
watched = [parent, root, cube, lamp]

for operation, suffix in [
    ({'op': 'entity.duplicate'}, '/target'),
    (duplicate(root, parent=None), '/parent'),
    (duplicate(root, children=False), '/children'),
    ({'op': 'entity.duplicate', 'target': {'ref': 'root'}}, '/target/ref'),
    ({'op': 'entity.duplicate', 'target': None}, '/target'),
    ({'op': 'entity.duplicate', 'target': {}}, '/target/id'),
    (duplicate('invalid'), '/target/id'),
    (duplicate('ffffffffffffffff-ffffffffffffffff'), '/target/id'),
    (duplicate(cube_transform), '/target/id'),
    (duplicate(temporary), '/target/id'),
    (duplicate(root, ref=''), '/ref'),
    (duplicate(root, ref=42), '/ref'),
]:
    reject_unchanged([duplicate(root), operation], watched, 1, suffix)

reject_unchanged([duplicate(root, ref='copy'), duplicate(root, ref='copy')], watched, 1, '/ref')
reject_unchanged([duplicate(root, ref='copy'), {'op': 'entity.create', 'ref': 'copy'}], watched, 1, '/ref')
reject_unchanged([{'op': 'entity.create', 'ref': 'copy'}, duplicate(root, ref='copy')], watched, 1, '/ref')
reject_unchanged([delete(root), duplicate(cube)], watched, 1, '/target/id')
reject_unchanged([{'op': 'entity.create', 'parent': {'ref': 'future'}}, duplicate(root, ref='future')],
                 watched, 0, '/parent/ref')

framed = request('/camera/frame', {'entities': [{'id': root}], 'padding': 3})
before_png = observe(framed['observationToken'], watched, 'before-duplication')
key = uuid.uuid4().hex
operations = [duplicate(root, ref='copy')]
copied = edit(operations, key=key)
mapping = verify_copy(copied['results'][0], sources)
copy_root = copied['refs']['copy']
assert copy_root == mapping[root]['entity']['id']
assert entity(parent)['children'][-1]['id'] == copy_root
assert [c['id'] for c in entity(copy_root)['children']] == [mapping[cube]['entity']['id'], mapping[lamp]['entity']['id']]

# Separate the copy for a visible observation; editing it cannot modify its source.
copy_transform = component(mapping[root]['entity'], 'Transform')['id']
moved = edit([component_update(copy_transform, LocalPosition={'x': 3, 'y': 0, 'z': 0})])
after_png = observe(moved['observationToken'], watched + [copy_root], 'moved-duplicate')
assert before_png != after_png
assert component(entity(root), 'Transform')['data']['LocalPosition'] == {'x': 0.0, 'y': 0.0, 'z': 0.0}
tree = request('/entities')
assert edit(operations, key=key) == copied
assert request('/entities') == tree
edit([duplicate(cube)], expected=409, key=key)

# The snapshot sees preceding properties and component replacements, but not later patches.
ordered = edit([
    update(root, name='Updated group', active=False, static=True),
    component_update(cube_transform, LocalPosition={'x': 1, 'y': 2, 'z': 3}),
    {'op': 'component.remove', 'target': {'id': renderer}},
    {'op': 'component.add', 'target': {'id': cube}, 'type': 'Light', 'properties': {'Intensity': 7}},
    duplicate(root, ref='ordered'),
    update(root, name='Later name', active=True),
    component_update(cube_transform, LocalPosition={'x': 9, 'y': 8, 'z': 7}),
    {'op': 'entity.create', 'ref': 'attached', 'parent': {'ref': 'ordered'}},
])
ordered_nodes = {n['sourceId']: n['entity'] for n in ordered['results'][4]['duplicatedEntities']}
assert ordered_nodes[root]['name'] == 'Updated group'
assert not ordered_nodes[root]['active'] and ordered_nodes[root]['static']
assert component(ordered_nodes[cube], 'Transform')['properties']['LocalPosition'] == {'x': 1, 'y': 2, 'z': 3}
assert component(ordered_nodes[cube], 'Light')['properties']['Intensity'] == 7
assert all(c['type'] != 'ModelRenderer' for c in ordered_nodes[cube]['components'])
copied_cube_entry = next(n for n in ordered['results'][4]['duplicatedEntities'] if n['sourceId'] == cube)
new_light_id = ordered['results'][3]['component']['id']
assert copied_cube_entry['componentIds'][new_light_id] == component(ordered_nodes[cube], 'Light')['id']
assert entity(ordered['refs']['attached'])['parent']['id'] == ordered['refs']['ordered']

# New named/unnamed descendants, reparented members and earlier copies participate in order.
ordered_tree = edit([
    {'op': 'entity.create', 'ref': 'new', 'parent': {'id': root}},
    {'op': 'entity.create', 'parent': {'ref': 'new'}},
    reparent(lamp, None),
    duplicate(cube, ref='copied-child'),
    duplicate(root, ref='copied-tree'),
    reparent(lamp, {'ref': 'copied-tree'}),
])
tree_nodes = ordered_tree['results'][4]['duplicatedEntities']
expected_sources = {root, cube, ordered_tree['refs']['new'], ordered_tree['results'][1]['entity']['id'],
                    ordered_tree['refs']['copied-child']}
assert {n['sourceId'] for n in tree_nodes} == expected_sources
assert len({n['entity']['id'] for n in tree_nodes}) == 5
new_child = ordered_tree['refs']['new']
new_child_copy = next(n['entity']['id'] for n in tree_nodes if n['sourceId'] == new_child)
unnamed_copy = next(n['entity']['id'] for n in tree_nodes if n['sourceId'] == ordered_tree['results'][1]['entity']['id'])
assert [c['id'] for c in entity(new_child_copy)['children']] == [unnamed_copy]
for node in tree_nodes:
    assert set(node['componentIds']) == {c['id'] for c in entity(node['sourceId'])['components']}
    assert set(node['componentIds'].values()) == {c['id'] for c in node['entity']['components']}

after_child_deletion = edit([delete(new_child), duplicate(root)])
assert {n['sourceId'] for n in after_child_deletion['results'][1]['duplicatedEntities']} == {
    root, cube, ordered_tree['refs']['copied-child']}

# A copied root below a deleted parent invalidates its ref, including its copied descendants.
reject_unchanged([duplicate(root, ref='doomed'), delete(parent),
                  {'op': 'entity.create', 'parent': {'ref': 'doomed'}}], [parent, root], 2, '/parent/ref')
deleted = edit([duplicate(root, ref='doomed'), delete(parent), {'op': 'entity.create', 'ref': 'replacement'}])
assert deleted['refs']['doomed'] is None
removed_ids = {e['id'] for e in deleted['results'][1]['removedEntities']}
assert {n['entity']['id'] for n in deleted['results'][0]['duplicatedEntities']} <= removed_ids
assert deleted['refs']['replacement'] not in removed_ids
tree = request('/entities')
assert edit(operations, key=key) == copied
assert request('/entities') == tree
reject_unchanged([duplicate(root)], [deleted['refs']['replacement']], 0, '/target/id')

# Root duplication remains independent when its source is subsequently deleted.
leaf = edit([{'op': 'entity.create', 'name': 'Independent leaf'}])['results'][0]['entity']
independent = edit([duplicate(leaf['id'], ref='independent'), delete(leaf['id'])])
assert independent['results'][0]['entity']['parent'] is None
assert entity(independent['refs']['independent'])['name'] == 'Independent leaf'

print('PASS: duplication, complete ID mappings, ordered snapshots, refs, validation, retries and rendered copies.')
print('Inspect captures in', args.output)

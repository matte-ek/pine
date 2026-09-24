"""Entity reparenting HTTP recipe; run only against a disposable Pine project."""

import argparse
import base64
import json
import math
from pathlib import Path
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19027')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-reparent-results'))
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


def reparent(entity_id, parent):
    return {'op': 'entity.reparent', 'target': {'id': entity_id}, 'parent': parent}


def entity(entity_id):
    return request('/entity?id=' + entity_id)


def vector(x, y, z):
    return {'x': x, 'y': y, 'z': z}


def assert_tree(expected_parents):
    parents = {}

    def visit(node, parent):
        assert node['id'] not in parents, 'Entity appears more than once in the hierarchy'
        parents[node['id']] = parent
        for child in node['children']:
            visit(child, node['id'])

    tree = request('/entities')
    for root in tree['entities']:
        visit(root, None)
    assert len(parents) == tree['count'], tree
    for entity_id, parent in expected_parents.items():
        assert parents[entity_id] == parent, (entity_id, parents, expected_parents)
        actual_parent = entity(entity_id)['parent']
        assert (None if actual_parent is None else actual_parent['id']) == parent


def reject_unchanged(operations, entity_ids, failing_operation, suffix):
    before = [entity(entity_id) for entity_id in entity_ids]
    before_tree = request('/entities')
    result = edit(operations, expected=400)
    assert result['phase'] == 'validation' and result['completed'] == 0, result
    assert result['operation'] == failing_operation, result
    assert result['path'] == '/operations/' + str(failing_operation) + suffix, result
    assert [entity(entity_id) for entity_id in entity_ids] == before
    assert request('/entities') == before_tree


def observe(token, entity_ids, name):
    result = request('/observe', {'after': token, 'entities': entity_ids, 'width': 640})
    assert result['frame']['id'] > token['frame']
    assert result['frame']['revision'] >= token['revision']
    assert result['frame']['sceneGeneration'] == token['sceneGeneration']
    for observed in result['entities']:
        current = entity(observed['id'])
        for field in ['parent', 'components', 'active', 'static']:
            assert observed[field] == current[field], (field, observed, current)
    png = base64.b64decode(result['image']['data'], validate=True)
    (args.output / (name + '.png')).write_bytes(png)
    return png


def bounds(entity_id, center):
    result = request('/camera/frame', {'entities': [{'id': entity_id}], 'includeChildren': False})
    box = result['framedBounds']
    for axis, expected in zip('xyz', center):
        actual = (box['min'][axis] + box['max'][axis]) / 2
        assert math.isclose(actual, expected, abs_tol=1e-5), (box, center)
    return [(box['max'][axis] - box['min'][axis]) / 2 for axis in 'xyz']


session = request('/requests')['session']
schema = request('/edit/schema')
assert 'entity.reparent' in schema['operations']
assert schema['entity']['reparent']['transformPreservation'] == 'local'
assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'

quarter_turn = math.sqrt(.5)
created = edit([
    {'op': 'entity.create', 'ref': 'left', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(-3, 0, 0)}}]},
    {'op': 'entity.create', 'ref': 'right', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(3, 0, 0),
         'LocalScale': vector(2, 3, 1),
         'LocalRotation': {'x': 0, 'y': 0, 'z': quarter_turn, 'w': quarter_turn}}}]},
    {'op': 'entity.create', 'ref': 'group', 'parent': {'ref': 'left'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(1, 0, 0),
         'LocalRotation': {'x': quarter_turn, 'y': 0, 'z': 0, 'w': quarter_turn}}}]},
    {'op': 'entity.create', 'ref': 'cube', 'parent': {'ref': 'group'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(0, 1, 0), 'LocalScale': vector(1, 2, 3)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'ref': 'lamp', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(0, 6, 8)}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': 6, 'Range': 40}}]}
])
left, right, group, cube, lamp = [created['refs'][name] for name in ['left', 'right', 'group', 'cube', 'lamp']]
entity_ids = [left, right, group, cube, lamp]
group_transform = created['results'][2]['entity']['components'][0]['id']
edit([{'op': 'entity.update', 'target': {'id': target}, 'properties': properties}
      for target, properties in [(group, {'static': True}), (cube, {'static': True}), (right, {'active': False})]])
original_components = {target: entity(target)['components'] for target in [group, cube]}

# Local values and identities survive attach/detach, including static descendants.
# A child sits in its parent's space: scaled, rotated, then translated. Rotation order is parent * local.
original_half = bounds(cube, (-2, 0, 1))
wide_view = request('/camera/frame', {'entities': [{'id': left}, {'id': right}], 'padding': 3})
camera_state = request('/camera')['state']
before = observe(wide_view['observationToken'], entity_ids, 'before')
moved = edit([reparent(group, {'id': right})])
assert moved['results'][0]['entity']['parent'] == {'id': right}
after = observe(moved['observationToken'], entity_ids, 'reparented')
assert after != before
assert_tree({left: None, right: None, group: right, cube: group})
moved_half = bounds(cube, (3, 2, 3))
expected_half = [original_half[1], original_half[0] * 2, original_half[2] * 3]
assert all(math.isclose(a, b, abs_tol=1e-5) for a, b in zip(moved_half, expected_half))
for target in [group, cube]:
    assert entity(target)['components'] == original_components[target]

# Assigning the same parent never duplicates or reorders child entries.
tree_before = request('/entities')
edit([reparent(group, {'id': right}), reparent(group, {'id': right})])
assert request('/entities') == tree_before
request('/camera', camera_state)
detached = edit([reparent(group, None)])
detached_png = observe(detached['observationToken'], entity_ids, 'detached')
assert detached_png != after
assert_tree({left: None, right: None, group: None, cube: group})
bounds(cube, (1, 0, 1))
for target in [group, cube]:
    assert entity(target)['components'] == original_components[target]
root_tree = request('/entities')
edit([reparent(group, None)])
assert request('/entities') == root_tree

# Cycle checks use each intermediate hierarchy, including a creation under the target.
reject_unchanged([reparent(group, {'id': group})], entity_ids, 0, '/parent')
reject_unchanged([reparent(group, {'id': cube})], entity_ids, 0, '/parent')
reject_unchanged([reparent(group, {'id': left}), reparent(left, {'id': cube})], entity_ids, 1, '/parent')
reject_unchanged([
    {'op': 'entity.create', 'ref': 'new-child', 'parent': {'id': group}},
    {'op': 'entity.create', 'ref': 'new-grandchild', 'parent': {'ref': 'new-child'}},
    reparent(group, {'ref': 'new-grandchild'})
], entity_ids, 2, '/parent')
reject_unchanged([reparent(group, {'id': cube}), reparent(cube, None)], entity_ids, 0, '/parent')

# Detaching first can make a previously cyclic move valid. Repeated moves compose.
ordered = edit([reparent(cube, None), reparent(group, {'id': cube}),
                reparent(group, {'id': left}), reparent(cube, {'id': group})])
assert [r['entity']['parent'] for r in ordered['results']] == [None, {'id': cube}, {'id': left}, {'id': group}]
assert_tree({group: left, cube: group})
new_parent = edit([{'op': 'entity.create', 'ref': 'new-parent'}, reparent(group, {'ref': 'new-parent'})])
assert_tree({group: new_parent['refs']['new-parent'], cube: group})
entity_ids.append(new_parent['refs']['new-parent'])

# Strict envelopes, existing-only targets, prior-only parent refs, and editor protection.
temporary = next(e for e in request('/entities')['entities'] if e['temporary'])['id']
invalid_operations = [
    ({'op': 'entity.reparent', 'target': {'id': group}}, '/parent'),
    ({**reparent(group, None), 'preserve': 'world'}, '/preserve'),
    ({**reparent(group, None), 'target': {'ref': 'new-parent'}}, '/target/ref'),
    (reparent(group_transform, None), '/target/id'),
    (reparent('invalid', None), '/target/id'),
    (reparent('ffffffffffffffff-ffffffffffffffff', None), '/target/id'),
    (reparent(temporary, None), '/target/id'),
    (reparent(group, {'id': temporary}), '/parent/id'),
    (reparent(group, {'id': group_transform}), '/parent/id'),
    (reparent(group, {'id': 'ffffffffffffffff-ffffffffffffffff'}), '/parent/id'),
    (reparent(group, {'id': 'invalid'}), '/parent/id'),
    (reparent(group, {'ref': 'later'}), '/parent/ref'),
    (reparent(group, {'ref': 'new-parent'}), '/parent/ref'),
    (reparent(group, {}), '/parent'),
    (reparent(group, {'id': left, 'ref': 'left'}), '/parent'),
    (reparent(group, {'extra': 1}), '/parent/extra'),
    (reparent(group, 'root'), '/parent'),
    (reparent(group, []), '/parent')
]
for invalid, suffix in invalid_operations:
    reject_unchanged([reparent(group, {'id': right}), invalid,
                      {'op': 'entity.create', 'ref': 'later'}], entity_ids + [temporary], 1, suffix)

# Component patches interleaved with moves retain the latest local values.
patched = edit([
    {'op': 'component.update', 'target': {'id': group_transform},
     'properties': {'LocalPosition': vector(2, 0, 0)}},
    reparent(group, {'id': left}),
    {'op': 'component.update', 'target': {'id': group_transform},
     'properties': {'LocalScale': vector(1, 1, 1)}},
    reparent(group, {'id': right})
])
assert patched['results'][1]['entity']['components'][0]['properties']['LocalPosition'] == vector(2, 0, 0)
bounds(cube, (3, 4, 3))

# Identified retries must not move the entity back after another edit.
key = uuid.uuid4().hex
operations = [reparent(group, {'id': left})]
first = edit(operations, key=key)
edit([reparent(group, {'id': right})])
assert edit(operations, key=key) == first
edit([reparent(group, None)], expected=409, key=key)
assert_tree({group: right, cube: group})

print('PASS: reparent/detach, local transforms, ordered cycles, refs, validation, retries and rendered descendants.')
print('Inspect captures in', args.output)

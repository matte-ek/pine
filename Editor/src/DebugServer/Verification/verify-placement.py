"""Surface-placement HTTP recipe; run against a disposable project with an active Level viewport."""

import argparse
import base64
import itertools
import json
import math
from pathlib import Path
import tempfile
import urllib.error
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19041')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-placement-results'))
parser.add_argument('--terrain', action='store_true', help='Also use the verify-spatial.py terrain fixture')
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


def place(entity_id, point, normal, **fields):
    return {'op': 'entity.place', 'target': {'id': entity_id},
            'surface': {'point': point, 'normal': normal}, 'anchor': {'type': 'modelBounds'}, **fields}


def relative_place(entity_id, reference_id, axes, offset=None):
    operation = {'op': 'entity.place', 'target': {'id': entity_id}, 'relativeTo': {'id': reference_id},
                 'boundsAlignment': {axis: {'target': target, 'reference': reference}
                                     for axis, (target, reference) in axes.items()}}
    if offset is not None:
        operation['offset'] = offset
    return operation


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


def check_contact(entity_id, local_min, local_max, point, normal, clearance):
    world = spatial(entity_id)['worldTransform']
    n = xyz(normal)
    length = math.sqrt(dot(n, n))
    n = [a / length for a in n]
    distances = []
    for corner in itertools.product(*zip(local_min, local_max)):
        scaled = [a*b for a, b in zip(corner, xyz(world['scale']))]
        transformed = add(xyz(world['position']), rotate(world['rotation'], scaled))
        distances.append(dot([a-b for a, b in zip(transformed, xyz(point))], n))
    assert abs(min(distances) - clearance) < 2e-4, distances
    center = [(a+b)/2 for a, b in zip(local_min, local_max)]
    center = add(xyz(world['position']), rotate(world['rotation'], [a*b for a, b in zip(center, xyz(world['scale']))]))
    offset = [a-b for a, b in zip(center, xyz(point))]
    close([a-dot(offset, n)*b for a, b in zip(offset, n)], [0, 0, 0])


schema = request('/edit/schema')
assert 'entity.place' in schema['operations']
fields = schema['operationSchemas']['entity.place']['fields']
assert fields['alignment']['whenOmitted'] == 'preserve world rotation'
assert fields['clearance']['minimum'] == 0 and fields['clearance']['default'] == 0
assert fields['target']['forms'] == ['id']
assert fields['relativeTo']['forms'] == ['id']
assert fields['boundsAlignment']['space'] == 'world'
assert fields['boundsAlignment']['minProperties'] == 1
assert fields['offset']['fields']['space']['values'] == ['world', 'referenceLocal']
assert schema['operationSchemas']['entity.place']['oneOf'] == [
    {'required': ['surface', 'anchor'], 'forbidden': ['relativeTo', 'boundsAlignment', 'offset']},
    {'required': ['relativeTo', 'boundsAlignment'], 'forbidden': ['surface', 'anchor', 'clearance', 'alignment']},
]

# Remove earlier recipe props/lights so captures show only this workflow. Keep the terrain fixture.
removals = []
for root in request('/entities')['entities']:
    if root['temporary']:
        continue
    if any(c['type'] == 'TerrainRenderer' for c in entity(root['id'])['components']):
        continue
    removals.append({'op': 'entity.delete', 'target': {'id': root['id']}})
if removals:
    edit(removals)

# Import a real, visibly renderable box whose pivot is outside its geometry.
source = Path(tempfile.mkdtemp(prefix='pine-placement-source.')) / 'offset-box.obj'
vertices = list(itertools.product((2, 4), (-3, -1), (4, 6)))
faces = [(3, 7, 5, 1), (6, 8, 4, 2), (5, 6, 2, 1), (4, 8, 7, 3), (2, 4, 3, 1), (7, 8, 6, 5)]
source.with_suffix('.mtl').write_text('newmtl PlacementMaterial\nKd 0.65 0.4 0.15\n')
source.write_text('\n'.join(['mtllib ' + str(source.with_suffix('.mtl')), 'o OffsetBox', 'usemtl PlacementMaterial'] + ['v %s %s %s' % p for p in vertices] +
                           ['vt 0 0', 'vt 1 0', 'vt 1 1', 'vt 0 1'] +
                           ['f ' + ' '.join('%d/%d' % (vertex, uv)
                                            for uv, vertex in enumerate(face, 1)) for face in faces]) + '\n')
imported = request('/assets/import', {'source': str(source), 'directory': 'placement-' + uuid.uuid4().hex})
model = next(item for item in imported['imports'] if item['type'] == 'Model')
angle = math.radians(35) / 2
sun_x, sun_y = math.radians(-50) / 2, math.radians(30) / 2
created = edit([
    {'op': 'entity.create', 'ref': 'parent', 'name': 'Placement parent', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(20, 7, -4), 'LocalScale': vec(-2, 3, .5),
         'LocalRotation': {'x': 0, 'y': 0, 'z': math.sqrt(.5), 'w': math.sqrt(.5)}}}]},
    {'op': 'entity.create', 'ref': 'crate', 'name': 'Placement offset crate', 'components': [
        {'type': 'Transform', 'properties': {'LocalScale': vec(.2, .2, .2)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'id': model['id']}}}]},
    {'op': 'entity.create', 'ref': 'wall', 'name': 'Placement wall', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(40, 4, 20), 'LocalScale': vec(4, 3, .2),
         'LocalRotation': {'x': 0, 'y': math.sin(angle), 'z': 0, 'w': math.cos(angle)}}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
    {'op': 'entity.create', 'ref': 'lamp', 'name': 'Placement lamp', 'parent': {'ref': 'parent'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalScale': vec(.2, .1, .3)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': .1, 'Range': 15}}]},
    {'op': 'entity.create', 'ref': 'empty', 'name': 'Placement empty'},
    {'op': 'entity.create', 'name': 'Placement inspection sun', 'components': [
        {'type': 'Transform', 'properties': {'LocalRotation': {
            'x': math.sin(sun_x) * math.cos(sun_y), 'y': math.cos(sun_x) * math.sin(sun_y),
            'z': -math.sin(sun_x) * math.sin(sun_y), 'w': math.cos(sun_x) * math.cos(sun_y)}}},
        {'type': 'Light', 'properties': {'Type': 'Directional', 'Intensity': 1, 'CastShadows': False}}]}
])
parent, crate, wall, lamp, empty = [created['refs'][name] for name in ['parent', 'crate', 'wall', 'lamp', 'empty']]
watched = [parent, crate, wall, lamp, empty]
close(xyz(spatial(crate)['bounds']['center']), [.6, -.4, 1])

# Use a fresh terrain ray hit when run by the spatial harness. Otherwise exercise a supplied slope.
point, normal = vec(10.25, 18, 24.25), vec(1, 1, 0)
if args.terrain:
    hit = request('/spatial/raycast', {'origin': vec(10.25, 40, 24.25), 'direction': vec(0, -1, 0),
                                      'maxDistance': 60, 'exclude': [{'id': crate}]})['hit']
    assert hit['geometry'] == 'terrain-surface'
    point, normal = hit['position'], hit['normal']
    assert abs(normal['x']) > .1
before = component(crate, 'Transform')['properties']
placed = edit([place(crate, point, normal, clearance=.03)])
check_contact(crate, (2, -3, 4), (4, -1, 6), point, normal, .03)
after = component(crate, 'Transform')['properties']
assert before['LocalRotation'] == after['LocalRotation'] and before['LocalScale'] == after['LocalScale']
request('/camera/frame', {'entities': [{'id': crate}], 'padding': 3,
                        'direction': vec(-normal['x'], -normal['y'], -normal['z'] - .5)})
placed_image = capture(placed['observationToken'], [crate], 'crate-placed')
undone = request('/history/undo', {})
assert component(crate, 'Transform')['properties'] == before
assert capture(undone['observationToken'], [crate], 'crate-undone') != placed_image
redone = request('/history/redo', {})
assert component(crate, 'Transform')['properties'] == after
capture(redone['observationToken'], [crate], 'crate-redone')

# Mount a parented, mirrored lamp against a rotated wall using a real surface ray.
wall_world = spatial(wall)['worldTransform']
outward = rotate(wall_world['rotation'], [0, 0, 1])
origin = add(xyz(wall_world['position']), [8*a for a in outward])
hit = request('/spatial/raycast', {'origin': vec(*origin), 'direction': vec(*[-a for a in outward]),
                                  'maxDistance': 10, 'exclude': [{'id': parent}]})['hit']
assert hit['entity'] == wall
mount = place(lamp, hit['position'], hit['normal'], clearance=.05,
              alignment={'axis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0)})
lamp_before = component(lamp, 'Transform')['properties']
mounted = edit([update(parent, {'LocalPosition': vec(22, 9, -6)}), mount])
check_contact(lamp, (-1, -1, -1), (1, 1, 1), hit['position'], hit['normal'], .05)
close(xyz(spatial(lamp)['forward']), xyz(hit['normal']))
close(xyz(spatial(lamp)['up']), [0, 1, 0])
lamp_after = component(lamp, 'Transform')['properties']
request('/camera/frame', {'entities': [{'id': wall}, {'id': lamp}], 'padding': 1.4,
                        'direction': vec(-outward[0] - .6, -.2, -outward[2] + .3)})
capture(mounted['observationToken'], [lamp, wall], 'lamp-mounted')
request('/history/undo', {})
assert component(parent, 'Transform')['properties']['LocalPosition'] == vec(20, 7, -4)
assert component(lamp, 'Transform')['properties'] == lamp_before
redone = request('/history/redo', {})
assert component(lamp, 'Transform')['properties'] == lamp_after
capture(redone['observationToken'], [lamp, wall], 'lamp-redone')

# Sequential planning sees new parents, model assignment, partial patches and repeated placements.
composed = edit([
    {'op': 'entity.create', 'ref': 'new-parent', 'components': [{'type': 'Transform', 'properties': {
        'LocalPosition': vec(-20, 3, 7), 'LocalScale': vec(2, 1, 3)}}]},
    {'op': 'entity.reparent', 'target': {'id': empty}, 'parent': {'ref': 'new-parent'}},
    {'op': 'component.add', 'target': {'id': empty}, 'type': 'ModelRenderer',
     'properties': {'Model': {'id': model['id']}, 'MeshIndex': 0}},
    update(empty, {'LocalScale': vec(-.2, .5, 0)}),
    place(empty, vec(4, 5, 6), vec(0, 2, 0)),
    update(empty, {'LocalRotation': {'x': 0, 'y': 0, 'z': 0, 'w': 1}}),
    place(empty, vec(7, 8, 9), vec(0, 5, 0), clearance=.2),
    {'op': 'entity.duplicate', 'target': {'id': empty}, 'ref': 'placed-copy'}
])
check_contact(empty, (2, -3, 4), (4, -1, 6), vec(7, 8, 9), vec(0, 1, 0), .2)
assert component(empty, 'Transform')['properties'] == component(composed['refs']['placed-copy'], 'Transform')['properties']
# A partial patch after placement must retain its position, including in a batch with duplication.
assert composed['results'][4]['entity']['components'][0]['properties']['LocalPosition'] == composed['results'][5]['component']['properties']['LocalPosition']

# Local anchors work without geometry, and include scale/rotation in their world offset.
local_anchor = vec(2, -1, .5)
local_operation = place(parent, vec(-5, 6, 3), vec(0, 0, 3), clearance=.4,
                        anchor={'type': 'localPoint', 'point': local_anchor})
edit([local_operation])
world = spatial(parent)['worldTransform']
contact = add(xyz(world['position']), rotate(world['rotation'], [a*b for a, b in zip(xyz(local_anchor), xyz(world['scale']))]))
close(contact, [-5, 6, 3.4])

# Invalid late placement leaves all scene state, history and dirty tracking untouched.
invalid = [
    {**mount, 'unexpected': True}, {**mount, 'target': {'ref': 'lamp'}},
    {**mount, 'target': {'id': component(lamp, 'Transform')['id']}},
    {**mount, 'target': {'id': 'ffffffffffffffff-ffffffffffffffff'}},
    {**mount, 'clearance': -1}, {**mount, 'clearance': True}, {**mount, 'clearance': 1e13},
    {**mount, 'surface': {'point': hit['position'], 'normal': vec(0, 0, 0)}},
    {**mount, 'surface': {'point': vec(1e13, 0, 0), 'normal': vec(0, 1, 0)}},
    {**mount, 'anchor': {'type': 'localPoint'}},
    {**mount, 'anchor': {'type': 'modelBounds', 'point': vec(0, 0, 0)}},
    {**mount, 'anchor': {'type': 'pivot'}},
    {**mount, 'alignment': {'axis': '-Z', 'upAxis': '+Z', 'up': vec(0, 1, 0)}},
    {**mount, 'alignment': {'axis': '-Z', 'upAxis': '+Y', 'up': hit['normal']}},
    {**mount, 'alignment': {'axis': 'z', 'upAxis': '+Y', 'up': vec(0, 1, 0)}},
    place(parent, vec(0, 0, 0), vec(0, 1, 0)),
]
temporary = next(item['id'] for item in request('/entities')['entities'] if item['temporary'])
invalid.append({**local_operation, 'target': {'id': temporary}})
for operation in invalid:
    states = [entity(item) for item in watched]
    history, level, tree = request('/history'), request('/level/status'), request('/entities')
    rejected = edit([update(crate, {'LocalPosition': vec(99, 99, 99)}), operation], expected=400)
    assert rejected['phase'] == 'validation' and rejected['completed'] == 0 and rejected['operation'] == 1
    assert [entity(item) for item in watched] == states
    assert request('/history') == history and request('/level/status') == level and request('/entities') == tree

# Geometry removed earlier in the same batch and deleted targets fail before mutation.
for operations in [
    [update(empty, {'Model': None, 'MeshIndex': -1}, 'ModelRenderer'), place(empty, vec(0, 0, 0), vec(0, 1, 0))],
    [{'op': 'entity.delete', 'target': {'id': empty}}, place(empty, vec(0, 0, 0), vec(0, 1, 0))],
]:
    state, history = entity(empty), request('/history')
    assert edit(operations, expected=400)['completed'] == 0
    assert entity(empty) == state and request('/history') == history

# Retrying an identified placement returns its original result without replacing a later edit.
headers = {'X-Pine-Session': request('/requests')['session'], 'Idempotency-Key': uuid.uuid4().hex}
first = edit([mount], headers=headers)
edit([update(lamp, {'LocalPosition': vec(1, 2, 3)})])
assert edit([mount], headers=headers) == first
assert component(lamp, 'Transform')['properties']['LocalPosition'] == vec(1, 2, 3)
edit([mount])

# Align two off-center models under different rotated, mirrored/scaled parents.
relative_created = edit([
    {'op': 'entity.create', 'ref': 'relative-parent', 'name': 'Relative parent', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(-10, 2, 6), 'LocalScale': vec(-2, 1.5, .5),
         'LocalRotation': {'x': 0, 'y': math.sin(angle), 'z': 0, 'w': math.cos(angle)}}}]},
    {'op': 'entity.create', 'ref': 'reference', 'name': 'Relative reference', 'parent': {'id': parent}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(4, 5, 6), 'LocalScale': vec(.3, .2, .5)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'id': model['id']}}}]},
    {'op': 'entity.create', 'ref': 'target', 'name': 'Relative target', 'parent': {'ref': 'relative-parent'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalScale': vec(.2, .3, .4)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'id': model['id']}, 'MeshIndex': 0}}]},
])
relative_parent, reference, target = [relative_created['refs'][name] for name in ['relative-parent', 'reference', 'target']]
watched += [relative_parent, reference, target]
side_by_side = relative_place(target, reference, {'x': ('min', 'max'), 'y': ('min', 'min'), 'z': ('center', 'center')},
                             {'space': 'world', 'value': vec(.1, 0, 0)})


def check_relative(operation):
    target_bounds = spatial(operation['target']['id'])['bounds']
    reference_state = spatial(operation['relativeTo']['id'])
    offset = operation.get('offset', {'space': 'world', 'value': vec(0, 0, 0)})
    translation = xyz(offset['value'])
    if offset['space'] == 'referenceLocal':
        translation = rotate(reference_state['worldTransform']['rotation'], translation)
    for axis, anchors in operation['boundsAlignment'].items():
        actual = target_bounds[anchors['target']][axis] - reference_state['bounds'][anchors['reference']][axis]
        close([actual], [translation['xyz'.index(axis)]])


before_relative = component(target, 'Transform')['properties']
aligned = edit([side_by_side])
check_relative(side_by_side)
after_relative = component(target, 'Transform')['properties']
assert before_relative['LocalRotation'] == after_relative['LocalRotation']
assert before_relative['LocalScale'] == after_relative['LocalScale']
request('/camera/frame', {'entities': [{'id': target}, {'id': reference}], 'padding': 1.5,
                        'direction': vec(.5, -.3, -1)})
capture(aligned['observationToken'], [target, reference], 'bounds-aligned')
request('/history/undo', {})
assert component(target, 'Transform')['properties'] == before_relative
redone = request('/history/redo', {})
assert component(target, 'Transform')['properties'] == after_relative
capture(redone['observationToken'], [target, reference], 'bounds-redone')

# Every anchor pair works on every axis. Unselected world coordinates remain unchanged.
for axis, target_anchor, reference_anchor in itertools.product('xyz', ['min', 'center', 'max'], ['min', 'center', 'max']):
    previous = spatial(target)['worldTransform']['position']
    operation = relative_place(target, reference, {axis: (target_anchor, reference_anchor)})
    edit([operation])
    check_relative(operation)
    current = spatial(target)['worldTransform']['position']
    close([current[a] for a in 'xyz' if a != axis], [previous[a] for a in 'xyz' if a != axis])

# Reference-local offsets rotate without inheriting the reference's mirrored/nonuniform scale.
local_offset = {**side_by_side, 'offset': {'space': 'referenceLocal', 'value': vec(.3, -.2, .4)}}
edit([local_offset])
check_relative(local_offset)
# Offsets also translate unselected axes after alignment.
previous = spatial(target)['worldTransform']['position']
edit([relative_place(target, reference, {'x': ('min', 'max')}, {'space': 'world', 'value': vec(0, .2, -.3)})])
current = spatial(target)['worldTransform']['position']
close([current['y'], current['z']], [previous['y'] + .2, previous['z'] - .3])

# Earlier edits to either hierarchy/model and repeated placements feed subsequent planning.
composed = edit([
    {'op': 'entity.create', 'ref': 'reference-parent', 'components': [{'type': 'Transform', 'properties': {
        'LocalPosition': vec(15, 3, -4), 'LocalScale': vec(.5, -2, 3)}}]},
    {'op': 'entity.reparent', 'target': {'id': reference}, 'parent': {'ref': 'reference-parent'}},
    update(reference, {'Model': {'path': 'engine/primitive/cube'}, 'MeshIndex': -1}, 'ModelRenderer'),
    update(relative_parent, {'LocalPosition': vec(10, 4, 1)}),
    place(reference, vec(20, 5, 10), vec(0, 1, 0)),
    side_by_side,
    update(target, {'LocalScale': vec(0, -.3, .4)}),
    local_offset,
    update(target, {'LocalRotation': after_relative['LocalRotation']}),
    {'op': 'entity.duplicate', 'target': {'id': target}, 'ref': 'relative-copy'},
])
check_relative(local_offset)
assert component(target, 'Transform')['properties'] == component(composed['refs']['relative-copy'], 'Transform')['properties']
request('/history/undo', {})
assert component(reference, 'ModelRenderer')['properties']['Model'] == {'id': model['id']}
request('/history/redo', {})
check_relative(local_offset)
request('/history/undo', {})

# Strict mode/axis/offset/reference validation and invalid proposed references cannot partially edit.
invalid_relative = [
    {**side_by_side, 'surface': mount['surface']},
    {**side_by_side, 'anchor': {'type': 'modelBounds'}},
    {**side_by_side, 'clearance': 0}, {**side_by_side, 'alignment': mount['alignment']},
    {key: value for key, value in side_by_side.items() if key != 'boundsAlignment'},
    {**side_by_side, 'boundsAlignment': {}}, {**side_by_side, 'boundsAlignment': {'X': {'target': 'min', 'reference': 'max'}}},
    {**side_by_side, 'boundsAlignment': {'x': {'target': 'minimum', 'reference': 'max'}}},
    {**side_by_side, 'boundsAlignment': {'x': {'target': 'min'}}},
    {**side_by_side, 'offset': {'space': 'local', 'value': vec(0, 0, 0)}},
    {**side_by_side, 'offset': {'space': 'world', 'value': vec(True, 0, 0)}},
    {**side_by_side, 'offset': {'space': 'world', 'value': vec(1e13, 0, 0)}},
    {**side_by_side, 'offset': {'value': vec(0, 0, 0)}},
    {**side_by_side, 'relativeTo': {'ref': 'reference'}},
    {**side_by_side, 'relativeTo': {'id': target}},
    {**side_by_side, 'relativeTo': {'id': temporary}},
    {**side_by_side, 'relativeTo': {'id': component(reference, 'Transform')['id']}},
    {**side_by_side, 'relativeTo': {'id': 'ffffffffffffffff-ffffffffffffffff'}},
    {**side_by_side, 'relativeTo': {'id': parent}},  # Parent has no own model geometry.
    {**mount, 'offset': {'space': 'world', 'value': vec(0, 0, 0)}},
    {'op': 'entity.place', 'target': {'id': target}},
]
for operation in invalid_relative:
    states = [entity(item) for item in watched]
    history, level, tree = request('/history'), request('/level/status'), request('/entities')
    rejected = edit([update(crate, {'LocalPosition': vec(99, 99, 99)}), operation], expected=400)
    assert rejected['phase'] == 'validation' and rejected['completed'] == 0 and rejected['operation'] == 1
    assert [entity(item) for item in watched] == states
    assert request('/history') == history and request('/level/status') == level and request('/entities') == tree

for preceding in [
    {'op': 'entity.delete', 'target': {'id': reference}},
    {'op': 'entity.delete', 'target': {'id': parent}},
    {'op': 'entity.reparent', 'target': {'id': reference}, 'parent': {'id': target}},
    update(reference, {'Model': None, 'MeshIndex': -1}, 'ModelRenderer'),
    update(target, {'Model': None, 'MeshIndex': -1}, 'ModelRenderer'),
]:
    states = [entity(item) for item in watched]
    history, level, tree = request('/history'), request('/level/status'), request('/entities')
    assert edit([preceding, side_by_side], expected=400)['completed'] == 0
    assert [entity(item) for item in watched] == states
    assert request('/history') == history and request('/level/status') == level and request('/entities') == tree

# Resolve from an ancestor reference too; moving its child does not move the reference.
edit([{'op': 'entity.reparent', 'target': {'id': target}, 'parent': {'id': reference}}, side_by_side])
check_relative(side_by_side)
request('/history/undo', {})
headers = {'X-Pine-Session': request('/requests')['session'], 'Idempotency-Key': uuid.uuid4().hex}
first = edit([side_by_side], headers=headers)
edit([update(target, {'LocalPosition': vec(1, 2, 3)})])
assert edit([side_by_side], headers=headers) == first
assert component(target, 'Transform')['properties']['LocalPosition'] == vec(1, 2, 3)
edit([side_by_side])
check_relative(side_by_side)

# Persistence stores the calculated local transform using the existing Level format.
saved_state = {item: component(item, 'Transform')['properties'] for item in [crate, lamp, target, reference]}
saved = request('/level/save-as', {'path': 'verification/placement-' + uuid.uuid4().hex})
assert saved['fileWritten'] and not saved['unsavedChanges']
request('/level/load', {'path': saved['path']})

def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node['children'])

by_name = {node['name']: node['id'] for node in walk(request('/entities')['entities'])}
for previous, name in [(crate, 'Placement offset crate'), (lamp, 'Placement lamp'),
                       (target, 'Relative target'), (reference, 'Relative reference')]:
    assert component(by_name[name], 'Transform')['properties'] == saved_state[previous]
request('/camera/frame', {'entities': [{'id': by_name['Placement wall']}, {'id': by_name['Placement lamp']}],
                        'padding': 1.4, 'direction': vec(-outward[0] - .6, -.2, -outward[2] + .3)})
capture(request('/level/save', {})['observationToken'], [by_name['Placement lamp']], 'reloaded')
reloaded_relative = {**side_by_side, 'target': {'id': by_name['Relative target']},
                     'relativeTo': {'id': by_name['Relative reference']}}
check_relative(reloaded_relative)
framed = request('/camera/frame', {'entities': [reloaded_relative['target'], reloaded_relative['relativeTo']],
                                  'padding': 1.5, 'direction': vec(.5, -.3, -1)})
capture(framed['observationToken'], [by_name['Relative target'], by_name['Relative reference']], 'bounds-reloaded')
edit([side_by_side], expected=400)
edit([mount], expected=400)
print('PASS: surface and relative bounds placement, off-center pivots, parent transforms, offsets, anchors, sequential batches, validation, undo/redo, retries, captures and save/reload.')
print('Inspect captures in', args.output)

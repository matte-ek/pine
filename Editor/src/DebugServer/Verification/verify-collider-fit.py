"""Check entity.fitCollider and batch refs as placement targets; needs an Editor build and Xvfb."""

import argparse
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19053)
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-collider-fit.'))
url = 'http://127.0.0.1:%d' % args.port
print('Collider fit verification:', root, flush=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Timestamps are
# preserved so the engine assets are not all re-imported on boot.
data = root / 'data'
(data / 'projects/colliders/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

# Every stock model is centred on its origin, which would leave the fitted Position at zero and
# prove nothing about the rotation and scale in that term. This box spans x 2..4, y 0..1, z -1..1,
# so its centre is (3, 0.5, 0) and half-extents are (1, 0.5, 1).
source = root / 'offcentre.obj'
source.write_text('\n'.join([
    # A named material is not decoration: Pine's model importer indexes its embedded-material list
    # whenever the scene has any material, and that list skips assimp's default one - so a
    # material-less model reads past the end of an empty vector and takes the Editor down.
    'mtllib offcentre.mtl', 'usemtl crate',
    'v 2 0 -1', 'v 4 0 -1', 'v 4 1 -1', 'v 2 1 -1',
    'v 2 0 1', 'v 4 0 1', 'v 4 1 1', 'v 2 1 1',
    'f 1 2 3 4', 'f 5 6 7 8', 'f 1 2 6 5', 'f 2 3 7 6', 'f 3 4 8 7', 'f 4 1 5 8',
]) + '\n')
(root / 'offcentre.mtl').write_text('newmtl crate\nKd 0.8 0.6 0.4\n')

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'editor.log'
log = log_path.open('w')
process = subprocess.Popen(['xvfb-run', '-a', str(build / 'Editor/Editor'), 'colliders'], cwd=data,
                           env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)


def shutdown():
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def fetch(path, body=None, expected=200):
    payload = None if body is None else json.dumps(body).encode()
    message = urllib.request.Request(url + path, data=payload,
                                     headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(message, timeout=120)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.code == expected, (path, response.code, body[:1500])
        return json.loads(body)


def edit(operations, expected=200):
    return fetch('/edit', {'version': 1, 'operations': operations}, expected)


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def near(actual, expected, tolerance=1e-5):
    if isinstance(expected, dict):
        return all(near(actual[key], expected[key], tolerance) for key in expected)
    return abs(actual - expected) < tolerance


def fit_matches_geometry(entity_id, box, padding=0.0, axis_aligned=True, tolerance=1e-4):
    """Check the fitted box against what the scene says the model actually occupies.

    The centre always has to agree: Position is an unscaled, unrotated offset from the entity's
    world position, and the box's world centre is the model's world bounds centre whatever the
    entity's orientation. The extents can only be compared directly when the entity is
    axis-aligned - for a rotated prop the world bounds are the AABB *around* the tilted box, which
    is larger, and being smaller than it is the whole reason this operation exists.
    """
    measured = fetch('/spatial/query', {'entities': [{'id': entity_id}]})['entities'][0]
    origin = measured['worldTransform']['position']
    centre = {axis: origin[axis] + box['Position'][axis] for axis in 'xyz'}

    if not near(centre, measured['bounds']['center'], tolerance):
        return False

    if not axis_aligned:
        return True

    # Half-extents are multiplied by the entity's world scale in the engine, padding included.
    scale = measured['worldTransform']['scale']
    return all(abs((box['Size'][axis] - padding) * 2 * abs(scale[axis])
                   - measured['bounds']['dimensions'][axis]) < tolerance for axis in 'xyz')


def collider_of(entity_id):
    entity = fetch('/entity?id=' + entity_id)
    return next(c['properties'] for c in entity['components'] if c['type'] == 'Collider')


def prop(model, **transform):
    """An entity carrying the model and a default Box collider, plus its returned ids."""
    properties = {'LocalPosition': vec(0, 0, 0), 'LocalScale': vec(1, 1, 1)}
    properties.update(transform)
    result = edit([{'op': 'entity.create', 'name': 'Prop', 'components': [
        {'type': 'Transform', 'properties': properties},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': model}}},
        {'type': 'Collider', 'properties': {}}]}])['results'][0]
    return result['entity']['id']


try:
    deadline = time.time() + 120
    while True:
        try:
            status = fetch('/status')
            break
        except Exception:
            assert process.poll() is None, 'Editor exited early; see ' + str(log_path)
            assert time.time() < deadline, 'Editor did not start serving in time; see ' + str(log_path)
            time.sleep(0.5)
    assert status['playState'] == 'stopped', status

    imported = fetch('/assets/import', {'source': str(source), 'directory': 'models'})
    offcentre = imported['imports'][0]['path']
    bounds = fetch('/assets/summary', {'assets': [{'path': offcentre}]})['assets'][0]['bounds']
    assert near(bounds['min'], vec(2, 0, -1)) and near(bounds['max'], vec(4, 1, 1)), bounds

    schema = fetch('/edit/schema')
    assert 'entity.fitCollider' in schema['operations'], schema['operations']
    assert schema['operationSchemas']['entity.fitCollider']['fields']['target']['forms'] == ['id', 'ref']

    # ------------------------------------------------------------------ the basic fit

    cube = prop('engine/primitive/cube')
    fitted = edit([{'op': 'entity.fitCollider', 'target': {'id': cube}}])['results'][0]

    assert fitted['entityId'] == cube
    assert fitted['component']['type'] == 'Collider'
    box = fitted['component']['properties']
    assert box['Type'] == 'Box'
    assert near(box['Size'], vec(1, 1, 1)), box            # the cube model spans -1..1
    assert near(box['Position'], vec(0, 0, 0)), box
    assert fit_matches_geometry(cube, box), box
    assert collider_of(cube)['Size'] == box['Size']

    # Padding grows every half-extent, so the box clears the model on all six sides.
    padded = edit([{'op': 'entity.fitCollider', 'target': {'id': cube}, 'padding': 0.25}])['results'][0]
    assert near(padded['component']['properties']['Size'], vec(1.25, 1.25, 1.25))
    assert near(padded['component']['properties']['Position'], vec(0, 0, 0))

    # ------------------------------------------------------------------ the point of it

    # Size is measured in model space and multiplied by the entity's world scale by the engine, and
    # the shape is rotated with the entity. So a rotated prop keeps a tight box, where a box built
    # from world bounds would have to grow to stay axis aligned.
    turned = prop('engine/primitive/cube',
                  LocalRotation={'w': math.cos(math.pi / 8), 'x': 0, 'y': math.sin(math.pi / 8), 'z': 0})
    box = edit([{'op': 'entity.fitCollider', 'target': {'id': turned}}])['results'][0]['component']['properties']
    assert near(box['Size'], vec(1, 1, 1)), box

    assert fit_matches_geometry(turned, box, axis_aligned=False), box

    world = fetch('/spatial/query', {'entities': [{'id': turned}]})['entities'][0]['bounds']
    assert world['dimensions']['x'] > 2.7, world   # 2 * sqrt(2), what an axis-aligned box would need
    assert box['Size']['x'] * 2 < world['dimensions']['x'], (box, world)

    # Scale multiplies the box in the engine, so the fitted Size must stay the model's own.
    scaled = prop('engine/primitive/cube', LocalScale=vec(3, 3, 3))
    box = edit([{'op': 'entity.fitCollider', 'target': {'id': scaled}}])['results'][0]['component']['properties']
    assert fit_matches_geometry(scaled, box), box
    assert near(box['Size'], vec(1, 1, 1)), box

    # ------------------------------------------------------------------ an off-centre pivot

    # Position is an unscaled, unrotated world-axis offset from the entity's world position, so the
    # model-space centre has to be scaled and rotated into world space.
    plain = prop(offcentre)
    box = edit([{'op': 'entity.fitCollider', 'target': {'id': plain}}])['results'][0]['component']['properties']
    assert fit_matches_geometry(plain, box), box
    assert near(box['Size'], vec(1, 0.5, 1)), box
    assert near(box['Position'], vec(3, 0.5, 0)), box

    big = prop(offcentre, LocalScale=vec(2, 2, 2))
    box = edit([{'op': 'entity.fitCollider', 'target': {'id': big}}])['results'][0]['component']['properties']
    assert fit_matches_geometry(big, box), box
    assert near(box['Size'], vec(1, 0.5, 1)), box
    assert near(box['Position'], vec(6, 1, 0)), box

    # A quarter turn about Y sends +X to -Z, so the centre offset follows.
    quarter = prop(offcentre, LocalRotation={'w': math.cos(math.pi / 4), 'x': 0,
                                             'y': math.sin(math.pi / 4), 'z': 0})
    box = edit([{'op': 'entity.fitCollider', 'target': {'id': quarter}}])['results'][0]['component']['properties']
    assert fit_matches_geometry(quarter, box, axis_aligned=False), box
    assert near(box['Size'], vec(1, 0.5, 1)), box
    assert near(box['Position'], vec(0, 0.5, -3)), box

    # ------------------------------------------------------------------ refusals

    assert edit([{'op': 'entity.fitCollider', 'target': {'id': cube}, 'padding': -1}], 400)['path'] \
        == '/operations/0/padding'
    assert edit([{'op': 'entity.fitCollider', 'target': {'id': cube}, 'nonsense': 1}], 400)['path'] \
        == '/operations/0/nonsense'

    bare = edit([{'op': 'entity.create', 'name': 'No collider', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]}])
    bare = bare['results'][0]['entity']['id']
    assert 'Collider' in edit([{'op': 'entity.fitCollider', 'target': {'id': bare}}], 400)['error']

    modelless = edit([{'op': 'entity.create', 'name': 'No model', 'components': [
        {'type': 'Collider', 'properties': {}}]}])['results'][0]['entity']['id']
    assert 'ModelRenderer' in edit([{'op': 'entity.fitCollider', 'target': {'id': modelless}}], 400)['error']

    sphere = prop('engine/primitive/sphere')
    edit([{'op': 'component.update',
           'target': {'id': [c['id'] for c in fetch('/entity?id=' + sphere)['components']
                             if c['type'] == 'Collider'][0]},
           'properties': {'Type': 'Sphere'}}])
    assert 'Box' in edit([{'op': 'entity.fitCollider', 'target': {'id': sphere}}], 400)['error']

    # ------------------------------------------------------------------ refs as targets

    # The whole point: build, place, aim and fit in one request, without a round trip to learn the
    # new entity's ID.
    batch = edit([
        {'op': 'entity.create', 'ref': 'crate', 'name': 'Crate', 'components': [
            {'type': 'Transform', 'properties': {'LocalScale': vec(2, 2, 2)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': offcentre}}},
            {'type': 'Collider', 'properties': {'IsTrigger': True, 'Layer': 4}}]},
        {'op': 'entity.place', 'target': {'ref': 'crate'},
         'surface': {'point': vec(10, 0, 5), 'normal': vec(0, 1, 0)},
         'anchor': {'type': 'modelBounds'}},
        {'op': 'entity.aim', 'target': {'ref': 'crate'}, 'point': vec(10, 0, 40),
         'forwardAxis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0)},
        {'op': 'entity.fitCollider', 'target': {'ref': 'crate'}, 'padding': 0.05},
    ])
    crate = batch['refs']['crate']
    assert batch['results'][3]['entityId'] == crate

    box = batch['results'][3]['component']['properties']
    assert near(box['Size'], vec(1.05, 0.55, 1.05)), box
    assert fit_matches_geometry(crate, box, padding=0.05, axis_aligned=False), box
    # Settings the fit has no opinion about survive it.
    assert box['IsTrigger'] is True and box['Layer'] == 4, box
    assert collider_of(crate)['Size'] == box['Size']

    # Placement put the model's lowest corner on the surface; the fitted box agrees with it.
    placed = fetch('/spatial/query', {'entities': [{'id': crate}]})['entities'][0]['bounds']
    assert near(placed['min']['y'], 0, 1e-4), placed

    # ------------------------------------------------------------------ ref refusals

    assert edit([
        {'op': 'entity.fitCollider', 'target': {'ref': 'later'}},
        {'op': 'entity.create', 'ref': 'later', 'name': 'Too late'},
    ], 400)['path'] == '/operations/0/target/ref'

    assert edit([{'op': 'entity.place', 'target': {'ref': 'unknown'},
                  'surface': {'point': vec(0, 0, 0), 'normal': vec(0, 1, 0)},
                  'anchor': {'type': 'modelBounds'}}], 400)['path'] == '/operations/0/target/ref'

    assert edit([{'op': 'entity.aim', 'target': {'id': cube, 'ref': 'crate'}, 'point': vec(0, 0, 1),
                  'forwardAxis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0)}], 400)['path'] \
        == '/operations/0/target'

    # Only the three operations that compute from an entity's own geometry take a ref. Everything
    # else still wants an ID, and says so rather than quietly accepting one form of reference.
    assert edit([
        {'op': 'entity.create', 'ref': 'doomed', 'name': 'Doomed'},
        {'op': 'entity.delete', 'target': {'ref': 'doomed'}},
    ], 400)['path'] == '/operations/1/target/ref'
    assert edit([
        {'op': 'entity.create', 'ref': 'doomed', 'name': 'Doomed'},
        {'op': 'entity.update', 'target': {'ref': 'doomed'}, 'properties': {'name': 'Renamed'}},
    ], 400)['path'] == '/operations/1/target/ref'

    # component.add earlier in the batch is what makes the collider exist, so the fit must see it.
    combined = edit([
        {'op': 'entity.create', 'ref': 'barrel', 'name': 'Barrel', 'components': [
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.fitCollider', 'target': {'ref': 'barrel'}},
    ], 400)
    assert 'Collider' in combined['error'], combined

    # ------------------------------------------------------------------ undo

    before = collider_of(plain)
    edit([{'op': 'entity.fitCollider', 'target': {'id': plain}, 'padding': 2}])
    assert not near(collider_of(plain)['Size'], before['Size'])
    assert fetch('/history/undo', {})['applied'] is True
    assert collider_of(plain) == before, (collider_of(plain), before)

    print('Collider fit verification passed.', flush=True)
finally:
    shutdown()

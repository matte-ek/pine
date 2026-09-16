"""HTTP retry/editing verification; run only against a disposable Pine project."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import http.client
import itertools
import json
import math
import socket
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19024')
parser.add_argument('--level', help='Optional loaded Level path; replacement deletes the current scene.')
parser.add_argument('--fill-retention', action='store_true', help='Fill the registry to verify capacity rejection.')
args = parser.parse_args()
prefix = uuid.uuid4().hex
counter = itertools.count()
session = None


def identity():
    return prefix + '-' + str(next(counter))


def encode(body):
    return body if isinstance(body, bytes) else json.dumps(body).encode()


def request(path, body=None, key=None, expected=200, headers=None, method=None):
    data = None if body is None else encode(body)
    request_headers = {'Content-Type': 'application/json'}
    if key:
        request_headers.update({'Idempotency-Key': key, 'X-Pine-Session': session})
    request_headers.update(headers or {})
    req = urllib.request.Request(args.url + path, data=data, headers=request_headers, method=method)
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        return json.loads(payload)


def status(key, expected=200):
    return request('/requests?id=' + key, headers={'X-Pine-Session': session}, expected=expected)


def edit(operations, key=None, expected=200):
    return request('/edit', {'version': 1, 'operations': operations}, key, expected)


def vector(x, y, z):
    return dict(x=x, y=y, z=z)


def xyz(value):
    return [value[axis] for axis in 'xyz']


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def assert_bounds_fit(framed, padding):
    state, bounds = framed['state'], framed['framedBounds']
    forward, up = xyz(framed['forward']), xyz(framed['up'])
    right = [forward[1]*up[2]-forward[2]*up[1], forward[2]*up[0]-forward[0]*up[2],
             forward[0]*up[1]-forward[1]*up[0]]
    tangent_y = math.tan(math.radians(state['fieldOfView']) / 2)
    tangent_x = tangent_y * framed['viewport']['width'] / framed['viewport']['height']
    for corner in range(8):
        point = [bounds['max' if corner & (1 << i) else 'min'][axis] for i, axis in enumerate('xyz')]
        offset = [x-y for x, y in zip(point, xyz(state['position']))]
        depth = dot(offset, forward)
        assert state['nearPlane'] <= depth <= state['farPlane']
        assert abs(dot(offset, right))*padding <= depth*tangent_x + 1e-4
        assert abs(dot(offset, up))*padding <= depth*tangent_y + 1e-4


policy = request('/requests')
session = policy['session']
assert policy['retentionSeconds'] == 600 and policy['maxRetainedRequests'] == 256
assert request('/camera')['viewport']['active'], 'Select the Level tab before running this recipe.'
unknown_key = identity()
assert status(unknown_key, 404)['request']['mayHaveExecuted']
request('/requests?id=' + unknown_key, expected=400)
old_session = request('/requests?id=' + unknown_key, headers={'X-Pine-Session': 'old-session'}, expected=409)
assert old_session['request']['state'] == 'unknown' and old_session['request']['mayHaveExecuted']
assert old_session['currentSession'] == session
request('/requests/cancel', method='POST', expected=400)
request('/requests?id=x&id=y', expected=400)

# Reject the whole batch, including a valid first creation, before any scene write.
before = request('/entities')
invalid_key = identity()
invalid = [
    {'op': 'entity.create', 'name': 'Must not exist'},
    {'op': 'component.update', 'target': {'id': 'invalid'}, 'properties': {}}
]
rejected = edit(invalid, invalid_key, 400)
assert rejected['phase'] == 'validation' and rejected['completed'] == 0
assert rejected['request']['state'] == 'rejected' and not rejected['request']['mayHaveExecuted']
assert edit(invalid, invalid_key, 400) == rejected
assert status(invalid_key)['result'] == {'status': 400, 'body': rejected}
assert request('/entities') == before

key = identity()
body = {'version': 1, 'operations': [
    {'op': 'entity.create', 'ref': 'parent', 'name': 'Retry parent ' + prefix},
    {'op': 'entity.create', 'ref': 'cube', 'name': 'Retry cube', 'parent': {'ref': 'parent'}, 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(2, 0, 0)}},
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}
    ]},
    {'op': 'entity.create', 'ref': 'light', 'name': 'Retry light', 'components': [
        {'type': 'Light', 'properties': {'Type': 'SpotLight', 'SpotlightInnerAngle': 10, 'SpotlightOuterAngle': 20}}
    ]}
]}
with ThreadPoolExecutor(max_workers=8) as pool:
    replies = list(pool.map(lambda _: request('/edit', body, key), range(8)))
created = replies[0]
assert all(reply == created for reply in replies)
assert created['request']['state'] == 'succeeded' and created['request']['tracked']
assert created['observationToken']['session'] == session
assert status(key)['result'] == {'status': 200, 'body': created}
parent_id, cube_id = created['refs']['parent'], created['refs']['cube']
assert request('/entity?id=' + cube_id)['parent']['id'] == parent_id
# Different path, body bytes (including whitespace), or query cannot reuse this key.
request('/camera', body, key, 409)
request('/edit', encode(body) + b' ', key, 409)
request('/edit?different=1', body, key, 409)
assert status(key)['result']['body'] == created
assert request('/requests/cancel?id=' + key, method='POST',
               headers={'X-Pine-Session': session})['request']['state'] == 'succeeded'
request('/edit', body, expected=400, headers={'Idempotency-Key': identity()})
request('/edit', body, expected=400, headers={'Idempotency-Key': 'invalid key', 'X-Pine-Session': session})
request('/edit', body, expected=409, headers={'Idempotency-Key': identity(), 'X-Pine-Session': 'old-session'})
request('/observe', {}, identity(), 400)
oversized_key = identity()
request('/edit', b' ' * (256 * 1024 + 1), oversized_key, 413)
assert status(oversized_key, 404)['request']['state'] == 'unknown'
request('/edit?path=x&path=y', body, identity(), 400)

# Existing-entity references and patches must compose against earlier proposed state.
child = edit([{'op': 'entity.create', 'parent': {'id': parent_id}, 'name': 'Existing parent child'}], identity())
child_id = child['results'][0]['entity']['id']
assert request('/entity?id=' + child_id)['parent']['id'] == parent_id
light_component = next(c for c in created['results'][2]['entity']['components'] if c['type'] == 'Light')
patches = [
    {'op': 'component.update', 'target': {'id': light_component['id']}, 'properties': {'SpotlightOuterAngle': 45}},
    {'op': 'component.update', 'target': {'id': light_component['id']}, 'properties': {'SpotlightInnerAngle': 40}}
]
patched = edit(patches, identity())
properties = patched['results'][-1]['component']['properties']
assert properties['SpotlightOuterAngle'] == 45 and properties['SpotlightInnerAngle'] == 40

# Lose the entire reply after admission, then recover the original result by identity.
lost_key = identity()
lost_body = {'version': 1, 'operations': [{'op': 'entity.create', 'name': 'Lost reply ' + prefix}]}
url = urllib.parse.urlsplit(args.url)
connection = http.client.HTTPConnection(url.hostname, url.port, timeout=15)
connection.request('POST', '/edit', body=encode(lost_body), headers={
    'Content-Type': 'application/json', 'Idempotency-Key': lost_key, 'X-Pine-Session': session})
deadline = time.monotonic() + 4
while True:
    try:
        retained = status(lost_key)
        break
    except AssertionError:
        if time.monotonic() >= deadline:
            raise
        time.sleep(.01)  # Poll admission only; never sleep to establish render ordering.
connection.sock.shutdown(socket.SHUT_RDWR)
connection.close()
recovered = request('/edit', lost_body, lost_key)
assert status(lost_key)['result']['body'] == recovered
assert request('/edit', lost_body, lost_key) == recovered

# Retrying a camera operation must not undo a later view or advance its revision.
original_camera = request('/camera')['state']
frame_key = identity()
frame_body = {'entities': [{'id': parent_id}], 'includeChildren': True, 'padding': 1.3}
framed = request('/camera/frame', frame_body, frame_key)
assert_bounds_fit(framed, frame_body['padding'])
for _ in range(3):
    observed = request('/observe', {'after': framed['observationToken'], 'width': 80})
    assert observed['camera']['position'] == framed['state']['position']
    assert observed['camera']['rotation'] == framed['state']['rotation']
    assert request('/camera')['state'] == framed['state']
restore_key = identity()
restored = request('/camera', original_camera, restore_key)
assert request('/camera/frame', frame_body, frame_key) == framed
assert request('/camera')['state'] == restored['state']
assert request('/camera', original_camera, restore_key) == restored
observed = request('/observe', {'after': restored['observationToken'], 'width': 80})
assert observed['frame']['revision'] == restored['observationToken']['revision']
assert observed['frame']['id'] > restored['observationToken']['frame']

# Replaying a creation must not produce additional entities.
def flatten(entities):
    for entity in entities:
        yield entity
        yield from flatten(entity['children'])

entities = list(flatten(request('/entities')['entities']))
assert sum(e['name'] == 'Retry parent ' + prefix for e in entities) == 1
assert sum(e['name'] == 'Lost reply ' + prefix for e in entities) == 1

if args.level:
    load_key = identity()
    load_body = {'path': args.level}
    loaded = request('/level/load', load_body, load_key)
    assert loaded['observationToken']['sceneGeneration'] > created['observationToken']['sceneGeneration']
    assert request('/edit', body, key) == created
    request('/entity?id=' + parent_id, expected=404)
    request('/observe', {'after': created['observationToken']}, expected=409)
    # Repeated loading with the same identity must not replace the scene again.
    assert request('/level/load', load_body, load_key) == loaded
    observed = request('/observe', {'after': loaded['observationToken'], 'width': 80})
    assert observed['frame']['sceneGeneration'] == loaded['observationToken']['sceneGeneration']
    assert observed['frame']['revision'] == loaded['observationToken']['revision']
    print('PASS: retained replies survive scene replacement; retries neither recreate nor reload; stale capture tokens fail.')

if args.fill_retention:
    for _ in range(policy['maxRetainedRequests']):
        fill_key = identity()
        # A retained validation rejection exercises capacity without creating scene objects.
        try:
            request('/camera', {'unknown': True}, fill_key, 400)
        except AssertionError as error:
            assert error.args[0][1] == 503, error
            assert status(fill_key, 404)['request']['state'] == 'unknown'
            break
    else:
        raise AssertionError('Registry never reached its advertised capacity')
    assert request('/edit', body, key) == created, 'Capacity pressure evicted an unexpired result'
    print('PASS: bounded registry rejects new identities and keeps earlier results replayable.')

print('PASS: concurrent and interrupted-reply retries, conflicts, status, validation, references, composed patches, camera persistence and bounds.')

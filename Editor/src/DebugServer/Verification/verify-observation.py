"""HTTP verification recipe; run only against a temporary Pine project."""

import argparse
import base64
import json
import math
from pathlib import Path
import struct
import urllib.error
import urllib.request


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19023')
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-observation-results'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def request(path, body=None, expected=200):
    data = body if isinstance(body, bytes) else None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(args.url + path, data=data, headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    payload = response.read()
    assert response.code == expected, (path, response.code, payload[:1000])
    return json.loads(payload)


def vector(x, y, z):
    return dict(x=x, y=y, z=z)


def edit(operations):
    return request('/edit', {'version': 1, 'operations': operations})


def observe(token, entities, name):
    observation = request('/observe', {'after': token, 'entities': entities, 'width': 800})
    frame = observation['frame']
    assert frame['id'] > token['frame']
    assert frame['revision'] >= token['revision']
    assert frame['session'] == token['session']
    assert frame['sceneGeneration'] == token['sceneGeneration']
    assert {entity['id'] for entity in observation['entities']} == set(entities)

    image = observation['image']
    png = base64.b64decode(image['data'], validate=True)
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', png[16:24]) == (image['width'], image['height'])
    viewport = observation['viewport']
    projection = observation['camera']['projectionMatrix']
    assert math.isclose(projection[1][1] / projection[0][0], viewport['width'] / viewport['height'], rel_tol=1e-5)
    (args.output / (name + '.png')).write_bytes(png)
    metadata = {**observation, 'image': {k: v for k, v in image.items() if k != 'data'}}
    (args.output / (name + '.json')).write_text(json.dumps(metadata, indent=2))
    return observation, png


def transform_position(observation, entity_id):
    entity = next(entity for entity in observation['entities'] if entity['id'] == entity_id)
    transform = next(component for component in entity['components'] if component['type'] == 'Transform')
    # Compare the complete serialized component with the public entity readback,
    # so this recipe need not invent a second schema for the inspection dump.
    current = request('/entity?id=' + entity_id)
    assert transform == next(component for component in current['components'] if component['type'] == 'Transform')
    return transform['data']


assert request('/camera')['viewport']['active'], 'Open the Level tab before running this recipe.'
created = edit([
    {'op': 'entity.create', 'name': 'Observation cube', 'ref': 'cube', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}
    ]},
    {'op': 'entity.create', 'name': 'Observation light', 'ref': 'light', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vector(3, 5, 6)}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Range': 30, 'Intensity': 5}}
    ]}
])
(args.output / 'creation.json').write_text(json.dumps(created, indent=2))
cube_id, light_id = created['refs']['cube'], created['refs']['light']
transform_id = next(component['id'] for component in created['results'][0]['entity']['components'] if component['type'] == 'Transform')
framed = request('/camera/frame', {'entities': [{'id': cube_id}], 'padding': 2.5, 'direction': vector(0, -.2, -1)})
first, first_png = observe(framed['observationToken'], [cube_id, light_id], 'before')
assert first['camera']['position'] == framed['state']['position']
assert first['camera']['rotation'] == framed['state']['rotation']
first_transform = transform_position(first, cube_id)

moved = edit([{'op': 'component.update', 'target': {'id': transform_id}, 'properties': {'LocalPosition': vector(2, 0, 0)}}])
second, second_png = observe(moved['observationToken'], [cube_id, light_id], 'moved')
assert second['frame']['id'] > first['frame']['id']
assert second['camera'] == first['camera']
assert second_png != first_png
assert transform_position(second, cube_id) != first_transform

restored = edit([{'op': 'component.update', 'target': {'id': transform_id}, 'properties': {'LocalPosition': vector(0, 0, 0)}}])
third, third_png = observe(restored['observationToken'], [cube_id], 'restored')
assert transform_position(third, cube_id) == first_transform
# A token is an ordering barrier: a later edit is allowed to supersede its state.
latest, _ = observe(moved['observationToken'], [cube_id], 'superseded')
assert transform_position(latest, cube_id) == first_transform
assert latest['frame']['revision'] > moved['observationToken']['revision']

logs = request('/logs')
assert request('/logs?since=' + str(logs['nextCursor']))['messages'] == []
page = request('/logs?since=0&limit=2')
assert len(page['messages']) <= 2
if page['hasMore']:
    following = request('/logs?since=' + str(page['nextCursor']) + '&limit=2')
    assert all(entry['sequence'] > page['nextCursor'] for entry in following['messages'])
for invalid in ['-1', 'x', '18446744073709551616', '1.5']:
    request('/logs?since=' + invalid, expected=400)
request('/logs?since=18446744073709551615', expected=409)
request('/logs?limit=0', expected=400)

for body in [b'not json', b'[]', b' '*16385, {'width': 0}, {'width': 4097}, {'width': 1.5},
             {'entities': ['not-an-id']}, {'entities': [cube_id]*129}, {'view': 'missing'},
             {'unknown': True}, {'after': {}}, {'logsSince': -1}, {'logsSince': 1e100}]:
    request('/observe', body, expected=400)
request('/observe', {'after': {**framed['observationToken'], 'session': 'other'}}, expected=409)
request('/observe', {'after': {**framed['observationToken'], 'sceneGeneration': 0}}, expected=409)
request('/observe', {'after': {**framed['observationToken'], 'revision': 18446744073709551615}}, expected=400)
fresh = request('/observe', {'width': 1})
assert fresh['image']['width'] == 1
expected_height = max(1, math.floor(fresh['viewport']['height'] / fresh['viewport']['width'] + .5))
assert fresh['image']['height'] == expected_height
assert fresh['frame']['id'] > fresh['after']['frame']
print('PASS: create/frame/capture/move/capture/restore, frame and camera metadata, PNG dimensions, entity state, log paging, and validation.')
print('Inspect before.png, moved.png, and restored.png in', args.output)

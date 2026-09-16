"""Import endpoint verification; run against a disposable project on this machine."""

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import struct
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import uuid


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://127.0.0.1:19029')
parser.add_argument('--data', type=Path, required=True, help='Temporary Editor working directory.')
parser.add_argument('--model', type=Path, required=True, help='A valid self-contained GLB, copied before modification.')
args = parser.parse_args()
source_root = Path(tempfile.mkdtemp(prefix='pine-import-sources.'))
directory = 'import-' + uuid.uuid4().hex
session = None


def request(path, body=None, key=None, expected=200):
    headers = {'Content-Type': 'application/json'}
    if key:
        headers.update({'Idempotency-Key': key, 'X-Pine-Session': session})
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


def translated_glb(original):
    # Wrap the existing scene in a translated node, preserving geometry and binary chunks.
    magic, version, length = struct.unpack_from('<III', original)
    json_length, chunk_type = struct.unpack_from('<II', original, 12)
    assert magic == 0x46546C67 and version == 2 and length == len(original)
    assert chunk_type == 0x4E4F534A
    document = json.loads(original[20:20 + json_length])
    scene = document['scenes'][document.get('scene', 0)]
    root_index = len(document['nodes'])
    document['nodes'].append({'translation': [2, 0, 0], 'children': scene['nodes']})
    scene['nodes'] = [root_index]
    encoded = json.dumps(document).encode()
    encoded += b' ' * (-len(encoded) % 4)
    remainder = original[20 + json_length:]
    return (struct.pack('<III', magic, version, 20 + len(encoded) + len(remainder))
            + struct.pack('<II', len(encoded), chunk_type) + encoded + remainder)


def observe(token, entity, name):
    observed = request('/observe', {'after': token, 'entities': [entity], 'width': 600})
    png = base64.b64decode(observed['image']['data'])
    assert png.startswith(b'\x89PNG\r\n\x1a\n')
    (source_root / (name + '.png')).write_bytes(png)
    return png


session = request('/requests')['session']
assert request('/status')['playState'] == 'stopped'
assert request('/camera')['viewport']['active'], 'Open the Level viewport before running this recipe.'
original = args.model.read_bytes()
source = source_root / 'probe.glb'
source.write_bytes(original)
body = {'source': str(source), 'directory': directory}

# Invalid inputs leave both the asset registry and project destination untouched.
before = request('/assets')
invalid_bodies = [
    {}, {'source': 7}, {'source': str(source_root)}, {'source': str(source_root / 'missing.glb')},
    {**body, 'extra': True}, {**body, 'directory': '../escape'},
    {**body, 'directory': '/tmp/escape'}, {**body, 'directory': 'BadCase'},
    {**body, 'directory': 'bad\u0000path'}, {**body, 'overwrite': 1},
]
for invalid in invalid_bodies:
    rejected = request('/assets/import', invalid, uuid.uuid4().hex, 400)
    assert rejected['request']['state'] == 'rejected'
    assert not rejected['request']['mayHaveExecuted']
unsupported = source_root / 'unsupported.txt'
unsupported.write_text('not an asset')
request('/assets/import', {'source': str(unsupported)}, expected=400)
request('/assets/import?unexpected=1', body, expected=400)
assert request('/assets') == before

# Concurrent retries all receive one import, including identical asset identity and token.
key = uuid.uuid4().hex
with ThreadPoolExecutor(max_workers=4) as pool:
    replies = list(pool.map(lambda _: request('/assets/import', body, key), range(4)))
created = replies[0]
assert all(reply == created for reply in replies)
asset = created['imports'][0]
assert asset['action'] == 'create' and asset['status'] == 'imported'
assert asset['path'] == directory + '/probe'
asset_file = args.data / asset['file']
content_file = args.data / asset['sources'][0]
assert asset_file.is_file() and content_file.read_bytes() == original
readback = request('/asset?id=' + asset['id'])
assert readback['uid'] == asset['id'] and readback['type'] == 'Model'
assert any(item['uid'] == asset['id'] for item in request('/assets')['assets'])
request('/assets/import', body, expected=409)

# A registered asset is usable immediately, including its loaded mesh bounds and GPU resources.
edit = request('/edit', {'version': 1, 'operations': [
    {'op': 'entity.create', 'ref': 'prop', 'name': 'Imported prop', 'components': [
        {'type': 'ModelRenderer', 'properties': {'Model': {'id': asset['id']}}}
    ]},
    {'op': 'entity.create', 'name': 'Import light', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': {'x': 3, 'y': 5, 'z': 6}}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Range': 30, 'Intensity': 5}}
    ]}
]})
entity = edit['refs']['prop']
frame_body = {'entities': [{'id': entity}], 'padding': 2,
              'direction': {'x': -1, 'y': -0.5, 'z': -1}}
framed = request('/camera/frame', frame_body)
first_png = observe(framed['observationToken'], entity, 'before')

# New source bytes do not make a retained retry execute again.
updated_bytes = translated_glb(original)
source.write_bytes(updated_bytes)
assert request('/assets/import', body, key) == created
assert content_file.read_bytes() == original
updated = request('/assets/import', {**body, 'overwrite': True}, uuid.uuid4().hex)
assert updated['imports'][0]['id'] == asset['id']
assert updated['imports'][0]['action'] == 'update'
assert content_file.read_bytes() == updated_bytes
second_png = observe(updated['observationToken'], entity, 'updated')
assert second_png != first_png
reframed = request('/camera/frame', frame_body)
for corner in ['min', 'max']:
    assert abs(reframed['framedBounds'][corner]['x'] - framed['framedBounds'][corner]['x'] - 2) < 0.0001

# Root imports exercise the shared copy-to-self guard on a later import from content/.
root_source = source_root / ('root-' + uuid.uuid4().hex + '.glb')
root_source.write_bytes(original)
root_asset = request('/assets/import', {'source': str(root_source)})['imports'][0]
root_content = args.data / root_asset['sources'][0]
same_file = request('/assets/import', {'source': str(root_content), 'overwrite': True})['imports'][0]
assert same_file['id'] == root_asset['id'] and root_content.read_bytes() == original

# Conflicts must not overwrite an unloaded .passet or a loaded asset of another type.
conflict_source = source_root / 'unloaded.glb'
conflict_source.write_bytes(original)
conflict_destination = asset_file.parent / 'unloaded.passet'
conflict_destination.write_bytes(b'unloaded sentinel')
request('/assets/import', {'source': str(conflict_source), 'directory': directory}, expected=409)
assert conflict_destination.read_bytes() == b'unloaded sentinel'
conflict_destination.unlink()
other_type = source.with_suffix('.png')
other_type.write_bytes(b'not decoded because the asset type conflicts')
request('/assets/import', {'source': str(other_type), 'directory': directory, 'overwrite': True}, expected=409)

# Decode failures happen after copying; report partial effects and retain the failure on retry.
broken = source_root / 'broken.glb'
broken.write_bytes(b'not a GLB')
failure_body = {'source': str(broken), 'directory': directory}
failure_key = uuid.uuid4().hex
failed = request('/assets/import', failure_body, failure_key, 500)
assert failed['request']['state'] == 'failed' and failed['request']['mayHaveExecuted']
assert failed['imports'][0]['status'] == 'failed' and 'observationToken' in failed
assert request('/assets/import', failure_body, failure_key, 500) == failed
request('/asset?path=' + urllib.parse.quote(directory + '/broken'), expected=404)

print('Verified external import, source copy, loaded model, rendered update, stable ID, retries, validation and failures.')
print('Screenshots and source fixtures:', source_root)
print('Persistent model for restart verification:', asset['path'], asset['id'])

"""Check asset previews, batched asset summaries and posed scene captures; needs an Editor build and Xvfb."""

import argparse
import base64
import concurrent.futures
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import zlib


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19051)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-capture-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-capture.'))
url = 'http://127.0.0.1:%d' % args.port
print('Capture verification:', root, flush=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Timestamps are
# preserved so the engine assets are not all re-imported on boot.
data = root / 'data'
(data / 'projects/capture/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'editor.log'
log = log_path.open('w')
process = subprocess.Popen(['xvfb-run', '-a', str(build / 'Editor/Editor'), 'capture'], cwd=data,
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


def fetch(path, body=None, expected=200, method=None):
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(url + path, data=data, method=method,
                                     headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(request, timeout=30)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        assert response.code == expected, (path, response.code, payload[:1500])
        return payload


def request(path, body=None, expected=200):
    return json.loads(fetch(path, body, expected))


def png(path, expected=200):
    payload = fetch(path, expected=expected)
    if expected != 200:
        return json.loads(payload)
    assert payload[:8] == b'\x89PNG\r\n\x1a\n', payload[:16]
    return payload


def query(route, **parameters):
    return route + '?' + urllib.parse.urlencode(parameters)


def decode(payload):
    """Minimal RGBA8 PNG reader, enough to tell a rendered image from a flat fill."""
    width, height, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', payload[16:29])
    assert (depth, colour, interlace) == (8, 6, 0), (depth, colour, interlace)

    compressed = b''
    offset = 8
    while offset < len(payload):
        length, kind = struct.unpack('>I4s', payload[offset:offset + 8])
        if kind == b'IDAT':
            compressed += payload[offset + 8:offset + 8 + length]
        offset += length + 12

    raw = zlib.decompress(compressed)
    stride = width * 4
    rows = []
    previous = bytearray(stride)
    position = 0
    for _ in range(height):
        filter_type = raw[position]
        line = bytearray(raw[position + 1:position + 1 + stride])
        position += stride + 1
        for index in range(stride):
            left = line[index - 4] if index >= 4 else 0
            up = previous[index]
            corner = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                line[index] = (line[index] + left) & 0xFF
            elif filter_type == 2:
                line[index] = (line[index] + up) & 0xFF
            elif filter_type == 3:
                line[index] = (line[index] + (left + up) // 2) & 0xFF
            elif filter_type == 4:
                delta = left + up - corner
                candidates = [abs(delta - left), abs(delta - up), abs(delta - corner)]
                nearest = [left, up, corner][candidates.index(min(candidates))]
                line[index] = (line[index] + nearest) & 0xFF
            elif filter_type != 0:
                raise AssertionError('unknown PNG filter %d' % filter_type)
        rows.append(bytes(line))
        previous = line
    return width, height, rows


def colours(payload):
    width, height, rows = decode(payload)
    return width, height, {row[index:index + 4] for row in rows for index in range(0, width * 4, 4)}


def save(name, payload):
    (args.output / name).write_bytes(payload)
    return payload


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def edit(operations, expected=200):
    return request('/edit', {'version': 1, 'operations': operations}, expected)


try:
    deadline = time.time() + 120
    while True:
        try:
            status = request('/status')
            break
        except Exception:
            assert process.poll() is None, 'Editor exited early; see ' + str(log_path)
            assert time.time() < deadline, 'Editor did not start serving in time; see ' + str(log_path)
            time.sleep(0.5)
    assert status['playState'] == 'stopped', status

    catalogue = {asset['path']: asset for asset in request('/assets')['assets']}
    assert 'engine/primitive/cube' in catalogue and 'engine/materials/default' in catalogue

    # ---------------------------------------------------------------- asset previews

    cube = save('cube.png', png(query('/asset/preview.png', path='engine/primitive/cube')))
    width, height, distinct = colours(cube)
    assert (width, height) == (512, 512), (width, height)
    assert len(distinct) > 1, 'preview is a flat fill, so nothing was drawn'
    assert all(colour[3] == 255 for colour in distinct), 'preview should be opaque'

    sized = save('material-128x96.png', png(query('/asset/preview.png',
                                                  path='engine/materials/default', width=128, height=96)))
    assert colours(sized)[:2] == (128, 96)
    assert len(colours(sized)[2]) > 1

    # A model, a material on the preview sphere, and the same model from another angle all have to
    # produce genuinely different images - otherwise the subject or the view angle is being ignored.
    sphere = save('sphere.png', png(query('/asset/preview.png', path='engine/primitive/sphere')))
    turned = save('cube-yaw90.png', png(query('/asset/preview.png', path='engine/primitive/cube', yaw=90, pitch=-25)))
    assert cube != sphere and cube != turned

    by_id = png(query('/asset/preview.png', id=catalogue['engine/primitive/cube']['uid']))
    assert by_id == cube, 'the same subject by id and by path should render identically'

    shader = next(asset for asset in catalogue.values() if asset['type'] == 'Shader')
    assert png(query('/asset/preview.png', path=shader['path']), expected=409)['error']
    assert png(query('/asset/preview.png', path='no/such/asset'), expected=404)['error']
    assert png('/asset/preview.png', expected=400)['error']
    assert png(query('/asset/preview.png', path='engine/primitive/cube', width=4), expected=400)['error']
    assert png(query('/asset/preview.png', path='engine/primitive/cube', width=9000), expected=400)['error']
    assert png(query('/asset/preview.png', path='engine/primitive/cube', yaw='sideways'), expected=400)['error']

    # ---------------------------------------------------------------- batched summaries

    summary = request('/assets/summary', {'assets': [
        {'path': 'engine/primitive/cube'},
        {'id': catalogue['engine/primitive/sphere']['uid']},
        {'path': 'engine/materials/default'},
    ]})
    assert summary['count'] == 3
    model, _, material = summary['assets']

    assert model['path'] == 'engine/primitive/cube' and model['type'] == 'Model'
    assert model['meshCount'] >= 1 and model['vertexCount'] > 0
    for axis in 'xyz':
        assert model['bounds']['size'][axis] == model['bounds']['max'][axis] - model['bounds']['min'][axis]
    assert model['materials'] and all({'id', 'path'} == set(entry) for entry in model['materials'])

    # The summary reports the live model; GET /asset reports the stored one. For an unmodified
    # asset they have to agree, which is what makes the cheap route trustworthy.
    stored = request(query('/asset', path='engine/primitive/cube'))['content']['Data']['Meshes']
    for axis in 'xyz':
        assert abs(min(mesh['BoundingBoxMin'][axis] for mesh in stored) - model['bounds']['min'][axis]) < 1e-5
        assert abs(max(mesh['BoundingBoxMax'][axis] for mesh in stored) - model['bounds']['max'][axis]) < 1e-5

    assert material['type'] == 'Material'
    assert material['renderingMode'] in ('Opaque', 'Discard', 'Transparent')
    assert set(material['textures']) == {'diffuse', 'specular', 'normal'}
    assert set(material['diffuseColor']) == set('xyz')

    assert request('/assets/summary', {'assets': []}, 400)['path'] == '/assets'
    assert request('/assets/summary', {'assets': [{'path': 'no/such/asset'}]}, 400)['path'] == '/assets/0'
    assert request('/assets/summary', {'assets': [{'path': 'engine/primitive/cube', 'id': 'a-b'}]}, 400)
    assert request('/assets/summary', {'assets': [{'path': 'engine/primitive/cube'}] * 257}, 400)
    assert request('/assets/summary', {'models': []}, 400)['path'] == '/models'

    # ---------------------------------------------------------------- posed scene captures

    created = edit([
        {'op': 'entity.create', 'name': 'Capture cube', 'components': [
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.create', 'name': 'Capture light', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(4, 6, 8)}},
            {'type': 'Light', 'properties': {'Type': 'PointLight', 'Range': 40, 'Intensity': 6}}]},
    ])
    revision = created['observationToken']['revision']

    before = request('/camera')['state']
    level_status = request('/level/status')

    front = request('/render', {'position': vec(0, 1, 6), 'lookAt': vec(0, 0, 0), 'width': 320, 'height': 240})
    assert front['image']['width'] == 320 and front['image']['height'] == 240
    assert front['viewport'] == {'view': 'capture', 'width': 320, 'height': 240}
    assert front['frame']['revision'] >= revision, (front['frame'], revision)
    assert abs(front['camera']['position']['z'] - 6) < 1e-5
    assert front['camera']['fieldOfView'] == 70 and front['camera']['farPlane'] == 1000

    front_png = save('render-front.png', base64.b64decode(front['image']['data'], validate=True))
    assert colours(front_png)[:2] == (320, 240)
    assert len(colours(front_png)[2]) > 1, 'capture is a flat fill, so the scene did not render'

    # The headline property: capturing must not move the user's editor camera, and must not change
    # what the scene would save as.
    assert request('/camera')['state'] == before
    assert request('/level/status')['unsavedChanges'] == level_status['unsavedChanges']

    # Nor is the capture size tied to the viewport's, which is what /viewport.png and /observe are
    # limited to. An odd size that no panel would have proves the two are independent.
    odd = request('/render', {'position': vec(0, 1, 6), 'lookAt': vec(0, 0, 0), 'width': 641, 'height': 233})
    assert (odd['image']['width'], odd['image']['height']) == (641, 233)
    viewport = request('/camera')['viewport']
    assert (viewport['width'], viewport['height']) != (641, 233)

    # The capture camera is an editor entity, so it is visible as temporary and excluded from the
    # filtered query the same way the editor's own camera is.
    roots = request('/entities')['entities']
    capture_camera = next(entity for entity in roots if entity['name'] == 'DebugServerCaptureCamera')
    assert capture_camera['temporary'] is True
    queried = request('/entities/query', {'filter': {'name': {'value': 'DebugServerCaptureCamera'}}})
    assert queried['entities'] == []

    away = request('/render', {'position': vec(0, 1, 6), 'lookAt': vec(0, 0, 40), 'width': 320, 'height': 240})
    away_png = save('render-away.png', base64.b64decode(away['image']['data'], validate=True))
    assert away_png != front_png, 'looking the other way produced the same image'
    assert away['frame']['id'] > front['frame']['id']

    rotated = request('/render', {'position': vec(0, 1, 6), 'rotation': {'x': 0, 'y': 0, 'z': 0, 'w': 1},
                                  'width': 160, 'height': 160})
    assert rotated['image']['width'] == 160 and rotated['image']['height'] == 160

    # One capture renders per frame, so concurrent requests have to queue rather than overwrite each
    # other's viewpoint. Each has to come back with its own size and its own frame.
    poses = [{'position': vec(x, 2, 7), 'lookAt': vec(0, 0, 0), 'width': 128 + x * 16, 'height': 96}
             for x in range(4)]
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(poses)) as pool:
        captures = list(pool.map(lambda pose: request('/render', pose), poses))
    for pose, capture in zip(poses, captures):
        assert capture['image']['width'] == pose['width'], (pose, capture['image'])
        assert abs(capture['camera']['position']['x'] - pose['position']['x']) < 1e-5
    assert len({capture['frame']['id'] for capture in captures}) == len(poses), 'captures shared a frame'

    assert request('/render', {'lookAt': vec(0, 0, 0)}, 400)['path'] == '/position'
    assert request('/render', {'position': vec(0, 0, 0)}, 400)['path'] == '/rotation'
    assert request('/render', {'position': vec(0, 0, 0), 'lookAt': vec(1, 0, 0),
                               'rotation': {'x': 0, 'y': 0, 'z': 0, 'w': 1}}, 400)
    assert request('/render', {'position': vec(0, 0, 0), 'lookAt': vec(0, 0, 0)}, 400)
    assert request('/render', {'position': vec(0, 0, 0), 'lookAt': vec(1, 0, 0), 'width': 9000}, 400)
    assert request('/render', {'position': vec(0, 0, 0), 'lookAt': vec(1, 0, 0),
                               'nearPlane': 10, 'farPlane': 1}, 400)['path'] == '/farPlane'
    assert request('/render', {'position': vec(0, 0, 0), 'lookAt': vec(1, 0, 0), 'zoom': 2}, 400)['path'] == '/zoom'

    # Reads must not accept the mutation retry headers.
    request_with_headers = urllib.request.Request(
        url + '/render', data=json.dumps({'position': vec(0, 0, 0), 'lookAt': vec(1, 0, 0)}).encode(),
        headers={'Content-Type': 'application/json', 'Idempotency-Key': 'capture-1',
                 'X-Pine-Session': request('/requests')['session']})
    try:
        urllib.request.urlopen(request_with_headers, timeout=30)
        raise AssertionError('/render accepted retry headers')
    except urllib.error.HTTPError as error:
        assert error.code == 400, error.code

    print('Capture verification passed. Images in', args.output, flush=True)
finally:
    shutdown()

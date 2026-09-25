"""Check that a Discard material casts the shadow of its alpha-tested surface, not of its whole quad; needs an Editor build and Xvfb."""

import argparse
import base64
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import zlib

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19055)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-cutout-shadows-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-cutout-shadows.'))
url = 'http://127.0.0.1:%d' % args.port
print('Cutout shadows verification:', root, flush=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Timestamps are
# preserved so the engine assets are not all re-imported on boot.
data = root / 'data'
(data / 'projects/cutoutshadows/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)


def png_chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


def mask_png(clear_columns):
    """An 8x8 white RGBA image whose first 'clear_columns' columns have alpha 0."""
    size = 8
    clear_pixel = bytes([255, 255, 255, 0])
    solid_pixel = bytes([255, 255, 255, 255])
    row = b'\x00' + clear_pixel * clear_columns + solid_pixel * (size - clear_columns)
    header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return (b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', header) +
            png_chunk(b'IDAT', zlib.compress(row * size)) + png_chunk(b'IEND', b''))


def pad(payload, filler):
    return payload + filler * (-len(payload) % 4)


def write_quad(path, clear_columns):
    """A GLB of a unit quad in the XZ plane facing down (-Y), its diffuse map a mask.

    A GLB rather than an OBJ, because /assets/import copies only the file it is given: an OBJ's
    material library and texture would be left behind. This one carries its texture embedded.

    Facing down keeps it out of the captures: the camera looks at its back face, which the scene
    pass culls, so only its shadow is in the frame. A cascade culls front faces and so draws it.
    """
    positions = [(-0.5, 0, -0.5), (0.5, 0, -0.5), (0.5, 0, 0.5), (-0.5, 0, 0.5)]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    indices = [0, 1, 2, 0, 2, 3]

    views = [
        b''.join(struct.pack('<3f', *p) for p in positions),
        struct.pack('<3f', 0, -1, 0) * 4,
        b''.join(struct.pack('<2f', *uv) for uv in uvs),
        struct.pack('<6H', *indices),
        mask_png(clear_columns),
    ]

    binary, buffer_views = b'', []
    for view in views:
        buffer_views.append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(view)})
        binary = pad(binary + view, b'\x00')

    document = {
        'asset': {'version': '2.0'},
        'scene': 0,
        'scenes': [{'nodes': [0]}],
        'nodes': [{'mesh': 0}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2},
                                    'indices': 3, 'material': 0}]}],
        'materials': [{'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}}}],
        'textures': [{'source': 0}],
        'images': [{'bufferView': 4, 'mimeType': 'image/png'}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3',
             'min': [-0.5, 0, -0.5], 'max': [0.5, 0, 0.5]},
            {'bufferView': 1, 'componentType': 5126, 'count': 4, 'type': 'VEC3'},
            {'bufferView': 2, 'componentType': 5126, 'count': 4, 'type': 'VEC2'},
            {'bufferView': 3, 'componentType': 5123, 'count': 6, 'type': 'SCALAR'},
        ],
        'bufferViews': buffer_views,
        'buffers': [{'byteLength': len(binary)}],
    }

    json_chunk = pad(json.dumps(document).encode(), b' ')
    chunks = (struct.pack('<I4s', len(json_chunk), b'JSON') + json_chunk +
              struct.pack('<I4s', len(binary), b'BIN\x00') + binary)
    path.write_bytes(struct.pack('<4sII', b'glTF', 2, 12 + len(chunks)) + chunks)
    return path


sources = root / 'sources'
sources.mkdir()

# The same quad twice: once fully solid, which imports as an Opaque material, and once with its
# left half clear, which imports as a Discard one.
solid_source = write_quad(sources / 'solid.glb', 0)
cutout_source = write_quad(sources / 'cutout.glb', 4)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'editor.log'
log = log_path.open('w')
process = subprocess.Popen(headless_command(build / 'Editor/Editor', 'cutoutshadows'), cwd=data,
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
        response = urllib.request.urlopen(message, timeout=30)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.code == expected, (path, response.code, body[:1500])
        return json.loads(body)


def edit(operations):
    return fetch('/edit', {'version': 1, 'operations': operations})


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def component_id(entity_id, name):
    entity = fetch('/entity?id=' + entity_id)
    return next(component['id'] for component in entity['components'] if component['type'] == name)


def update_renderer(entity_id, **properties):
    edit([{'op': 'component.update', 'target': {'id': component_id(entity_id, 'ModelRenderer')},
           'properties': properties}])


def import_model(source):
    """Imports a quad and returns its model id and the rendering mode of its one material."""
    imported = fetch('/assets/import', {'source': str(source), 'directory': 'quads'})
    model = next(item for item in imported['imports'] if item['type'] == 'Model')

    summary = fetch('/assets/summary', {'assets': [{'id': model['id']}]})['assets'][0]
    assert len(summary['materials']) == 1, summary
    material_id = summary['materials'][0]['id']
    material = fetch('/assets/summary', {'assets': [{'id': material_id}]})['assets'][0]
    return model['id'], material['renderingMode']


def decode_png(payload):
    """Minimal RGB reader for the viewport's own 8-bit non-interlaced output."""
    assert payload[:8] == b'\x89PNG\r\n\x1a\n', 'Not a PNG'
    offset, compressed, width, height, channels = 8, b'', 0, 0, 0
    while offset < len(payload):
        length, kind = struct.unpack('>I4s', payload[offset:offset + 8])
        chunk = payload[offset + 8:offset + 8 + length]
        if kind == b'IHDR':
            width, height, depth, colour = struct.unpack('>IIBB', chunk[:10])
            assert depth == 8 and colour in (2, 6), 'Unexpected PNG format'
            channels = 3 if colour == 2 else 4
        elif kind == b'IDAT':
            compressed += chunk
        elif kind == b'IEND':
            break
        offset += length + 12

    raw = zlib.decompress(compressed)
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        filter_type, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                line[x] = (line[x] + left) & 0xff
            elif filter_type == 2:
                line[x] = (line[x] + up) & 0xff
            elif filter_type == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xff
            elif filter_type == 4:
                estimate = left + up - corner
                to_left, to_up, to_corner = abs(estimate - left), abs(estimate - up), abs(estimate - corner)

                # Ties break towards left, then up, then corner; see verify-terrain-lighting.py.
                if to_left <= to_up and to_left <= to_corner:
                    predictor = left
                elif to_up <= to_corner:
                    predictor = up
                else:
                    predictor = corner

                line[x] = (line[x] + predictor) & 0xff
        rows.append([tuple(line[x:x + 3]) for x in range(0, stride, channels)])
        previous = line
    return width, height, rows


def render(name):
    """The same pose every time, so two captures differ only by what an edit changed."""
    capture = fetch('/render', {'position': vec(0, 5, 5), 'lookAt': vec(0, 0, 0),
                                'width': 240, 'height': 180})
    image = base64.b64decode(capture['image']['data'], validate=True)
    (args.output / (name + '.png')).write_bytes(image)
    return decode_png(image)[2]


def luminance(pixel):
    return sum(pixel) / 3.0


# A shadow region is dark enough that lifting it moves a pixel by far more than this, and the
# frame is otherwise deterministic with grain off.
CHANGE = 20

FRAME = 240 * 180


def shadow_pixels(unshadowed, shadowed, label):
    """How many pixels the caster darkens. Nothing may brighten: the caster itself is out of view."""
    darker = brighter = 0
    for row_before, row_after in zip(unshadowed, shadowed):
        for pixel_before, pixel_after in zip(row_before, row_after):
            delta = luminance(pixel_after) - luminance(pixel_before)
            if delta < -CHANGE:
                darker += 1
            elif delta > CHANGE:
                brighter += 1
    print('%s: %d pixels in shadow, %d brighter' % (label, darker, brighter), flush=True)

    assert brighter < 10, (label, brighter)
    return darker


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

    # Grain is animated, and would make two captures of an unchanged scene differ.
    fetch('/level/settings', {'properties': {'GrainStrength': 0.0}})

    existing_lights = fetch('/entities/query', {'filter': {'component': 'Light'}})['entities']
    assert not existing_lights, 'The fresh level already has lights, which would muddy every capture'

    # ------------------------------------------------------------------ the quads

    solid_model, solid_mode = import_model(solid_source)
    cutout_model, cutout_mode = import_model(cutout_source)

    # The importer picks the mode from the diffuse map's alpha. If it stops doing that, the
    # comparison below would pass or fail for the wrong reason.
    assert solid_mode == 'Opaque', solid_mode
    assert cutout_mode == 'Discard', cutout_mode

    # ------------------------------------------------------------------ the scene

    created = edit([
        {'op': 'entity.create', 'ref': 'floor', 'name': 'Floor', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(0, -0.1, 0), 'LocalScale': vec(4, 0.1, 4)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.create', 'ref': 'quad', 'name': 'Quad', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(0, 1.2, 0)}},
            {'type': 'ModelRenderer', 'properties': {'Model': {'id': solid_model}}}]},
        {'op': 'entity.create', 'ref': 'sun', 'name': 'Sun', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(1, 6, 0.5)}},
            {'type': 'Light', 'properties': {'Type': 'Directional', 'Intensity': 2.0}}]},
        {'op': 'entity.aim', 'target': {'ref': 'sun'}, 'point': vec(0, 0, 0),
         'forwardAxis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0)},
    ])
    quad = created['refs']['quad']

    # ------------------------------------------------------------------ directional light
    #
    # The cascades are rebuilt every frame, so each capture reflects the edit before it.

    update_renderer(quad, CastShadows=False)
    unshadowed = render('no-caster')

    update_renderer(quad, CastShadows=True)
    solid_shadow = shadow_pixels(unshadowed, render('solid'), 'solid quad')

    # A unit quad seen from about seven units away covers a few hundred pixels of floor. The upper
    # bound is what separates a shadow from the whole floor changing brightness.
    assert 150 < solid_shadow < FRAME // 8, solid_shadow

    update_renderer(quad, Model={'id': cutout_model})
    cutout_shadow = shadow_pixels(unshadowed, render('cutout'), 'cutout quad')

    # Half the quad is clear, so half the shadow is gone. Without the alpha test in the shadow
    # pass the cutout casts the whole quad, and the two counts come out equal.
    ratio = cutout_shadow / solid_shadow
    print('cutout / solid shadow: %.2f' % ratio, flush=True)
    assert 0.3 < ratio < 0.7, ratio

    # Switching back restores the full shadow, so the difference above belongs to the material
    # rather than to anything the first swap left behind.
    update_renderer(quad, Model={'id': solid_model})
    restored = shadow_pixels(unshadowed, render('solid-again'), 'solid quad again')
    assert abs(restored - solid_shadow) < 10, (restored, solid_shadow)

    errors = [line for line in log_path.read_text().splitlines() if '[error]' in line or '[fatal]' in line]
    assert not errors, errors

    print('Cutout shadows verification passed. Captures in', args.output, flush=True)
finally:
    shutdown()

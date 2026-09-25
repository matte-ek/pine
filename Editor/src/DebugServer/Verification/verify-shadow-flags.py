"""Check ModelRenderer's CastShadows and ReceiveShadows on the rendered frame; needs an Editor build and Xvfb."""

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
parser.add_argument('--port', type=int, default=19054)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-shadow-flags-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-shadow-flags.'))
url = 'http://127.0.0.1:%d' % args.port
print('Shadow flags verification:', root, flush=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Timestamps are
# preserved so the engine assets are not all re-imported on boot.
data = root / 'data'
(data / 'projects/shadowflags/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'editor.log'
log = log_path.open('w')
process = subprocess.Popen(headless_command(build / 'Editor/Editor', 'shadowflags'), cwd=data,
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


def renderer(entity_id):
    entity = fetch('/entity?id=' + entity_id)
    return next(component['properties'] for component in entity['components'] if component['type'] == 'ModelRenderer')


def set_flags(entity_id, **flags):
    edit([{'op': 'component.update', 'target': {'id': component_id(entity_id, 'ModelRenderer')},
           'properties': flags}])


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


def compare(before, after):
    """How many pixels got brighter, and how many got darker, going from 'before' to 'after'."""
    brighter = darker = 0
    for row_before, row_after in zip(before, after):
        for pixel_before, pixel_after in zip(row_before, row_after):
            delta = luminance(pixel_after) - luminance(pixel_before)
            if delta > CHANGE:
                brighter += 1
            elif delta < -CHANGE:
                darker += 1
    return brighter, darker


FRAME = 240 * 180


def expect_shadow_lifted(before, after, label):
    """'after' is 'before' with the caster's shadow gone: a shadow-sized patch brightens, nothing darkens."""
    brighter, darker = compare(before, after)
    print('%s: %d pixels brighter, %d darker' % (label, brighter, darker), flush=True)

    # A caster roughly a unit across seen from about seven units away: a few hundred pixels. The
    # upper bound is what separates a shadow from the whole floor changing brightness.
    assert 80 < brighter < FRAME // 8, (label, brighter)
    assert darker < 10, (label, darker)
    return brighter


def expect_same(first, second, label):
    brighter, darker = compare(first, second)
    print('%s: %d pixels brighter, %d darker' % (label, brighter, darker), flush=True)
    assert brighter < 10 and darker < 10, (label, brighter, darker)


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

    # ------------------------------------------------------------------ the schema

    schema = fetch('/edit/schema')
    model_renderer = schema['components']['ModelRenderer']
    for flag in ['CastShadows', 'ReceiveShadows']:
        assert model_renderer['properties'][flag]['type'] == 'boolean', model_renderer['properties'][flag]
        assert model_renderer['defaults'][flag] is True, model_renderer['defaults']

    # Grain is animated, and would make two captures of an unchanged scene differ.
    fetch('/level/settings', {'properties': {'GrainStrength': 0.0}})

    existing_lights = fetch('/entities/query', {'filter': {'component': 'Light'}})['entities']
    assert not existing_lights, 'The fresh level already has lights, which would muddy every capture'

    # ------------------------------------------------------------------ the scene

    cube = {'path': 'engine/primitive/cube'}
    created = edit([
        {'op': 'entity.create', 'ref': 'floor', 'name': 'Floor', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(0, -0.1, 0), 'LocalScale': vec(4, 0.1, 4)}},
            {'type': 'ModelRenderer', 'properties': {'Model': cube}}]},
        {'op': 'entity.create', 'ref': 'caster', 'name': 'Caster', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(0, 1.2, 0), 'LocalScale': vec(0.5, 0.5, 0.5)}},
            {'type': 'ModelRenderer', 'properties': {'Model': cube}}]},
        {'op': 'entity.create', 'ref': 'sun', 'name': 'Sun', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(2, 6, 1)}},
            {'type': 'Light', 'properties': {'Type': 'Directional', 'Intensity': 2.0}}]},
        {'op': 'entity.aim', 'target': {'ref': 'sun'}, 'point': vec(0, 0, 0),
         'forwardAxis': '-Z', 'upAxis': '+Y', 'up': vec(0, 1, 0)},
    ])
    floor, caster, sun = (created['refs'][name] for name in ['floor', 'caster', 'sun'])

    # Both flags default on, for a new renderer and for one saved before they existed.
    assert renderer(caster)['CastShadows'] is True and renderer(caster)['ReceiveShadows'] is True

    # ------------------------------------------------------------------ directional light
    #
    # The cascades are rebuilt every frame, so this is the uncached path.

    shadowed = render('sun-shadowed')

    set_flags(caster, CastShadows=False)
    no_cast = render('sun-caster-off')
    expect_shadow_lifted(shadowed, no_cast, 'sun, caster stops casting')

    set_flags(caster, CastShadows=True)
    expect_same(shadowed, render('sun-caster-on'), 'sun, caster casts again')

    set_flags(floor, ReceiveShadows=False)
    no_receive = render('sun-floor-not-receiving')
    expect_shadow_lifted(shadowed, no_receive, 'sun, floor stops receiving')

    # Casting and receiving are independent: the floor ignoring shadows looks the same whether or
    # not anything is casting onto it.
    set_flags(caster, CastShadows=False)
    expect_same(no_receive, render('sun-neither'), 'sun, floor not receiving, caster not casting')

    set_flags(caster, CastShadows=True)
    set_flags(floor, ReceiveShadows=True)
    expect_same(shadowed, render('sun-restored'), 'sun, both restored')

    # ------------------------------------------------------------------ point light
    #
    # Local lights keep their shadow tiles between frames and only re-render one when something in
    # it changed. Nothing moves in this phase, so a flag that did not invalidate the tile would
    # leave the old shadow standing, and these comparisons would come out equal.

    edit([{'op': 'component.remove', 'target': {'id': component_id(sun, 'Light')}}])
    edit([{'op': 'entity.create', 'ref': 'lamp', 'name': 'Lamp', 'components': [
        {'type': 'Transform', 'properties': {'LocalPosition': vec(0.3, 3.5, 0.2)}},
        {'type': 'Light', 'properties': {'Type': 'PointLight', 'Intensity': 30.0, 'Range': 15.0}}]}])

    # Several frames, so the lamp has won its tiles and faded its shadow fully in.
    for _ in range(4):
        render('lamp-settling')
    lamp_shadowed = render('lamp-shadowed')

    set_flags(caster, CastShadows=False)
    render('lamp-caster-off-settling')
    lamp_no_cast = render('lamp-caster-off')
    expect_shadow_lifted(lamp_shadowed, lamp_no_cast, 'lamp, caster stops casting')

    set_flags(caster, CastShadows=True)
    render('lamp-caster-on-settling')
    expect_same(lamp_shadowed, render('lamp-caster-on'), 'lamp, caster casts again')

    set_flags(floor, ReceiveShadows=False)
    expect_shadow_lifted(lamp_shadowed, render('lamp-floor-not-receiving'), 'lamp, floor stops receiving')

    # ------------------------------------------------------------------ persistence

    fetch('/level/save-as', {'path': 'levels/shadow-flags'})
    fetch('/level/load', {'path': 'levels/shadow-flags'})

    reloaded = {entity['name']: entity['id'] for entity in fetch('/entities/query', {'filter': {'component': 'ModelRenderer'}})['entities']}
    assert renderer(reloaded['Floor'])['ReceiveShadows'] is False, renderer(reloaded['Floor'])
    assert renderer(reloaded['Floor'])['CastShadows'] is True, renderer(reloaded['Floor'])
    assert renderer(reloaded['Caster'])['CastShadows'] is True, renderer(reloaded['Caster'])

    set_flags(reloaded['Caster'], CastShadows=False)
    fetch('/level/save', {})
    fetch('/level/load', {'path': 'levels/shadow-flags'})

    reloaded = {entity['name']: entity['id'] for entity in fetch('/entities/query', {'filter': {'component': 'ModelRenderer'}})['entities']}
    assert renderer(reloaded['Caster'])['CastShadows'] is False, renderer(reloaded['Caster'])
    assert renderer(reloaded['Caster'])['ReceiveShadows'] is True, renderer(reloaded['Caster'])

    errors = [line for line in log_path.read_text().splitlines() if '[error]' in line or '[fatal]' in line]
    assert not errors, errors

    print('Shadow flags verification passed. Captures in', args.output, flush=True)
finally:
    shutdown()

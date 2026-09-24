"""Build a native terrain probe, then check the four blended layers; requires a Ninja Editor build and Xvfb."""

import argparse
import base64
import json
import os
from pathlib import Path
import shlex
import shutil
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
parser.add_argument('--port', type=int, default=19037)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-terrain-layer-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-terrain-layers.'))
url = 'http://127.0.0.1:%d' % args.port
print('Terrain layer verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# the probe: scene setup /edit cannot express, then the same Run() so the debug server still serves.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-layers.inc').read_text()
(root / 'probe.cpp').write_text(includes + source.replace(marker, body))
command = shlex.split(entry['command'])
command[command.index('-o') + 1] = str(root / 'probe.o')
command[-1] = str(root / 'probe.cpp')
subprocess.run(command, cwd=build, check=True)

link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'Editor'], text=True).splitlines()[-1]
command = shlex.split(link.removeprefix(': && ').removesuffix(' && :'))
command[command.index('Editor/CMakeFiles/Editor.dir/src/Application.cpp.o')] = str(root / 'probe.o')
command[command.index('-o') + 1] = str(root / 'probe')
subprocess.run(command, cwd=build, check=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Copied with cp -a to
# keep timestamps: anything that looks newer than its source triggers a hot-reload re-import of
# every engine asset. A panel layout is required - without one the Level viewport is never laid out
# and every capture endpoint answers 409.
#
# The layout comes from this directory rather than from data/imgui.ini, because the editor rewrites
# that file every time a person runs it: resizing a dock while trying something out would otherwise
# change the size of the viewport these scripts measure, and a check on how much of the frame the
# terrain fills would start failing for a reason that has nothing to do with the terrain.
data = root / 'data'
(data / 'projects/terrain/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'terrain.log'
log = log_path.open('w')
process = subprocess.Popen(headless_command(root / 'probe', 'terrain'), cwd=data,
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


def fail(message):
    shutdown()
    log.close()
    print(log_path.read_text()[-4000:])
    raise SystemExit('FAIL: ' + message)


def request(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url + path, data=data, headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(req, timeout=20)
    except urllib.error.HTTPError as error:
        fail('%s -> %d %s' % (path, error.code, error.read().decode()[:400]))
    payload = response.read()
    return payload if response.headers.get('Content-Type') == 'image/png' else json.loads(payload)


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

                # Ties break towards left, then up, then corner. Picking the smallest *value*
                # instead - which sorting the three candidates would do - decodes almost every row
                # correctly and then drifts, because the average filter on the rows below carries
                # the error forward.
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


# The terrain the probe built: 4 chunks of 16 quads at 16 units, so a 65x65 sample field spanning
# 64 world units, with one sample per unit.
FIELD = 65
EXTENT = 64.0

# The diffuse colour of each layer, matching the materials the probe assigns. Layer 3 is white on
# purpose: three primaries and one neutral is the set that lets every pair of layers be told apart
# by which channel is brighter.
LAYER_COLORS = [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 1.0, 1.0)]


def expected_weights(x, z):
    """The bilinear field the probe painted, at a terrain-local point."""
    u, v = x / EXTENT, z / EXTENT
    return [(1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v]


def expected_color(x, z):
    """What the blend of those weights should come out as, before lighting and tone mapping."""
    weights = expected_weights(x, z)
    return [sum(weights[layer] * LAYER_COLORS[layer][channel] for layer in range(4))
            for channel in range(3)]


def transform(matrix, vector):
    """Applies a matrix given as four columns of four, which is how /observe reports them."""
    return [sum(matrix[column][row] * vector[column] for column in range(4)) for row in range(4)]


def project(camera, width, height, point):
    """The pixel a terrain-local point lands on, using the camera the frame was actually drawn with.

    Derived from the reported matrices rather than assumed from the camera placement: the whole
    reason for probing named points is to catch a splat lookup that is offset or mirrored, and a
    hand-guessed screen mapping could hide exactly that.
    """
    clip = transform(camera['projectionMatrix'], transform(camera['viewMatrix'], [point[0], point[1], point[2], 1.0]))

    if clip[3] <= 0:
        fail('a probe point at %s projected behind the camera' % (point,))

    ndc = [clip[0] / clip[3], clip[1] / clip[3]]

    return int((ndc[0] * 0.5 + 0.5) * width), int((1.0 - (ndc[1] * 0.5 + 0.5)) * height)


def sample_block(rows, width, height, pixel, radius=6):
    """Mean colour of a small block, which is what removes the post-process grain."""
    x, y = pixel

    if not (radius <= x < width - radius and radius <= y < height - radius):
        fail('a probe point landed at %s, outside the captured frame' % (pixel,))

    points = [rows[y + dy][x + dx]
              for dy in range(-radius, radius + 1)
              for dx in range(-radius, radius + 1)]

    return [sum(point[channel] for point in points) / float(len(points)) for channel in range(3)]


try:
    deadline = time.time() + 120
    while True:
        if process.poll() is not None:
            fail('the probe exited before serving; its native checks are in the log')
        try:
            urllib.request.urlopen(url + '/status', timeout=5).read()
            break
        except (urllib.error.URLError, OSError):
            if time.time() > deadline:
                fail('the debug server never came up')
            time.sleep(1)

    if 'PASS(native)' not in log_path.read_text():
        fail('the probe served without reporting its native checks')

    terrain = request('/terrain?path=ground')

    if len(terrain['layers']) != 4:
        fail('expected four layer slots, got %d' % len(terrain['layers']))

    for layer, path in enumerate(terrain['layers']):
        if path is None or not path.endswith('layer-%d' % layer):
            fail('layer slot %d holds %r rather than its own material' % (layer, path))

    # Nothing asked for it: the render path is what notices the weight field has changed and
    # uploads it, in Prepare rather than mid-pass.
    if not terrain['splatMapReady']:
        fail('the render path never built the splat texture')

    # The stored weights, read back through the server at points spread over the terrain. This is
    # the exact half of the check - what the picture below can only show approximately.
    for x, z in [(0, 0), (64, 0), (0, 64), (64, 64), (32, 32), (17, 41)]:
        reported = request('/terrain?path=ground&x=%d&z=%d' % (x, z))['layerWeights']

        if reported['sample'] != {'x': x, 'z': z}:
            fail('a weight query at (%d, %d) answered for sample %s' % (x, z, reported['sample']))

        for layer, (actual, wanted) in enumerate(zip(reported['weights'], expected_weights(x, z))):
            if abs(actual - wanted) > 0.01:
                fail('layer %d at (%d, %d) stored %.3f, expected %.3f' % (layer, x, z, actual, wanted))

    # Steep and close, but tilted off vertical: a look-at straight down has no well-defined up
    # vector. The tilt is why the projection below is taken from the frame's own matrices.
    centre = {'x': EXTENT / 2, 'y': 0, 'z': EXTENT / 2}
    moved = request('/camera', {'position': {'x': centre['x'], 'y': EXTENT * 0.95, 'z': centre['z'] + EXTENT * 0.12},
                                'lookAt': centre, 'farPlane': EXTENT * 8})

    observation = request('/observe', {'after': moved['observationToken'], 'view': 'level', 'width': 900})

    image = base64.b64decode(observation['image']['data'])
    (args.output / 'blend.png').write_bytes(image)
    width, height, rows = decode_png(image)

    camera = observation['camera']

    # Inside the terrain rather than on its rim, so that no probe block can catch the silhouette,
    # and far enough apart that the weights at any two of them differ substantially.
    corners = [(EXTENT * 0.2, EXTENT * 0.2), (EXTENT * 0.8, EXTENT * 0.2),
               (EXTENT * 0.2, EXTENT * 0.8), (EXTENT * 0.8, EXTENT * 0.8),
               (EXTENT * 0.5, EXTENT * 0.5)]

    measured = {}

    for x, z in corners:
        pixel = project(camera, width, height, [x, 0.0, z])
        measured[(x, z)] = sample_block(rows, width, height, pixel)

    # Compared as an ordering of channels rather than as colours. Everything between the blend and
    # the pixel - the light, the ambient term, the sRGB encode, the vignette - is monotonic per
    # channel and very nearly the same for all three, so which channel is brighter survives it all
    # while the actual values do not.
    #
    # A terrain that ignored the splat map entirely would carry all four layers at equal weight
    # everywhere and come out grey, so every one of these comparisons would fail - which is what
    # keeps the check from passing on a picture of nothing in particular.
    for (x, z), actual in measured.items():
        wanted = expected_color(x, z)

        for bright in range(3):
            for dim in range(3):
                if wanted[bright] <= wanted[dim] * 1.25:
                    continue

                if actual[bright] <= actual[dim]:
                    fail('at (%.0f, %.0f) the blend wanted channel %d above channel %d '
                         '(expected %s), but the frame shows %s'
                         % (x, z, bright, dim, ['%.2f' % value for value in wanted],
                            ['%.0f' % value for value in actual]))

    # Two points inside the same chunk - chunk (1, 1) covers 16..32 on both axes - which is what
    # "four layers blend across a chunk" actually asks for. Per-chunk weights, or a splat lookup
    # that resolved to one value per chunk, would make these two identical.
    near, far = (17.0, 17.0), (31.0, 31.0)

    inside = {point: sample_block(rows, width, height, project(camera, width, height, [point[0], 0.0, point[1]]))
              for point in (near, far)}

    # Layer 0 (red) loses about half its share between the two, and layers 1 and 2 (green, blue)
    # gain, so red's lead over the other two has to shrink. Measured on this scene: about 45 points
    # of lead at the near point against about 5 at the far one.
    def red_lead(color):
        return color[0] - max(color[1], color[2])

    if red_lead(inside[near]) - red_lead(inside[far]) < 15:
        fail('the blend barely changed within one chunk: red leads by %.1f at %s and %.1f at %s'
             % (red_lead(inside[near]), near, red_lead(inside[far]), far))

    print('PASS: native weight field, splat texture built by the render path, stored weights over '
          'HTTP,\n      four layer colours landing where the field puts them, and the blend '
          'varying within one chunk.')
    print('Inspect blend.png in', args.output)
finally:
    shutdown()
    log.close()

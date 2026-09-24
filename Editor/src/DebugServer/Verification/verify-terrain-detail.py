"""Build a native terrain probe, then check terrain detail placement and drawing; requires a Ninja Editor build and Xvfb."""

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
parser.add_argument('--port', type=int, default=19041)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-terrain-detail-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-terrain-detail.'))
url = 'http://127.0.0.1:%d' % args.port
print('Terrain detail verification:', root, flush=True)

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
#include <vector>
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-detail.inc').read_text()
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


# The terrain the probe built: 4 chunks of 16 quads at 16 units, one sample per unit, with the
# x < 32 half painted into the layer the magenta detail grows on.
EXTENT = 64.0


def transform(matrix, vector):
    """Applies a matrix given as four columns of four, which is how /observe reports them."""
    return [sum(matrix[column][row] * vector[column] for column in range(4)) for row in range(4)]


def project(camera, width, height, point):
    """The pixel a world point lands on, using the camera the frame was actually drawn with."""
    clip = transform(camera['projectionMatrix'], transform(camera['viewMatrix'], [point[0], point[1], point[2], 1.0]))

    if clip[3] <= 0:
        fail('a probe point at %s projected behind the camera' % (point,))

    ndc = [clip[0] / clip[3], clip[1] / clip[3]]

    return int((ndc[0] * 0.5 + 0.5) * width), int((1.0 - (ndc[1] * 0.5 + 0.5)) * height)


def is_magenta(color):
    """The detail material is pure magenta and the ground is a dark grey, so red and blue well
    above green is detail and nothing else in the frame."""
    return color[0] > color[1] + 40 and color[2] > color[1] + 40


def block_mean(rows, width, height, pixel, radius=1):
    """Mean colour of a small block, which is what removes the post-process grain."""
    x, y = pixel

    if not (radius <= x < width - radius and radius <= y < height - radius):
        fail('a probe point landed at %s, outside the captured frame' % (pixel,))

    points = [rows[y + dy][x + dx]
              for dy in range(-radius, radius + 1)
              for dx in range(-radius, radius + 1)]

    return [sum(point[channel] for point in points) / float(len(points)) for channel in range(3)]


def detail_instances():
    return request('/stats')['level']['terrainDetailInstances']


def wait_for_instances(expected, what):
    """Placements are generated a few batches per frame, so a change can take several frames to
    show up in the counters. An observation with no token waits for a frame after it was accepted."""
    for _ in range(30):
        request('/observe', {'view': 'level', 'width': 64})

        if detail_instances() == expected:
            return

    fail('%s: the scene pass drew %d detail instances, expected %d' % (what, detail_instances(), expected))


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

    probe_output = log_path.read_text()

    if 'PASS(native)' not in probe_output:
        fail('the probe served without reporting its native checks')

    total = None
    samples = []

    for line in probe_output.splitlines():
        fields = line.split()

        if fields[:1] == ['DETAIL_TOTAL']:
            total = int(fields[1])
        elif fields[:1] == ['DETAIL_SAMPLE']:
            samples.append([float(value) for value in fields[1:4]])

    if total is None or len(samples) != 8:
        fail('the probe did not print the placements this script checks against')

    terrain = request('/terrain?path=ground')
    detail_types = terrain['detailTypes']

    if len(detail_types) != 1:
        fail('expected one detail type over HTTP, got %d' % len(detail_types))

    detail = detail_types[0]

    if detail['model'] != 'engine/primitive/cube' or detail['layer'] != 1 or abs(detail['density'] - 0.1) > 1e-6 \
            or detail['scaleMin'] != detail['scaleMax'] or detail['drawDistance'] != 150:
        fail('/terrain reported the detail type as %s' % detail)

    # Steep and close, over the middle, tilted off vertical so the look-at has an up vector.
    centre = {'x': EXTENT / 2, 'y': 0, 'z': EXTENT / 2}
    overview = {'position': {'x': centre['x'], 'y': EXTENT * 0.95, 'z': centre['z'] + EXTENT * 0.12},
                'lookAt': centre, 'farPlane': EXTENT * 8}

    request('/camera', overview)

    # Every chunk is within the draw distance and in view, so every placement the terrain
    # generates has to be drawn - no more, which would be a chunk drawn twice or a stale batch,
    # and no fewer, which would be a batch never generated or wrongly culled.
    wait_for_instances(total, 'looking at the whole terrain')

    moved = request('/camera', overview)
    observation = request('/observe', {'after': moved['observationToken'], 'view': 'level', 'width': 900})

    image = base64.b64decode(observation['image']['data'])
    (args.output / 'detail.png').write_bytes(image)
    width, height, rows = decode_png(image)

    camera = observation['camera']

    # Named placements, projected with the frame's own matrices: this is what catches a placement
    # read from the wrong entry, a transform applied twice, or copies drawn at the terrain origin.
    for sample in samples:
        pixel = project(camera, width, height, sample)
        colour = block_mean(rows, width, height, pixel)

        if not is_magenta(colour):
            fail('no detail at the placement %s (pixel %s shows %s)'
                 % (sample, pixel, ['%.0f' % value for value in colour]))

    # A grid of ground points over each half. Detail grows on the painted half and nowhere on the
    # other, so magenta there would be copies drawn where the layer is not.
    def magenta_share(x_range):
        points = [(x + 0.5, z + 0.5) for x in range(*x_range) for z in range(6, 58)]
        hits = sum(1 for x, z in points if is_magenta(block_mean(rows, width, height,
                                                                 project(camera, width, height, [x, 0.0, z]), 0)))

        return hits / float(len(points))

    painted, bare = magenta_share((4, 27)), magenta_share((38, 60))

    if painted < 0.05:
        fail('only %.1f%% of the painted half shows detail' % (100 * painted))

    if bare > 0.002:
        fail('%.1f%% of the unpainted half shows detail' % (100 * bare))

    # Far above the terrain, every chunk is past the draw distance: nothing is drawn, and the
    # placements are released.
    request('/camera', {'position': {'x': centre['x'], 'y': 1000.0, 'z': centre['z'] + 1.0},
                        'lookAt': centre, 'farPlane': 5000.0})

    wait_for_instances(0, 'far past the draw distance')

    # And back: the released placements have to be generated again, identically.
    request('/camera', overview)

    wait_for_instances(total, 'after coming back within the draw distance')

    # A brush stroke through the editor's own tool, painting the detail's layer onto the bare half.
    # The brush writes weight rectangles, so this is the path an author actually takes: the stroke
    # has to restamp the chunks it covers and the renderer has to notice and regenerate them.
    stroke = request('/terrain/sculpt?path=ground', {'mode': 'paint', 'layer': 1, 'x': 48.0, 'z': 32.0,
                                                     'radius': 8.0, 'falloff': 0.0, 'strength': 1000.0,
                                                     'duration': 1.0})

    if stroke['appliedPoints'] != 1:
        fail('the paint stroke did not land on the terrain: %s' % stroke)

    # About 200 square units at 0.1 per unit, so ~20 more; a few less is chance, none is a failure
    # to regenerate.
    for _ in range(30):
        request('/observe', {'view': 'level', 'width': 64})

        if detail_instances() >= total + 10:
            break
    else:
        fail('painting the detail layer onto bare ground grew %d instances there'
             % (detail_instances() - total))

    painted_total = detail_instances()

    # Undo restores the weight bytes exactly, and placement is deterministic, so the count has to
    # come back to exactly what it was - not merely close to it.
    request('/history/undo', {})

    wait_for_instances(total, 'after undoing the paint stroke')

    print('PASS: native placement and revisions, detail types over HTTP, %d placements drawn where '
          'the probe put them,\n      only on the painted half (%.1f%% of it covered, %.2f%% of the bare '
          'half),\n      released past the draw distance and regenerated on return, and a paint stroke '
          'grew %d more\n      that its undo took away again exactly.'
          % (total, 100 * painted, 100 * bare, painted_total - total))
    print('Inspect detail.png in', args.output)
finally:
    shutdown()
    log.close()

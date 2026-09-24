"""Build a native terrain probe, then check the sculpting brush and its undo; requires a Ninja Editor build and Xvfb."""

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
parser.add_argument('--port', type=int, default=19038)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-terrain-sculpt-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-terrain-sculpt.'))
url = 'http://127.0.0.1:%d' % args.port
print('Terrain sculpt verification:', root, flush=True)

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
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Shader/Importer/ShaderImporter.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Rendering/Features/Terrain/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Other/TerrainSculpting/TerrainSculpting.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-sculpt.inc').read_text()
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

                # Ties break towards left, then up, then corner.
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


def request(path, body=None, expected=200):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url + path, data=data, headers={'Content-Type': 'application/json'})
    try:
        response = urllib.request.urlopen(req, timeout=20)
    except urllib.error.HTTPError as error:
        if error.code == expected:
            return json.loads(error.read())
        fail('%s -> %d %s' % (path, error.code, error.read().decode()[:400]))
    if response.code != expected:
        fail('%s answered %d, expected %d' % (path, response.code, expected))
    return json.loads(response.read())


# The terrain the probe built: 4 chunks of 16 quads at 16 units, so a 65x65 sample field spanning
# 64 world units at one sample per unit, flat at height 0.
EXTENT = 64.0
CENTRE = (32.0, 32.0)


def height_at(x, z):
    """The interpolated surface height at a terrain-local point."""
    answer = request('/terrain?path=ground&x=%.4f&z=%.4f' % (x, z))['height']['y']
    if answer is None:
        fail('the terrain reports no height at (%.2f, %.2f)' % (x, z))
    return answer


def profile(points):
    return [height_at(x, z) for x, z in points]


def weights_at(x, z):
    """The layer weights stored at the sample nearest a terrain-local point."""
    answer = request('/terrain?path=ground&x=%.4f&z=%.4f' % (x, z)).get('layerWeights')
    if answer is None:
        fail('the terrain reports no layer weights at (%.2f, %.2f)' % (x, z))
    return answer['weights']


def blend(points):
    return [weights_at(x, z) for x, z in points]


def sculpt(expected=200, **fields):
    return request('/terrain/sculpt?path=ground', fields, expected)


def undo():
    return request('/history/undo', {})


def redo():
    return request('/history/redo', {})


def undo_count():
    return request('/history')['undoCount']


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

    # Points spread from the brush centre outwards, plus one well outside any brush used below.
    # Sampled as a group before and after every stroke, which is what makes "this moved and that did
    # not" a single comparison rather than a series of separate ones.
    # In order: the centre, then 4, 8 and 12 units out along x, then 8 units out along z - the
    # mirror of the third - and finally a point right across the terrain.
    probes = [CENTRE, (36.0, 32.0), (40.0, 32.0), (44.0, 32.0), (32.0, 40.0), (8.0, 8.0)]

    flat = profile(probes)

    if any(abs(value) > 0.001 for value in flat):
        fail('the probe terrain did not start flat: %s' % flat)

    if undo_count() != 0:
        fail('the history was not empty before the first stroke')

    # A single raise dab with the falloff turned all the way up, which makes the whole radius the
    # soft edge - a dome peaking under the cursor. The centre has to rise, each ring further out has
    # to rise less, and everything past the radius has to be untouched.
    raised = sculpt(mode='raise', x=CENTRE[0], z=CENTRE[1], radius=10.0, strength=20.0,
                    falloff=1.0, duration=0.5)

    if raised['appliedPoints'] != 1:
        fail('a dab on the terrain reported %d applied points' % raised['appliedPoints'])

    after_raise = profile(probes)

    if after_raise[0] <= flat[0] + 1.0:
        fail('a raise stroke moved the centre by %.3f' % (after_raise[0] - flat[0]))

    if not after_raise[0] > after_raise[1] > after_raise[2] > after_raise[3]:
        fail('a fully soft brush does not fall off from its centre: %s' % after_raise[:4])

    if abs(after_raise[3] - flat[3]) > 0.001:
        fail('a point 12 units from a 10 unit brush moved by %.4f' % (after_raise[3] - flat[3]))

    if abs(after_raise[5] - flat[5]) > 0.001:
        fail('a point across the terrain moved by %.4f' % (after_raise[5] - flat[5]))

    # The brush is round, not square: two points the same distance from the centre have to match,
    # and the corner of the rectangle the brush reads has to be untouched.
    if abs(after_raise[2] - after_raise[4]) > 0.001:
        fail('the brush is not radially symmetric: %.4f eight units along x against %.4f along z'
             % (after_raise[2], after_raise[4]))

    corner = height_at(CENTRE[0] + 8.0, CENTRE[1] + 8.0)

    if abs(corner) > 0.001:
        fail('the corner of the brush rectangle, 11.3 units out, moved by %.4f' % corner)

    # Falloff is measured inwards from the rim, so turning it down leaves the middle of the brush at
    # full strength rather than weakening the whole thing. At 0 the brush is a hard-edged stamp: two
    # points well inside it have to move by exactly the same amount.
    undo()

    sculpt(mode='raise', x=CENTRE[0], z=CENTRE[1], radius=10.0, strength=20.0, falloff=0.0, duration=0.5)

    stamped = profile(probes)

    if abs(stamped[0] - stamped[2]) > 0.001:
        fail('a hard-edged brush is not flat topped: %.4f at the centre against %.4f 8 units out'
             % (stamped[0], stamped[2]))

    if abs(stamped[3] - flat[3]) > 0.001:
        fail('a hard-edged brush reached past its radius by %.4f' % (stamped[3] - flat[3]))

    undo()

    sculpt(mode='raise', x=CENTRE[0], z=CENTRE[1], radius=10.0, strength=20.0, falloff=1.0, duration=0.5)

    if profile(probes) != after_raise:
        fail('repeating the first stroke on restored ground gave a different result')

    # One stroke is one undo step, and undoing it puts the ground back exactly - not approximately.
    if undo_count() != 1:
        fail('one stroke left %d undo steps' % undo_count())

    undo()

    restored = profile(probes)

    if restored != flat:
        fail('undo did not restore the ground exactly: %s against %s' % (restored, flat))

    redo()

    if profile(probes) != after_raise:
        fail('redo did not put the stroke back exactly')

    # A drag: many points, one undo step. This is also what grows the stroke's recorded rectangle,
    # which is the part of the brush most likely to record the wrong "before" - the samples the
    # first points moved must not be re-read when a later point extends the region.
    before_drag = profile(probes)
    steps = 24
    drag = [{'x': 12.0 + (EXTENT - 24.0) * step / (steps - 1), 'z': 20.0} for step in range(steps)]

    dragged = sculpt(mode='raise', points=drag, radius=6.0, strength=15.0, falloff=0.6, duration=0.2)

    if dragged['appliedPoints'] != steps:
        fail('a drag of %d points applied %d of them' % (steps, dragged['appliedPoints']))

    if undo_count() != 2:
        fail('a %d point drag left %d undo steps, expected 2' % (steps, undo_count()))

    ridge = [height_at(x, 20.0) for x in (12.0, 24.0, 36.0, 48.0)]

    if any(value < 1.0 for value in ridge):
        fail('a drag did not raise ground along its whole length: %s' % ridge)

    undo()

    after_drag_undo = profile(probes)

    if after_drag_undo != before_drag:
        fail('undoing a drag did not restore the ground exactly')

    if any(abs(height_at(x, 20.0)) > 0.001 for x in (12.0, 24.0, 36.0, 48.0)):
        fail('undoing a drag left part of the ridge behind')

    redo()

    # Lower, on the mound the first stroke raised, so it is measured against ground that is not flat.
    before_lower = profile(probes)

    sculpt(mode='lower', x=CENTRE[0], z=CENTRE[1], radius=10.0, strength=10.0, falloff=0.5, duration=0.5)

    after_lower = profile(probes)

    if after_lower[0] >= before_lower[0] - 1.0:
        fail('a lower stroke moved the centre by %.3f' % (after_lower[0] - before_lower[0]))

    undo()

    # Flatten, to a height the request names, so the expected answer needs nothing recomputed.
    sculpt(mode='flatten', x=CENTRE[0], z=CENTRE[1], radius=12.0, strength=40.0, falloff=0.0,
           duration=1.0, height=3.0)

    levelled = [height_at(CENTRE[0] + offset, CENTRE[1]) for offset in (-6.0, -2.0, 2.0, 6.0)]

    if any(abs(value - 3.0) > 0.05 for value in levelled):
        fail('flatten to height 3 left %s' % levelled)

    undo()

    # Smooth, on a corner of the terrain no other stroke has touched, so the result is not measured
    # against ground the earlier strokes left uneven.
    #
    # The spike is deliberately one sample wide - a radius of exactly one unit on a field with one
    # sample per unit reaches nothing but the sample under it. A wider one would be a plateau as far
    # as a three-by-three average is concerned, and only its edges would move.
    quiet = (52.0, 50.0)
    quiet_neighbour = (53.0, 50.0)

    if abs(height_at(*quiet)) > 0.001 or abs(height_at(*quiet_neighbour)) > 0.001:
        fail('the corner reserved for the smooth check is not flat')

    sculpt(mode='raise', x=quiet[0], z=quiet[1], radius=1.0, strength=40.0, falloff=0.0, duration=0.5)

    spike = height_at(*quiet)

    if spike < 10.0:
        fail('the spike a smooth stroke needs only reached %.3f' % spike)

    if abs(height_at(*quiet_neighbour)) > 0.001:
        fail('a one unit brush reached the sample next to it')

    sculpt(mode='smooth', x=quiet[0], z=quiet[1], radius=4.0, strength=40.0, falloff=0.3, duration=1.0)

    smoothed = height_at(*quiet)
    spread = height_at(*quiet_neighbour)

    # Smoothing is not lowering: the peak comes down and the ground around it comes up to meet it.
    # A brush that only pushed samples downwards would pass the first of these and fail the second.
    if smoothed >= spike * 0.5:
        fail('smoothing moved a %.2f unit spike only to %.2f' % (spike, smoothed))

    if smoothed <= 0.0:
        fail('smoothing cut through the spike to %.3f rather than levelling it' % smoothed)

    if spread <= 0.1:
        fail('smoothing lowered the spike without raising the ground beside it, which reads %.3f' % spread)

    # A stroke entirely off the terrain changes nothing and records nothing - the normal case for a
    # drag that has left the edge, which must not consume the author's next undo.
    steps_before = undo_count()
    away = sculpt(mode='raise', x=EXTENT + 50.0, z=EXTENT + 50.0, radius=4.0, strength=10.0, duration=0.5)

    if away['appliedPoints'] != 0:
        fail('a stroke off the terrain applied %d points' % away['appliedPoints'])

    if undo_count() != steps_before:
        fail('a stroke that changed nothing added an undo step')

    # Rejections, checked because the endpoint is the one part of this a person drives by hand.
    sculpt(expected=400, mode='sideways', x=1.0, z=1.0)
    sculpt(expected=400, mode='raise', x=1.0, z=1.0, radius=0.0)
    sculpt(expected=400, mode='raise', x=1.0, z=1.0, duration=0.0)
    sculpt(expected=400, mode='raise', points=[])
    request('/terrain/sculpt?path=ground', {'mode': 'raise'}, 400)
    request('/terrain/sculpt?path=engine/materials/default', {'x': 0, 'z': 0}, 400)
    request('/terrain/sculpt?path=nothing/here', {'x': 0, 'z': 0}, 404)

    # Painting, which is the same brush writing layer weights instead of heights. Done on a corner
    # none of the strokes above reached, so the ground under it is flat and the picture below shows
    # the blend rather than the shading.
    #
    # In order: the centre, then 4, 8 and 12 units out along x, then 8 units out along z, and
    # finally a point right across the terrain.
    PAINT_CENTRE = (12.0, 52.0)
    paint_probes = [PAINT_CENTRE, (16.0, 52.0), (20.0, 52.0), (24.0, 52.0), (12.0, 60.0), (48.0, 16.0)]

    unpainted = blend(paint_probes)

    if any(sample != [1.0, 0.0, 0.0, 0.0] for sample in unpainted):
        fail('the terrain did not start entirely on layer 0: %s' % unpainted)

    ground_before_paint = profile(paint_probes)
    steps_before = undo_count()

    painted = sculpt(mode='paint', layer=1, x=PAINT_CENTRE[0], z=PAINT_CENTRE[1], radius=10.0,
                     strength=4.0, falloff=1.0, duration=0.5)

    if painted['appliedPoints'] != 1 or painted['layer'] != 1:
        fail('a paint dab reported %s' % painted)

    after_paint = blend(paint_probes)

    # The same round, soft-edged brush the height modes use, reading off the layer it painted.
    if after_paint[0][1] < 0.5:
        fail('painting gave the centre only %.3f of layer 1' % after_paint[0][1])

    if not after_paint[0][1] > after_paint[1][1] > after_paint[2][1] > after_paint[3][1]:
        fail('a fully soft paint brush does not fall off from its centre: %s'
             % [sample[1] for sample in after_paint[:4]])

    if abs(after_paint[2][1] - after_paint[4][1]) > 0.01:
        fail('the paint brush is not radially symmetric: %.3f along x against %.3f along z'
             % (after_paint[2][1], after_paint[4][1]))

    if after_paint[3] != [1.0, 0.0, 0.0, 0.0] or after_paint[5] != [1.0, 0.0, 0.0, 0.0]:
        fail('painting reached past its radius: %s' % after_paint[3:])

    # A sample gives away shares rather than gaining them: whatever layer 1 took came out of the
    # layers that were there, so the four still describe one surface.
    for point, sample in zip(paint_probes, after_paint):
        if abs(sum(sample) - 1.0) > 0.01:
            fail('the weights at (%.1f, %.1f) sum to %.4f' % (point[0], point[1], sum(sample)))

    # Painting is not sculpting. A paint stroke that moved the ground would be undone by the wrong
    # command, and would re-cook collision for a change collision cannot see.
    if profile(paint_probes) != ground_before_paint:
        fail('a paint stroke moved the ground')

    if undo_count() != steps_before + 1:
        fail('one paint stroke left %d undo steps' % (undo_count() - steps_before))

    undo()

    if blend(paint_probes) != unpainted:
        fail('undo did not restore the weights exactly: %s against %s' % (blend(paint_probes), unpainted))

    redo()

    if blend(paint_probes) != after_paint:
        fail('redo did not put the paint back exactly')

    # Held longer, coverage approaches full rather than overshooting it - which is what keeps a
    # stroke that lingers under the cursor from being different in kind from one that passes.
    sculpt(mode='paint', layer=1, x=PAINT_CENTRE[0], z=PAINT_CENTRE[1], radius=10.0,
           strength=4.0, falloff=1.0, duration=4.0)

    saturated = weights_at(*PAINT_CENTRE)

    if not after_paint[0][1] < saturated[1] <= 1.0:
        fail('a long paint stroke took layer 1 from %.3f to %.3f' % (after_paint[0][1], saturated[1]))

    # A second layer over the first takes its share from what is there, rather than from layer 0
    # alone - the sample is a blend of four, not a stack of one over another.
    sculpt(mode='paint', layer=2, x=PAINT_CENTRE[0], z=PAINT_CENTRE[1], radius=10.0,
           strength=4.0, falloff=0.0, duration=0.5)

    mixed = weights_at(*PAINT_CENTRE)

    if mixed[2] < 0.2:
        fail('painting layer 2 over layer 1 gave it %.3f' % mixed[2])

    if mixed[1] >= saturated[1] or abs(sum(mixed) - 1.0) > 0.01:
        fail('painting layer 2 left the sample as %s' % mixed)

    undo()
    undo()

    if weights_at(*PAINT_CENTRE) != after_paint[0]:
        fail('undoing two paint strokes did not return to the first one')

    # A paint stroke off the terrain changes nothing and records nothing, exactly as a height one
    # does - a drag that has left the edge must not consume the author's next undo.
    steps_before = undo_count()
    away = sculpt(mode='paint', layer=1, x=EXTENT + 50.0, z=EXTENT + 50.0, radius=4.0,
                  strength=4.0, duration=0.5)

    if away['appliedPoints'] != 0 or undo_count() != steps_before:
        fail('a paint stroke off the terrain applied %d points and left %d undo steps'
             % (away['appliedPoints'], undo_count() - steps_before))

    # A layer is a fixed splat channel, so an index outside the four is a mistake rather than
    # something to clamp into range and paint anyway.
    sculpt(expected=400, mode='paint', layer=-1, x=1.0, z=1.0)
    sculpt(expected=400, mode='paint', layer=4, x=1.0, z=1.0)
    sculpt(expected=400, mode='paint', layer=1.5, x=1.0, z=1.0)
    sculpt(expected=400, mode='paint', layer='grass', x=1.0, z=1.0)

    # And the picture: the weights have to reach the splat texture, which is a separate upload from
    # anything the numbers above went through. Layer 1's material is green and layer 0's is the
    # default white, so painted ground is the only strongly green thing in the scene.
    def green_pixels(camera_x, camera_z, name):
        moved = request('/camera', {'position': {'x': camera_x, 'y': 26.0, 'z': camera_z - 16.0},
                                    'lookAt': {'x': camera_x, 'y': 0, 'z': camera_z},
                                    'farPlane': EXTENT * 8})

        observation = request('/observe', {'after': moved['observationToken'], 'view': 'level', 'width': 700})
        image = base64.b64decode(observation['image']['data'])
        (args.output / name).write_bytes(image)

        _, _, rows = decode_png(image)

        # Green against both of the others, so neither the white ground nor the blue sky can be
        # mistaken for paint however bright either of them is.
        return sum(1 for row in rows for red, green, blue in row
                   if green > 90 and green - red > 40 and green - blue > 40)

    over_paint = green_pixels(PAINT_CENTRE[0], PAINT_CENTRE[1], 'painted.png')

    # The control is the same camera over the same ground with the paint taken back off it, rather
    # than a different corner of the terrain. It differs from the picture above in the one thing
    # being checked, so a green count cannot come from where the camera happened to be pointing.
    undo()

    if weights_at(*PAINT_CENTRE) != [1.0, 0.0, 0.0, 0.0]:
        fail('undoing the last paint stroke left the ground painted: %s' % weights_at(*PAINT_CENTRE))

    unpainted_again = green_pixels(PAINT_CENTRE[0], PAINT_CENTRE[1], 'painted-control.png')

    redo()

    if over_paint < 500:
        fail('painted ground drew %d green pixels, which is not a patch' % over_paint)

    # Which also says the splat map is re-uploaded when the weights change, not only built once:
    # a texture that never came back would leave these two counts identical.
    if unpainted_again > 50:
        fail('the same ground is green with the paint undone: %d pixels against the patch\'s %d'
             % (unpainted_again, over_paint))

    # Finally, a picture of sculpted ground. Everything above is numeric; this is what catches a
    # terrain whose mesh never picked up the new heights - the field would read as sculpted over
    # HTTP while the ground on screen stayed flat.
    sculpt(mode='raise', x=20.0, z=44.0, radius=9.0, strength=25.0, falloff=0.7, duration=0.6)

    centre = {'x': EXTENT / 2, 'y': 0, 'z': EXTENT / 2}
    moved = request('/camera', {'position': {'x': centre['x'] - 20.0, 'y': 34.0, 'z': centre['z'] - 34.0},
                                'lookAt': centre, 'farPlane': EXTENT * 8})

    observation = request('/observe', {'after': moved['observationToken'], 'view': 'level', 'width': 900})

    (args.output / 'sculpted.png').write_bytes(base64.b64decode(observation['image']['data']))

    # The drawn mesh has to carry the new shape, not just the field. A flat terrain and a sculpted
    # one differ in vertex count by nothing at all, so what says the mesh was rebuilt is that no
    # chunk is still waiting to be: Prepare clears the flag only once it has rebuilt the chunk.
    dirty = [chunk['coordinate'] for chunk in request('/terrain?path=ground')['chunks'] if chunk['dirty']]

    if dirty:
        fail('chunks were still dirty after a frame: %s' % dirty)

    # Every chunk the strokes touched has to have grown a taller box, since the ground under it
    # rose. A brush that wrote the field without dirtying the chunk would leave these at zero.
    tallest = max(chunk['boundsMax']['y'] for chunk in request('/terrain?path=ground')['chunks'])

    if tallest < 1.0:
        fail('no chunk grew a taller bounding box; the tallest reaches %.3f' % tallest)

    # The brush overlay, which is the only part of the brush the author actually looks at. The probe
    # re-applies it every frame at a corner of the terrain none of the strokes above reach.
    #
    # Counted as strongly warm pixels rather than measured at a projected point: the ring is a band
    # of one colour nothing else in this scene comes near - the ground is white under a white sun
    # and the sky is blue - so counting them needs no model of where the band lands, and the control
    # below is what makes the count mean something.
    def warm_pixels(camera_x, camera_z, name):
        moved = request('/camera', {'position': {'x': camera_x, 'y': 30.0, 'z': camera_z - 20.0},
                                    'lookAt': {'x': camera_x, 'y': 0, 'z': camera_z},
                                    'farPlane': EXTENT * 8})

        observation = request('/observe', {'after': moved['observationToken'], 'view': 'level', 'width': 700})
        image = base64.b64decode(observation['image']['data'])
        (args.output / name).write_bytes(image)

        _, _, rows = decode_png(image)

        # Measured on this scene, the ring reaches a red-blue difference of 38 while the brightest
        # thing anywhere else in the frame reaches 5 - so the threshold sits an order of magnitude
        # clear of both, and no part of it is tuned to a pixel count.
        return sum(1 for row in rows for red, green, blue in row if red > 90 and red - blue > 20)

    over_ring = warm_pixels(52.0, 8.0, 'brush-ring.png')
    away_from_ring = warm_pixels(8.0, 8.0, 'brush-ring-control.png')

    if over_ring < 500:
        fail('the brush overlay drew %d ring pixels, which is not a ring' % over_ring)

    # The control is what stops this passing on a warm frame: the overlay names the terrain it
    # belongs to and a distance from one point on it, so ground elsewhere has to be untinted.
    if away_from_ring > 50:
        fail('ground away from the brush is tinted too: %d pixels against the ring\'s %d'
             % (away_from_ring, over_ring))

    print('PASS: the ray march and both rectangle accessors natively, a brush with round falloff,\n'
          '      raise, lower, flatten and smooth, one undo step per stroke restoring the ground\n'
          '      exactly, a drag growing its recorded region, strokes off the terrain recording\n'
          '      nothing, rejected requests, the drawn mesh following the field, the brush overlay\n'
          '      reaching the ground through its own shader version, and the same brush painting\n'
          '      a layer - falling off, normalizing, undoing exactly and reaching the splat map.')
    print('Inspect sculpted.png, painted.png and brush-ring.png in', args.output)
finally:
    shutdown()
    log.close()

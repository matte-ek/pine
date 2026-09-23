"""Build a native terrain probe, then check how terrain is lit and what it casts; requires a Ninja Editor build and Xvfb."""

import argparse
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


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19036)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-terrain-lighting-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-terrain-lighting.'))
url = 'http://127.0.0.1:%d' % args.port
print('Terrain lighting verification:', root, flush=True)

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
#include <stdexcept>
#include <string>
#include <vector>
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-lighting.inc').read_text()
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
log_path = root / 'terrain-lighting.log'
log = log_path.open('w')
process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'terrain'], cwd=data,
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




# The clear colour is a blue gradient and the terrain draws with the untextured default material,
# so "more blue than red" separates the two whatever the light does to the ground's brightness.
def is_sky(pixel):
    return pixel[2] > pixel[0] + 25


def luminance(pixel):
    return sum(pixel) / 3.0


# Mirrors the lamp grid in terrain-lighting.inc. Written out again rather than read back over HTTP:
# the claim under test is that the engine picked the right five lamps for each chunk, and taking the
# engine's own answer as the input would make that unfalsifiable.
LAMPS = [(7.0, 5.0), (51.0, 9.0), (13.0, 47.0), (57.0, 53.0),
         (29.0, 19.0), (37.0, 61.0), (3.0, 31.0), (61.0, 25.0)]
LAMP_HEIGHT = 12.0

# Renderer3D::Specifications::ObjectLightSlots. A chunk keeps this many point lights and drops the
# rest, which is the whole reason there are more lamps than slots.
POINT_SLOT_COUNT = 5


def find_entity(name):
    pending = list(request('/entities')['entities'])
    while pending:
        entity = pending.pop()
        if entity['name'] == name:
            return entity
        pending += entity['children']
    fail("the probe's scene has no entity named '%s'" % name)


def component_of(entity_name, component_type):
    entity = request('/entity?id=' + find_entity(entity_name)['id'])
    for component in entity['components']:
        if component['type'] == component_type:
            return component
    fail("'%s' has no %s" % (entity_name, component_type))


# Sends a component's properties back with a few of them changed. The whole set rather than the
# changed keys, because a component is applied as one state.
def update_component(entity_name, component_type, changes):
    component = component_of(entity_name, component_type)
    properties = dict(component['properties'])
    properties.update(changes)

    request('/edit', {'version': 1, 'operations': [
        {'op': 'component.update', 'target': {'id': component['id']}, 'properties': properties}
    ]})


def expected_lamps(chunk, positions):
    """The lamps a chunk should have kept: the nearest POINT_SLOT_COUNT to its box's centre."""
    centre = tuple((chunk['boundsMin'][axis] + chunk['boundsMax'][axis]) / 2.0 for axis in 'xyz')

    def distance(index):
        x, z = positions[index]
        return (x - centre[0]) ** 2 + (LAMP_HEIGHT - centre[1]) ** 2 + (z - centre[2]) ** 2

    # The terrain entity sits at the origin, so a chunk's terrain-local box is its world box.
    return ['Lamp %d' % index for index in sorted(range(len(positions)), key=distance)[:POINT_SLOT_COUNT]]


def check_chunk_lights(positions, when):
    for chunk in request('/terrain?path=ground')['chunks']:
        expected = expected_lamps(chunk, positions)

        if chunk['lights']['point'] != expected:
            fail('chunk (%d, %d) %s is lit by %s; the nearest five lamps are %s'
                 % (chunk['coordinate']['x'], chunk['coordinate']['z'], when,
                    chunk['lights']['point'], expected))

        if chunk['lights']['spot']:
            fail('chunk (%d, %d) claims spot lights (%s) in a scene that has none'
                 % (chunk['coordinate']['x'], chunk['coordinate']['z'], chunk['lights']['spot']))


# Renders the level viewport, keeps the image next to the results for a human to look at, and
# returns it decoded. One request rather than two: a second render of the "same" frame is a
# different frame, and comparing two of those would fold the post-process grain into every result.
def capture(name):
    payload = request('/viewport.png?view=level&width=700')
    (args.output / name).write_bytes(payload)

    return decode_png(payload)


# How many pixels of 'before' the matching pixel of 'after' is brighter than, by more than a
# threshold that clears the post-process grain.
def count_brighter(before, after, threshold):
    width, height, before_rows = before
    _, _, after_rows = after

    return sum(1
               for y in range(height)
               for x in range(width)
               if luminance(after_rows[y][x]) - luminance(before_rows[y][x]) > threshold)


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

    chunks = request('/terrain?path=ground')['chunks']

    if any(chunk['dirty'] for chunk in chunks):
        fail('the render path left %d chunks unbuilt' % sum(chunk['dirty'] for chunk in chunks))

    # Nothing asked for these. The slots are filled once per frame by the scene processor, through
    # the same code that lights a model renderer, and a chunk that came back with none means terrain
    # is still drawing with whatever light indices the previous draw happened to leave behind.
    if all(not chunk['lights']['point'] for chunk in chunks):
        fail('no chunk was assigned any light at all')

    check_chunk_lights(LAMPS, 'as the scene was built')

    # Eight lamps over sixteen chunks: if every chunk came back with the same five, the assignment
    # is not a function of where the chunk is and the check above would pass on a constant.
    if len({tuple(chunk['lights']['point']) for chunk in chunks}) < 4:
        fail('every chunk ended up with much the same five lamps, so the choice is not positional')

    # Moving a lamp has to move the slots with it. This is the half that caching gets wrong: the
    # slots survive between frames on purpose, so something has to notice that the inputs changed.
    moved = list(LAMPS)
    moved[0] = (61.0, 61.0)

    update_component('Lamp 0', 'Transform',
                     {'LocalPosition': {'x': moved[0][0], 'y': LAMP_HEIGHT, 'z': moved[0][1]}})

    check_chunk_lights(moved, 'after a lamp moved across the terrain')

    # Framed on the flat ground just past the plateau, which is where the sun's shadow of it lands,
    # and close enough that the ground is inside the cascades' reach.
    request('/camera', {'position': {'x': 32, 'y': 14, 'z': 60},
                        'lookAt': {'x': 32, 'y': 0, 'z': 44}, 'farPlane': 400})

    lit = capture('sun-shadows-on.png')

    # The same frame with the sun's shadows turned off. Terrain is the only thing in this scene, so
    # every pixel that changes between the two is one the ground shadowed for itself - if terrain
    # never reached the shadow pass, the two frames would be identical.
    update_component('Sun', 'Light', {'CastShadows': False})

    unlit = capture('sun-shadows-off.png')

    width, height, _ = lit
    pixels = width * height

    brightened = count_brighter(lit, unlit, 12)

    if brightened < pixels * 0.02:
        fail('turning the sun\'s shadows off changed %.2f%% of the frame; the plateau should be '
             'casting a shadow across the ground in front of it' % (100.0 * brightened / pixels))

    # The converse, and the reason the threshold above is a floor rather than a target: a terrain
    # that came back uniformly darker with shadows on is not casting a shadow, it is acneing.
    if brightened > pixels * 0.6:
        fail('turning the sun\'s shadows off changed %.1f%% of the frame; that is the whole ground '
             'rather than a shadow, which is what self-shadow acne looks like'
             % (100.0 * brightened / pixels))

    # And the same for a local light, which reaches the atlas through a different path: its tiles
    # are built once for the scene and cached between frames, rather than rebuilt per viewer.
    #
    # Over the plateau's near edge and well above it, so the band it shadows ends inside the frame:
    # the ray grazing the plateau's far corner reaches the ground around z = 50, leaving lit ground
    # beyond that. A light low enough to shadow everything the camera can see would be
    # indistinguishable from one that is simply dimmer.
    update_component('Lamp 3', 'Transform', {'LocalPosition': {'x': 32, 'y': 16, 'z': 26}})
    update_component('Lamp 3', 'Light', {'Range': 64, 'Intensity': 3, 'CastShadows': True})

    lamp_shadowed = capture('lamp-shadows-on.png')

    update_component('Lamp 3', 'Light', {'Range': 64, 'Intensity': 3, 'CastShadows': False})

    lamp_unshadowed = capture('lamp-shadows-off.png')

    # The lamp stands on the near side of the plateau, so everything past it is behind the plateau
    # from the lamp's point of view and loses the lamp's contribution once it casts.
    lamp_brightened = count_brighter(lamp_shadowed, lamp_unshadowed, 8)

    if lamp_brightened < pixels * 0.01:
        fail('turning a point light\'s shadows off changed %.2f%% of the frame; the plateau should '
             'be blocking it from the ground behind it' % (100.0 * lamp_brightened / pixels))

    if lamp_brightened > pixels * 0.4:
        fail('turning a point light\'s shadows off changed %.1f%% of the frame; the lamp stands '
             'over the plateau and should only be blocked from the band of ground just past it'
             % (100.0 * lamp_brightened / pixels))

    print('PASS: native chunk slot invalidation, per-chunk light slots matching the nearest lamps\n'
          '      and following one as it moves, terrain casting into the directional cascades and\n'
          '      into a point light\'s atlas tiles.')
    print('Inspect sun-shadows-on/off.png and lamp-shadows-on/off.png in', args.output)
finally:
    shutdown()
    log.close()

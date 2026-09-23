"""Build a native terrain probe, then check the rendered terrain; requires a Ninja Editor build and Xvfb."""

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
parser.add_argument('--port', type=int, default=19035)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-terrain-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-terrain-render.'))
url = 'http://127.0.0.1:%d' % args.port
print('Terrain render verification:', root, flush=True)

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
#include <vector>
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('terrain-render.inc').read_text()
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
    layout = terrain['layout']
    chunks = terrain['chunks']

    if terrain['chunkCount'] != layout['chunkCount']['x'] * layout['chunkCount']['z']:
        fail('the chunk grid and the chunk list disagree')

    # Nothing asked for these meshes: the render path is what notices a dirty chunk, and it has to
    # do that from Pipeline3D::Prepare rather than mid-pass, once for every context.
    if any(chunk['dirty'] for chunk in chunks):
        fail('the render path left %d of %d chunks unbuilt'
             % (sum(chunk['dirty'] for chunk in chunks), len(chunks)))

    if not any(chunk['boundsMax']['y'] - chunk['boundsMin']['y'] > 1 for chunk in chunks):
        fail('the noise fill produced a terrain flat enough that lighting cannot be judged')

    span = layout['chunkCount']['x'] * layout['chunkSize']
    centre = {'x': span / 2, 'y': 0, 'z': span / 2}

    # Steep and close: steep enough that a crack between chunks shows sky through it, close enough
    # that the terrain dominates the frame, and tilted just off vertical so the look-at has a
    # well-defined up vector.
    request('/camera', {'position': {'x': centre['x'], 'y': span * 0.78, 'z': centre['z'] + span * 0.18},
                        'lookAt': centre, 'farPlane': span * 8})

    overhead = request('/viewport.png?view=level&width=700')
    (args.output / 'overhead.png').write_bytes(overhead)
    width, height, rows = decode_png(overhead)

    sky = sum(1 for y in range(height) for x in range(width) if is_sky(rows[y][x]))
    covered = 1.0 - sky / float(width * height)

    if covered < 0.55:
        fail('terrain covered %.1f%% of the frame; the camera should be looking straight at it' % (100 * covered))

    # The camera looks at the middle of the terrain, so this band is known to be over ground without
    # having to find the silhouette in the image - which editor overlays drawn on top of it would
    # make unreliable. At 60% of the frame it still spans several chunk boundaries in both axes.
    inset_x, inset_y = int(width * 0.2), int(height * 0.2)
    interior = [(x, y)
                for y in range(inset_y, height - inset_y)
                for x in range(inset_x, width - inset_x)]

    # Every pixel in that band has to be ground. A sky pixel there is a hole - a crack between
    # chunks, a chunk that never rendered, or a mesh wound the wrong way and culled away.
    holes = [point for point in interior if is_sky(rows[point[1]][point[0]])]

    if holes:
        fail('%d sky pixels inside the terrain, first at %s' % (len(holes), holes[0]))

    luminance = sorted(sum(rows[y][x]) / 3.0 for x, y in interior)

    if luminance[len(luminance) // 2] < 20:
        fail('the terrain rendered nearly black (median luminance %.1f)' % (luminance[len(luminance) // 2]))

    # Terrain lit by flat normals - the bug this unit exists to fix - renders as an even wash, so
    # the check has to be shading *variation* rather than brightness. Neither of the two obvious
    # statistics works on its own: the post-process grain swamps a per-pixel gradient, and the
    # vignette inflates a whole-frame percentile spread by as much as real relief does. Averaging
    # blocks first removes the grain, and comparing neighbouring blocks removes the vignette, which
    # varies over the whole frame rather than between two adjacent blocks.
    block = 8
    columns, lines = (width - 2 * inset_x) // block, (height - 2 * inset_y) // block
    grid = [[sum(sum(rows[inset_y + by * block + y][inset_x + bx * block + x]) / 3.0
                 for y in range(block) for x in range(block)) / (block * block)
             for bx in range(columns)]
            for by in range(lines)]

    differences = sorted(
        [abs(grid[y][x] - grid[y][x + 1]) for y in range(lines) for x in range(columns - 1)] +
        [abs(grid[y][x] - grid[y + 1][x]) for y in range(lines - 1) for x in range(columns)])
    contrast = differences[int(len(differences) * 0.9)]

    # Measured on this scene: ~4.2 with the normals the height field implies, ~1.9 with every normal
    # forced straight up. Halfway between, so both sides have room to move.
    if contrast < 3.0:
        fail('the terrain rendered with almost no shading variation (90th percentile block contrast '
             '%.2f), which is what flat normals look like' % contrast)

    # Looking at the whole terrain from above, nothing is behind the viewer, so every chunk has to
    # survive the frustum test. This is the reading that catches a culling test with its sense
    # inverted - which otherwise renders a perfectly convincing empty frame.
    statistics = request('/stats')['level']

    if statistics['culledTerrainChunks'] != 0:
        fail('%d of %d chunks were culled while the camera was looking at all of them'
             % (statistics['culledTerrainChunks'], len(chunks)))

    if statistics['visibleTerrainChunks'] != len(chunks):
        fail('expected all %d chunks visible from overhead, got %d'
             % (len(chunks), statistics['visibleTerrainChunks']))

    # Every visible chunk draws once in the depth pre-pass and once in the scene pass, and the
    # terrain is the only thing in this scene that draws at all.
    if statistics['drawCalls'] < 2 * len(chunks):
        fail('expected at least %d draw calls for %d chunks, got %d'
             % (2 * len(chunks), len(chunks), statistics['drawCalls']))

    # What one chunk costs at each detail level, following Terrain::BuildChunkMesh: 6 indices per
    # quad of the grid, plus a skirt ring of 4 * quads segments at two triangles each. Terrain is
    # the only thing drawing in this scene, so dividing the frame's submitted vertices by its draw
    # calls says which level the chunks were drawn at - which is otherwise invisible over HTTP.
    def chunk_indices(level):
        level_quads = layout['chunkQuads'] >> level

        return 6 * level_quads * level_quads + 24 * level_quads

    # Same framing, backed off past the coarsest switch distance. Every chunk is still in frame, so
    # anything other than the coarsest level here means selection is not reaching the end of its
    # range - the case where LOD exists but the levels beyond the first are dead code.
    request('/camera', {'position': {'x': centre['x'], 'y': span * 6, 'z': centre['z'] + span * 1.4},
                        'lookAt': centre, 'farPlane': span * 40})

    statistics = request('/stats')['level']
    coarsest = chunk_indices(layout['lodCount'] - 1)

    if statistics['visibleTerrainChunks'] != len(chunks):
        fail('backing off dropped %d chunks; the framing was supposed to keep all of them'
             % (len(chunks) - statistics['visibleTerrainChunks']))

    if statistics['vertexCount'] != statistics['drawCalls'] * coarsest:
        fail('%d draw calls submitted %d vertices from far away; every one of them should have '
             'drawn the coarsest level, at %d each'
             % (statistics['drawCalls'], statistics['vertexCount'], coarsest))

    # Turned around on the spot. Every chunk is now behind the viewer, so the frustum test has to
    # reject all of them - and the draw calls that would have gone with them have to disappear too,
    # which is what separates real culling from a counter that is merely reported.
    request('/camera', {'position': {'x': centre['x'], 'y': span * 0.78, 'z': centre['z'] + span * 0.18},
                        'lookAt': {'x': centre['x'], 'y': span * 1.6, 'z': centre['z'] + span * 1.2},
                        'farPlane': span * 8})

    statistics = request('/stats')['level']

    if statistics['visibleTerrainChunks'] != 0:
        fail('%d chunks stayed visible with the camera pointed away from the terrain'
             % statistics['visibleTerrainChunks'])

    if statistics['culledTerrainChunks'] != len(chunks):
        fail('expected all %d chunks culled looking away, got %d'
             % (len(chunks), statistics['culledTerrainChunks']))

    if statistics['drawCalls'] != 0:
        fail('%d draw calls were still issued with every chunk culled' % statistics['drawCalls'])

    # Back to ground level, which is where LOD cracks actually show. Standing on the terrain puts
    # the nearest chunks at the finest level and the horizon several levels coarser, so this frame
    # crosses every switch distance the terrain has.
    request('/camera', {'position': {'x': centre['x'], 'y': 30, 'z': span * 0.85},
                        'lookAt': {'x': centre['x'], 'y': 0, 'z': span * 0.25}, 'farPlane': span * 8})

    ground_level = request('/viewport.png?view=level&width=700')
    (args.output / 'ground-level.png').write_bytes(ground_level)
    width, height, rows = decode_png(ground_level)

    # Below the skyline every pixel is ground: the terrain is a height field seen from above its
    # surface, so once a column has hit ground, everything under it is more ground. A sky pixel
    # down there is a crack between two chunks drawn at different levels - which is precisely what
    # the skirts exist to hide, and which no overhead shot can show.
    #
    # Only the middle columns qualify. Towards the sides of the frame the terrain's own outer edge
    # comes into view, and sky past the edge of the world is not a crack.
    cracks = []
    for x in range(int(width * 0.3), int(width * 0.7)):
        column = [y for y in range(height) if not is_sky(rows[y][x])]

        if not column:
            fail('column %d of the ground-level frame is entirely sky' % x)

        cracks += [(x, y) for y in range(column[0], height) if is_sky(rows[y][x])]

    if cracks:
        fail('%d sky pixels below the skyline, first at %s - a chunk edge is showing through'
             % (len(cracks), cracks[0]))

    # The other end of the range. Standing on the terrain, the chunk underfoot is at zero distance,
    # so at least one chunk has to be drawing at the finest level - and since level 0 is the only
    # level above level 1, costing more than an all-level-1 frame is what proves it.
    statistics = request('/stats')['level']
    second_finest = statistics['drawCalls'] * chunk_indices(1)

    if statistics['vertexCount'] <= second_finest:
        fail('%d draw calls submitted %d vertices from ground level, no more than the %d an '
             'entirely level-1 frame would cost; nothing reached the finest level'
             % (statistics['drawCalls'], statistics['vertexCount'], second_finest))

    print('PASS: native per-level chunk meshes with skirts and dirty propagation, meshes built by\n'
          '      the render path, hole-free coverage, real shading variation, per-chunk draw calls,\n'
          '      frustum culling in both directions and detail falling off with distance.')
    print('Inspect overhead.png and ground-level.png in', args.output)
finally:
    shutdown()
    log.close()

"""Check Level atmosphere authoring, its undo behaviour and persistence; needs an Editor build and Xvfb."""

import argparse
import base64
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

from headless import headless_command


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
parser.add_argument('--port', type=int, default=19052)
parser.add_argument('--output', type=Path, default=Path('/tmp/pine-level-settings-results'))
args = parser.parse_args()
build = args.build.resolve()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix='pine-level-settings.'))
url = 'http://127.0.0.1:%d' % args.port
print('Level settings verification:', root, flush=True)

# A disposable data directory, because the editor rewrites imgui.ini on exit. Timestamps are
# preserved so the engine assets are not all re-imported on boot.
data = root / 'data'
(data / 'projects/atmosphere/assets').mkdir(parents=True)
for name in ['engine', 'editor']:
    subprocess.run(['cp', '-a', str(repo / 'data' / name), str(data / name)], check=True)
subprocess.run(['cp', '-a', str(Path(__file__).with_name('verification-layout.ini')),
                str(data / 'imgui.ini')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null', 'PINE_DEBUG_SERVER': str(args.port)}
log_path = root / 'editor.log'
log = log_path.open('w')
process = subprocess.Popen(headless_command(build / 'Editor/Editor', 'atmosphere'), cwd=data,
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


def settings(properties, expected=200):
    return fetch('/level/settings', {'properties': properties}, expected)


def read():
    return fetch('/level/settings')['properties']


def edit(operations, expected=200):
    return fetch('/edit', {'version': 1, 'operations': operations}, expected)


def vec(x, y, z):
    return dict(zip('xyz', (x, y, z)))


def near(actual, expected):
    """Compare a readback with what was asked for, at float32 precision."""
    if isinstance(expected, dict):
        return set(actual) == set(expected) and all(near(actual[key], expected[key]) for key in expected)
    if isinstance(expected, (int, float)):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 1e-6
    return actual == expected


def render(name):
    """A deterministic capture of the scene, saved for inspection."""
    capture = fetch('/render', {'position': vec(0, 1, 6), 'lookAt': vec(0, 0, 0),
                                'width': 240, 'height': 180})
    image = base64.b64decode(capture['image']['data'], validate=True)
    (args.output / name).write_bytes(image)
    return image


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

    # ------------------------------------------------------------------ reading

    reply = fetch('/level/settings')
    assert set(reply) == {'level', 'properties'}, reply
    assert reply['level']['path'] == status['activeLevel'], (reply['level'], status)

    defaults = reply['properties']
    expected_properties = {'Skybox', 'AmbientColor', 'FogColor', 'FogDensity', 'FogHeight',
                           'FogHeightFalloff', 'Exposure', 'BloomThreshold', 'BloomIntensity',
                           'GrainStrength', 'VignetteStrength', 'WindDirection', 'WindStrength',
                           'WindSpeed'}
    assert set(defaults) == expected_properties, sorted(defaults)

    # The engine's own defaults, so a caller can tell an authored value from an untouched one.
    assert defaults['Skybox'] is None
    assert near(defaults['AmbientColor'], vec(0.05, 0.05, 0.05)), defaults['AmbientColor']
    assert near(defaults['FogColor'], {'x': 0.0, 'y': 0.0, 'z': 0.0, 'w': 1.0}), defaults['FogColor']
    assert near(defaults['FogDensity'], 0.0) and near(defaults['FogHeightFalloff'], 0.1)
    assert near(defaults['Exposure'], 1.0) and near(defaults['BloomIntensity'], 0.6)
    assert near(defaults['WindStrength'], 0.1) and near(defaults['WindSpeed'], 0.5)

    # The camera belongs to /level/camera; overlapping the two routes would give two ways to
    # write one field.
    assert 'CameraEntity' not in defaults and 'HasCamera' not in defaults

    schema = fetch('/edit/schema')['levelSettings']
    assert schema['method'] == 'POST' and schema['undo'] is True
    assert schema['requiredPlayState'] == 'Stopped'
    assert set(schema['properties']) == expected_properties, sorted(schema['properties'])
    assert schema['properties']['FogColor']['type'] == 'vector4'
    assert schema['properties']['Exposure']['uiRange'] == [0, 8]

    # ------------------------------------------------------------------ partial writes

    reply = settings({'Exposure': 2.5})
    assert reply['history'] == 'recorded'
    assert near(reply['properties']['Exposure'], 2.5)

    after = read()
    unchanged = {name: value for name, value in after.items() if name != 'Exposure'}
    assert unchanged == {name: value for name, value in defaults.items() if name != 'Exposure'}, after

    # A supplied colour replaces the whole colour rather than one channel.
    settings({'AmbientColor': vec(0.4, 0.1, 0.1),
              'FogColor': {'x': 0.2, 'y': 0.25, 'z': 0.3, 'w': 1.0},
              'FogDensity': 0.04, 'FogHeight': -2.5, 'FogHeightFalloff': 0.2})
    after = read()
    assert near(after['AmbientColor'], vec(0.4, 0.1, 0.1)), after
    assert near(after['FogColor'], {'x': 0.2, 'y': 0.25, 'z': 0.3, 'w': 1.0}), after
    assert near(after['FogDensity'], 0.04) and near(after['FogHeightFalloff'], 0.2)
    assert near(after['FogHeight'], -2.5), 'a fog height below zero is a valid height'

    # ------------------------------------------------------------------ rejection

    assert settings({'Exposure': -1}, 400)['path'] == '/properties/Exposure'
    assert settings({'WindStrength': -0.5}, 400)['path'] == '/properties/WindStrength'
    assert settings({'WindSpeed': -1}, 400)['path'] == '/properties/WindSpeed'
    assert settings({'FogDensity': -0.1}, 400)['path'] == '/properties/FogDensity'
    assert settings({'FogHeightFalloff': -1}, 400)['path'] == '/properties/FogHeightFalloff'
    assert settings({'Exposure': 'bright'}, 400)['path'] == '/properties/Exposure'
    assert settings({'Nonsense': 1}, 400)['path'] == '/properties/Nonsense'
    assert settings({'AmbientColor': {'x': 1, 'y': 1}}, 400)['path'] == '/properties/AmbientColor/z'
    assert settings({'AmbientColor': vec(-1, 0, 0)}, 400)['path'] == '/properties/AmbientColor/x'
    assert settings({'FogColor': vec(1, 1, 1)}, 400)['path'] == '/properties/FogColor/w'
    assert settings({}, 400)['path'] == '/properties'
    assert settings([], 400)['path'] == '/properties'
    assert fetch('/level/settings', {}, 400)['path'] == '/properties'
    assert fetch('/level/settings', {'properties': {}, 'extra': 1}, 400)['path'] == '/extra'
    assert fetch('/level/settings', {'properties': {'Exposure': {'a': {'b': {'c': {'d': {'e': {'f': 1}}}}}}}}, 400)

    # Skybox is an asset reference, so it gets the asset rules: loaded, right type, or explicit null.
    # There is no Texture3D in the engine assets, so only the rejection and clearing paths are
    # covered here; setting a cubemap needs a project that has one.
    assert settings({'Skybox': {'path': 'engine/shaders/3d/skybox'}}, 400)['path'] == '/properties/Skybox'
    assert settings({'Skybox': {'path': 'no/such/asset'}}, 400)['path'] == '/properties/Skybox'
    assert settings({'Skybox': {'id': 'a-b'}}, 400)['path'] == '/properties/Skybox/id'
    assert settings({'Skybox': None})['properties']['Skybox'] is None

    # Every rejection above left the level alone.
    assert read() == after, read()

    # ------------------------------------------------------------------ the renderer agrees

    # Pine shades nothing without a light, so a scene lit only by AmbientColor renders black and
    # would report no difference however the atmosphere is set.
    edit([
        {'op': 'entity.create', 'name': 'Atmosphere cube', 'components': [
            {'type': 'ModelRenderer', 'properties': {'Model': {'path': 'engine/primitive/cube'}}}]},
        {'op': 'entity.create', 'name': 'Atmosphere light', 'components': [
            {'type': 'Transform', 'properties': {'LocalPosition': vec(4, 6, 8)}},
            {'type': 'Light', 'properties': {'Type': 'PointLight', 'Range': 40, 'Intensity': 6}}]},
    ])

    # Film grain is animated, so a capture is only reproducible with it turned off - which is
    # itself a write through the route under test.
    settings({'GrainStrength': 0, 'VignetteStrength': 0, 'Exposure': 1, 'FogDensity': 0,
              'AmbientColor': vec(0.05, 0.05, 0.05)})
    dim = render('ambient-dim.png')
    assert render('ambient-dim-again.png') == dim, 'captures are not reproducible; cannot compare'

    settings({'AmbientColor': vec(0.9, 0.2, 0.2)})
    assert render('ambient-bright.png') != dim, 'ambient light did not reach the renderer'

    # ------------------------------------------------------------------ undo and redo

    history = fetch('/history')
    assert fetch('/history/undo', {})['applied'] is True
    assert near(read()['AmbientColor'], vec(0.05, 0.05, 0.05)), read()
    assert render('ambient-undone.png') == dim, 'undo restored the value but not the rendered scene'

    assert fetch('/history/redo', {})['applied'] is True
    assert near(read()['AmbientColor'], vec(0.9, 0.2, 0.2)), read()
    assert fetch('/history')['undoCount'] == history['undoCount']

    # An undo step that did not touch the settings must leave them alone, rather than rewriting
    # them from whatever the snapshot happened to hold.
    created = edit([{'op': 'entity.create', 'name': 'Unrelated'}])['results'][0]['entity']['id']
    settings({'Exposure': 3.25})
    edit([{'op': 'entity.delete', 'target': {'id': created}}])
    fetch('/history/undo', {})
    assert near(read()['Exposure'], 3.25), read()
    assert any(entity['name'] == 'Unrelated' for entity in fetch('/entities')['entities'])

    # ------------------------------------------------------------------ persistence

    fetch('/level/save-as', {'path': 'levels/atmosphere'})
    assert fetch('/level/status')['unsavedChanges'] is False

    settings({'Exposure': 4.5, 'BloomThreshold': 2.25, 'FogDensity': 0.125, 'FogHeight': 3.5,
              'WindDirection': 135, 'WindStrength': 0.35, 'WindSpeed': 1.25})
    assert fetch('/level/status')['unsavedChanges'] is True, 'a settings change is not authored state'

    fetch('/level/save', {})
    assert fetch('/level/status')['unsavedChanges'] is False
    saved = read()

    fetch('/level/load', {'path': 'levels/atmosphere'})
    reloaded = read()
    assert reloaded == saved, (saved, reloaded)
    assert near(reloaded['Exposure'], 4.5) and near(reloaded['BloomThreshold'], 2.25)
    assert near(reloaded['WindDirection'], 135) and near(reloaded['WindStrength'], 0.35)
    assert near(reloaded['WindSpeed'], 1.25)
    assert near(reloaded['FogDensity'], 0.125) and near(reloaded['FogHeight'], 3.5)

    print('Level settings verification passed. Images in', args.output, flush=True)
finally:
    shutdown()

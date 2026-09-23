"""Drive Light, Camera, Collider and the two audio components from C# through the Editor's boot sequence; needs a Ninja Editor build, the .NET SDK and Xvfb."""

import argparse
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import signal
import struct
import subprocess
import tempfile


repo = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=repo / 'cmake-build-debug-agent')
args = parser.parse_args()
build = args.build.resolve()
root = Path(tempfile.mkdtemp(prefix='pine-script-components.'))
print('Script component binding verification:', root, flush=True)

# Use the same compiler flags and linked objects as Editor, replacing only its main-loop call with
# deterministic checks.
application = repo / 'Editor/src/Application.cpp'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(command for command in commands if Path(command['file']) == application)
source = application.read_text()
marker = '    Pine::Engine::Run();'
assert source.count(marker) == 1, 'Editor main-loop entry changed; update this probe.'
includes = '''#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
'''
body = Path(__file__).with_name('script-components.inc').read_text()
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

data = root / 'data'
assets = data / 'projects/scriptcomponents/assets'
assets.mkdir(parents=True)
for name in ['engine', 'editor']:
    shutil.copytree(repo / 'data' / name, data / name)

# A clip for the AudioSource binding to carry. Half a second of 440 Hz at 44100 Hz mono, so the
# duration C# reads back is exactly 0.5 and not something a float comparison has to be lenient
# about. The probe imports it; it lives outside assets/ so nothing else picks it up on the way.
content = data / 'projects/scriptcomponents/content'
content.mkdir(parents=True)
sample_rate = 44100
frames = sample_rate // 2
samples = b''.join(struct.pack('<h', int(math.sin(2 * math.pi * 440 * frame / sample_rate) * 20000))
                   for frame in range(frames))
wave = (b'WAVE'
        + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, sample_rate, sample_rate * 2, 2, 16)
        + b'data' + struct.pack('<I', len(samples)) + samples)
(content / 'tone.wav').write_bytes(b'RIFF' + struct.pack('<I', len(wave)) + wave)

# Reads every bound property back, then writes to a different one of each, so a binding that is
# wired in only one direction fails here rather than looking fine. ScriptAsset is read too: it
# resolves through the same AssetTypeToString lookup as the clip, and was null until the managed
# class and that name agreed.
(assets / 'ComponentTest.cs').write_text('''using Pine.Assets;
using Pine.Math;
using Pine.World.Components;

namespace Game
{
    public class ComponentTest : Script
    {
        public bool ReadBackOk;

        public void OnStart()
        {
            var light = Parent.GetComponent<Light>();
            var camera = Parent.GetComponent<Camera>();
            var collider = Parent.GetComponent<Collider>();
            var audioSource = Parent.GetComponent<AudioSource>();
            var audioListener = Parent.GetComponent<AudioListener>();

            var clip = AssetManager.Get<Audio>("tone");

            ReadBackOk =
                light.LightType == LightType.SpotLight &&
                light.LightIntensity == 2.5f &&
                light.Range == 12.0f &&
                !light.CastShadows &&
                camera.FieldOfView == 80.0f &&
                camera.FarPlane == 500.0f &&
                collider.ColliderType == ColliderType.Sphere &&
                collider.IsTrigger &&
                collider.Layer == 4u &&
                audioSource.AudioFile == null &&
                audioSource.PlaybackState == PlaybackState.Stopped &&
                !audioSource.IsPlaying &&
                audioSource.Loop &&
                !audioSource.Spatial &&
                audioSource.Pitch == 1.5f &&
                clip != null &&
                clip.Duration == 0.5f &&
                audioListener.Volume == 0.25f &&
                ScriptAsset != null &&
                ScriptAsset.Type == AssetType.CSharpScript;

            light.LightColor = new Vector3(0.25f, 0.5f, 0.75f);
            light.LightIntensity = 4.0f;
            light.SpotlightOuterAngle = 60.0f;

            camera.FieldOfView = 55.0f;
            camera.CameraType = CameraType.Orthographic;

            collider.Size = new Vector3(2.0f, 3.0f, 4.0f);
            collider.LayerMask = 12u;

            audioSource.AudioFile = clip;
            audioSource.Volume = 0.75f;
            audioSource.MaxDistance = 80.0f;
            audioSource.PlaybackPosition = 0.2f;
            audioSource.Play();

            audioListener.Volume = 0.5f;
        }
    }
}
''')

# The gameplay assembly, which the editor loads but never builds. A throwaway SDK project rather
# than the one the real projects carry: a single source file against a single reference needs
# nothing else, and the properties below are the ones the engine's loader depends on - an output
# path with no framework segment in it, and no local copy of Pine.dll for the game's load context
# to find a second time.
runtime = data / 'projects/scriptcomponents/runtime-bin'
runtime.mkdir(parents=True)
(runtime / 'Game.csproj').write_text(f'''<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <AssemblyName>Game</AssemblyName>
    <OutputPath>./</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="{assets / 'ComponentTest.cs'}" />
    <Reference Include="Pine">
      <HintPath>{data / 'engine/script/Pine.dll'}</HintPath>
      <Private>false</Private>
    </Reference>
  </ItemGroup>
</Project>
''')
subprocess.run(['dotnet', 'build', '-c', 'Release', '--nologo', '--verbosity', 'quiet',
                str(runtime / 'Game.csproj')], check=True)

environment = {**os.environ, 'PINE_X11': '1', 'ALSOFT_DRIVERS': 'null'}
environment.pop('PINE_DEBUG_SERVER', None)
log_path = root / 'native.log'
with log_path.open('w') as log:
    process = subprocess.Popen(['xvfb-run', '-a', str(root / 'probe'), 'scriptcomponents'], cwd=data,
                               env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        result = process.wait(timeout=120)
    finally:
        # Also clean up the virtual display when a failing probe exits unexpectedly.
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
print(log_path.read_text()[-4000:])
assert result == 0, 'Script component binding verification failed; see ' + str(log_path)

# Audio

Two layers, the same split as `Graphics/` → `Rendering/`. Paths relative to `Engine/src/Pine/`.

## Start here
- `Audio/Audio.{hpp,cpp}` — `namespace Pine::Audio`: the voice pool and the per-frame reconcile. The whole subsystem is here.
- `Audio/Interfaces/IAudioAPI.hpp` — the backend seam: buffers, voices, and the one listener.
- `Audio/Interfaces/IAudioSource.hpp`, `IAudioBuffer.hpp` — a voice and a decoded clip.
- `Audio/OpenAL/` — the only implementation. `Source/ALSource`, `Buffer/ALBuffer`.
- `World/Components/AudioSource/`, `World/Components/AudioListener/` — the components. They hold playback state; the one device-side thing they do is hand their voice back in `AudioSource::OnDestroyed`.
- `Assets/AudioFile/` — the clip asset and its importer; see [assets.md](assets.md).

## How it fits together

- **`Audio/` is the device layer.** `IAudioAPI` owns buffers, voices and the listener. `Pine::Audio::Setup()` is called early in `Engine::Setup` (right after Graphics), and a failure is a **warning, not fatal** — the engine runs fine on a machine with no output device.
- **`Pine::Audio` is the subsystem on top.** `Audio::Update()` runs once a frame from `World::Update()`, after physics so a sound riding a moving body is heard from where that body ended up. It moves the listener to the active `AudioListener`, then reconciles every `AudioSource` to the device.
- **The components are data.** `AudioSource::Play`, `Pause` and `Stop` write a field; nothing above the device layer makes an `al*` call. `AudioSource::IsPlaying` is a field read, and `Audio::Update` writes that field back to `Stopped` when a clip that is not looping reaches its end, so it stays true rather than only reflecting the last call made.

## Voices

`Audio::Setup()` reserves a fixed pool of **32** `IAudioSource` voices up front. Every OpenAL
implementation caps how many sources can exist, well below what a level wants to play, so reserving
the budget makes exhaustion a plain "the pool is empty" check instead of a device error mid-frame.

A voice is lent to an `AudioSource` only while it is actually making noise, and handed back when it
stops, finishes, is disabled or is destroyed. **A source that asks to play while all 32 are busy is
simply not heard** — it keeps asking, and is heard as soon as one frees up.

The two ends of the loan:

- The pool's end is a `ComponentHandle<AudioSource>`, which re-validates on access. That is what
  distinguishes a voice still in use from one whose component was destroyed — including the case
  where the ECS has since reused that component's slot for something else.
- The component's end is an `Audio::PlaybackHintData` (voice index, generation, mirrored playback
  position, seek flag), reached through `AudioSource::GetPlaybackHintData()`. It lives on the
  component for the same reason `ModelRenderer::GetRenderingHintData` does: the subsystem needs
  somewhere with the component's exact lifetime, and the component is the only thing that has one.
  **Nothing outside `Pine::Audio` should write to it.**

`AcquireVoice` in `Audio.cpp` is the only place a voice is handed out, so priority and stealing —
neither of which exists yet — go there and nowhere else.

## Mono, stereo and `Spatial`

Two independent switches, and they are easy to confuse:

- **Channel count** is fixed at import by `AudioImportConfiguration::ForceMono`.
- **Positioning** is per-component, via `AudioSource::SetSpatial`.

OpenAL only pans and attenuates **mono** buffers. That gives four combinations:

| Clip | `Spatial` | Result |
|---|---|---|
| Mono | on | Positioned: pans and fades with distance. World SFX. |
| Mono | off | Centred, constant volume. UI and 2D SFX. |
| Stereo | on | **Plays flat anyway.** OpenAL will not position a stereo buffer. |
| Stereo | off | Flat, stereo image intact. Music. |

So **background music is a stereo clip with `Spatial` off** — do not tick Force Mono for it, and put
the source on its own entity rather than on the player, since a non-spatial source ignores its
transform entirely. Force Mono exists for the opposite case: making a stereo *source file* usable as
a positioned 3D sound.

`SetSpatial(false)` marks the source relative to the listener and parks it at the origin, so it sits
at distance zero and the attenuation values stop mattering. That is all 2D audio is in OpenAL; there
is no separate path for it.

## Notes

- **The listener is device state, not an object** — there is one, and everything spatial is heard from it. The first enabled `AudioListener` wins, and `Audio::Update` warns once if it finds more than one. `AudioListener` carries master volume, applied on top of each source's own.
- **No listener means silence.** `Audio::Update` sets listener volume to zero rather than leaving the last listener's position in place, which would keep the level audible from wherever the ears happened to be standing when they were removed.
- In a first-person game put the `AudioListener` on the **camera**, not the player body, so the ears turn when you look. `Transform::GetForward`/`GetUp` compose the parent's rotation, so childing it to a camera that is itself childed to the player works.
- `AudioSource::SetPlaybackPosition` works while the source holds no voice; the seek waits until one is lent. That is what makes "play this from halfway" a single call.
- Playing an already-playing source does nothing. Call `AudioSource::Stop()` first to start a clip over.
- **Both components are bound to C#** (`Pine.World.Components.AudioSource` / `AudioListener`), and a
  clip is `Pine.Assets.Audio` there - named after `AssetType::Audio` rather than `Pine::AudioFile`,
  which is what the object factory looks a managed asset class up by. See
  [scripting.md](scripting.md).

⚠ **`Audio::Update` gates on `World::IsPaused()`**, pausing every voice rather than stopping it, so
leaving play mode and going back in continues a clip instead of restarting it. The editor keeps the
world paused outside play mode, which is also why you cannot audition a clip by pressing play on a
source in the editor — the asset panel needs a preview button (see `Audio/TODO.md`).

⚠ **It iterates disabled components on purpose** (`Components::Get<AudioSource>(true)`). A source
that has just been switched off still holds a voice, and the default iterator would skip the one
frame where it needs collecting.

⚠ **Engine shutdown does not return voices.** `Components::Shutdown()` frees the component pools
without running `OnDestroyed`, so `AudioSource::OnDestroyed` never releases its voice then;
`Audio::Shutdown()` destroys every voice in the pool itself.

⚠ **Hot-reloading a clip while it is playing leaks an AL buffer.** `AudioFile::Dispose` deletes the
buffer without checking whether a voice has it bound; OpenAL refuses the delete with
`AL_INVALID_OPERATION` and the id is dropped anyway. The voice is released on the next
`Audio::Update`, so nothing crashes and no sound is wrong — it is a leak per reload-while-playing,
listed in `Audio/TODO.md`.

## Verification

```sh
python3 Editor/src/DebugServer/Verification/verify-audio-playback.py --build build
```

A native probe (the `verify-physics-native.py` pattern) because `/edit` has no `AudioSource`
operation. It drives `Audio::Update()` over real time under the **null OpenAL backend**, which gives
a real context, real sources and a mixer at the real sample rate with no output device — so playback
is genuinely timed rather than mocked. Nine parts: every `IAudioSource` setter read back off
OpenAL, the listener following its entity and falling silent without one, a one-shot advancing and
ending on its own, looping, pause/stop/seek, voices handed back by disabled and destroyed sources, a
48-source crowd staying inside the pool and passing a freed voice on, a paused world holding its
sounds, and both components surviving a save and a load.

`verify-audio-asset.py` covers the import and `.passet` half separately, and
`verify-script-components.py` covers the C# bindings alongside the other bound components.

Related: [assets.md](assets.md), [scripting.md](scripting.md), [world-ecs.md](world-ecs.md).
`Engine/src/Pine/Audio/TODO.md` lists what is still missing — voice priority, `/edit` adapters, an
asset-panel preview button, and streaming for long clips.

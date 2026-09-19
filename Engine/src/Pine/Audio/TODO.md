## Audio to-do list

The asset half is done: `AudioFile` decodes wave and Ogg Vorbis at import, stores interleaved
16-bit PCM in its `.passet`, and uploads it to an `Audio::IAudioBuffer` on load.

Playback is now done too, and is worth describing rather than listing, because the shape of it is
what the rest of this file assumes.

`Audio/` is the device layer. `IAudioAPI` owns buffers, sources and the one listener; `OpenAL/` is
the only implementation. Nothing above this layer makes an `al*` call.

`Pine::Audio` is the subsystem on top. It reserves a fixed pool of `IAudioSource` voices at
`Audio::Setup()`, and `Audio::Update()` - called once a frame from `World::Update()` - makes the
device match the world: the listener moves to the active `AudioListener`, and every `AudioSource`
that wants to be heard is lent a voice, positioned and configured from the component's fields.

`AudioSource` and `AudioListener` are data. They hold what the author sets plus, on the source, the
state it has been asked to be in; `AudioSource::Play()` and friends write a field rather than
touching the device. The subsystem's own per-source state lives in an `Audio::PlaybackHintData`
parked on the component, the same way the renderer parks its per-object state on a `ModelRenderer`.

A failed `Audio::Setup()` is a warning rather than a fatal error, so the engine runs on a machine
with no output device. Clips still import and load there; they just get no buffer, and
`AudioFile::GetBuffer()` returns nullptr. Anything added below has to keep working in that state.

- [ ] Voice management. The pool is a flat budget of 32: a source that asks to play while they are
      all busy is simply not heard, and nothing decides that the footstep behind the player matters
      less than the one in front of them. Priority and stealing belong in `Audio.cpp`'s
      `AcquireVoice`, which is the only place that hands a voice out.
- [ ] Expose `AudioSource` and `AudioListener` to C#, the way `70f0caa` did for Light, Camera and
      Collider. `ScriptRuntime/World/Components/` has no audio in it yet.
- [ ] `/edit` adapters for both components, under `Editor/src/DebugServer/Editing/Components/`.
      Without them an audio scene cannot be built over HTTP, so verifying one means a native probe.
- [ ] A preview button in the editor's asset properties panel. There is a playback API to build it
      on now - a standalone `AudioSource`, or a voice taken straight off `IAudioAPI`.
- [ ] Free a voice before the clip it is playing is disposed. `AudioFile::Dispose` deletes its
      buffer without checking whether anything has it bound, so hot-reloading a clip mid-playback
      leaks one: OpenAL refuses the delete with `AL_INVALID_OPERATION` and the id is dropped
      anyway. Nothing crashes - the voice is released on the next `Audio::Update` - but the fix
      wants a `Audio::Internal` entry point that stops whatever is playing a given `AudioFile`.
- [ ] Doppler, if it turns out to be wanted. `IAudioSource` has no velocity, and nothing tracks how
      fast an emitter is moving.

### Formats

Clips are stored decoded, which is right for sound effects and expensive for music: a three minute
stereo track is about 30 MB of PCM, and PCM does not compress. Streaming long clips straight from
the stored Vorbis is the way out, and `AudioFile`'s payload carries its format explicitly so a
streamed mode can be added without migrating what is already imported.

- [ ] Stream long clips instead of holding them whole. Note that this is the one thing here that
      the voice pool's shape actually constrains: a streaming voice owns queued buffers of its own,
      so `Voice` would grow a streaming state rather than just pointing at an `AudioFile`.
- [ ] FLAC decoding, if it turns out to be wanted. Ogg FLAC and Speex are not supported either -
      `.oga` is accepted as Vorbis and rejected if it holds anything else.

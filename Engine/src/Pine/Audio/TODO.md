## Audio to-do list

The asset half is done: `AudioFile` stores an Ogg Vorbis source as its own bytes and decodes them on
load, stores a wave source as 16-bit PCM decoded at import, and uploads the result to an
`Audio::IAudioBuffer` on load.

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
- [ ] `/edit` adapters for both components, under `Editor/src/DebugServer/Editing/Components/`.
      Without them an audio scene cannot be built over HTTP, so verifying one means a native probe.
- [ ] Free a voice before the clip it is playing is disposed. `AudioFile::Dispose` deletes its
      buffer without checking whether anything has it bound, so hot-reloading a clip mid-playback
      leaks one: OpenAL refuses the delete with `AL_INVALID_OPERATION` and the id is dropped
      anyway. Nothing crashes - the voice is released on the next `Audio::Update` - but the fix
      wants a `Audio::Internal` entry point that stops whatever is playing a given `AudioFile`.
      The editor's clip preview already does this through `Audio::Internal::StopPreviewOf`; the
      pool's voices still need it. Note that `FreeVoice` only stops a voice without unbinding its
      clip, so a voice that has finished with a clip still blocks that clip's re-import upload.
- [ ] Doppler, if it turns out to be wanted. `IAudioSource` has no velocity, and nothing tracks how
      fast an emitter is moving.

### Formats

A Vorbis clip is stored as Vorbis, but every clip is still decoded whole on load, which is right
for sound effects and expensive for music: a three minute stereo track is about 30 MB of PCM in
memory, and decoding it is part of loading the project.

- [ ] Stream long clips from their stored Vorbis instead of decoding them whole. That would be a
      per-clip load setting next to `ForceMono`, and a clip that streams keeps its Vorbis bytes in
      memory rather than a buffer. This is the one thing here that the voice pool's shape actually
      constrains: a streaming voice owns queued buffers of its own, so `Voice` would grow a
      streaming state rather than just pointing at an `AudioFile`.
- [ ] Store wave sources as Vorbis too. That needs an encoder (libvorbis); `stb_vorbis` only decodes.
- [ ] FLAC decoding, if it turns out to be wanted. Ogg FLAC and Speex are not supported either -
      `.oga` is accepted as Vorbis and rejected if it holds anything else.

## Audio to-do list

The asset half is done: `AudioFile` decodes wave and Ogg Vorbis at import, stores interleaved
16-bit PCM in its `.passet`, and uploads it to an `Audio::IAudioBuffer` on load. What is left is
playback.

A failed `Audio::Setup()` is a warning rather than a fatal error, so the engine runs on a machine
with no output device. Clips still import and load there; they just get no buffer, and
`AudioFile::GetBuffer()` returns nullptr. Anything added below has to keep working in that state.

- [ ] Put sources behind `IAudioAPI` the way buffers are. `AudioSource` and the editor still call
      `al*` directly, so the backend seam only half exists.
- [ ] `Audio::Update()`, driven from the main loop: move the listener to the active
      `AudioListener`, move each playing source to its entity's transform. Nothing does this today,
      so a positioned sound never moves.
- [ ] Give `AudioSource` its `LoadData`/`SaveData`. None of its fields are serialized, so the clip
      an entity plays does not survive a save - and it needs `OnDestroyed` to free its source, since
      the ECS runs no destructors.
- [ ] Finish `AudioSource`: `Pause`/`Stop` are declared and never defined, and `m_Loop`,
      `m_Volume`, `m_PlaybackPosition` and `m_WorldPosition` are stored but unused.
- [ ] `AudioListener` does nothing at all - no `alListener*` call is ever made.
- [ ] A preview button in the editor's asset properties panel, once there is a playback API to
      build it on.
- [ ] 2D audio: in OpenAL that is a source marked relative to the listener at the origin, rather
      than a separate path.
- [ ] Voice management - OpenAL runs out of sources long before a game runs out of things to play.

### Formats

Clips are stored decoded, which is right for sound effects and expensive for music: a three minute
stereo track is about 30 MB of PCM, and PCM does not compress. Streaming long clips straight from
the stored Vorbis is the way out, and `AudioFile`'s payload carries its format explicitly so a
streamed mode can be added without migrating what is already imported.

- [ ] Stream long clips instead of holding them whole.
- [ ] FLAC decoding, if it turns out to be wanted. Ogg FLAC and Speex are not supported either -
      `.oga` is accepted as Vorbis and rejected if it holds anything else.

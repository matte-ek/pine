## Audio to-do list

- [ ] Play WaveFile
- [ ] Support FlacFile Decoding, loading and playing
- [ ] Support both Ogg FLAC and Ogg vorbis
- [ ] Write Ogg Vorbis Decoder, loader and play it
- [ ] 2D Audio -> 2D audio in OpenAL is just audio without 3D coord given to it.
- [ ] Error handling OpenAL

### Planning>
AudioListener <- AudioSource(s)
AudioSource has a loaded file or a stream associated with it, Handles OpenAL buffers etc.

Audiofile is loaded -> uses WaveFile/FlacFile/OggFile to get PCM Data -> Factory creates AudioSource from said data

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Pine/Assets/AudioFile/AudioFile.hpp"

namespace Pine::Importer::AudioLoader
{
    // A decoded clip, in the one representation every format decoder here produces: interleaved
    // signed 16-bit PCM. Whatever bit depth the source file used is gone by this point, so nothing
    // downstream of the loader has to know about source formats.
    struct AudioData
    {
        std::vector<std::int16_t> Samples;

        int Channels = 0;
        int SampleRate = 0;

        // Counted per channel, so Samples.size() is this times Channels.
        int SampleCount = 0;

        // How the clip should be stored. A Vorbis source also hands back the bytes it was decoded
        // from, since those are what gets stored; EncodedData is empty for anything stored as PCM.
        AudioEncoding Encoding = AudioEncoding::Pcm;
        std::vector<std::uint8_t> EncodedData;
    };

    // Decodes 'file' into 'audioData', picking the decoder from the file extension. Returns false
    // if no decoder here reads that extension, or if the file turns out to be malformed.
    bool LoadAudio(const std::filesystem::path& file, AudioData& audioData);

    // Decodes an Ogg Vorbis stream held in memory. This is also how AudioFile loads a clip that
    // was stored as Vorbis. Returns false, without logging, if the stream does not decode.
    bool DecodeVorbis(const std::uint8_t* data, std::size_t size, AudioData& audioData);

    // Collapses an interleaved multi-channel clip down to a single channel, in place.
    void DownmixToMono(AudioData& audioData);
}

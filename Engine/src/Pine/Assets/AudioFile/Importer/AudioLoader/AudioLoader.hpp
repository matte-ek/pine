#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Pine::Importer::AudioLoader
{
    // A decoded clip, in the one representation every format decoder here produces: interleaved
    // signed 16-bit PCM. Whatever bit depth or encoding the source file used is gone by this
    // point, so nothing downstream of the loader has to know about source formats.
    struct AudioData
    {
        std::vector<std::int16_t> Samples;

        int Channels = 0;
        int SampleRate = 0;

        // Counted per channel, so Samples.size() is this times Channels.
        int SampleCount = 0;
    };

    // Decodes 'file' into 'audioData', picking the decoder from the file extension. Returns false
    // if no decoder here reads that extension, or if the file turns out to be malformed.
    bool LoadAudio(const std::filesystem::path& file, AudioData& audioData);
}

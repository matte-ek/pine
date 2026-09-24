#include "Pine/Assets/AudioFile/Importer/AudioLoader/AudioLoader.hpp"

#include <climits>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"

// stb_vorbis decodes straight to interleaved 16-bit samples, which is what AudioData holds, so
// there is no conversion step here the way the wave loader needs one.
#include <stb/stb_vorbis.c>

namespace Pine::Importer::AudioLoader
{

bool DecodeVorbis(const std::uint8_t* data, const std::size_t size, AudioData& audioData)
{
    // stb_vorbis takes the length as an int.
    if (data == nullptr || size == 0 || size > INT_MAX)
    {
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    short* samples = nullptr;

    const auto sampleCount = stb_vorbis_decode_memory(data, static_cast<int>(size), &channels, &sampleRate, &samples);

    if (sampleCount <= 0 || samples == nullptr || channels <= 0 || sampleRate <= 0)
    {
        // stb_vorbis hands back a malloc'd block that is ours to release, even on failure.
        free(samples);

        return false;
    }

    audioData.Channels = channels;
    audioData.SampleRate = sampleRate;
    audioData.SampleCount = sampleCount;

    audioData.Samples.assign(samples, samples + static_cast<std::size_t>(sampleCount) * channels);

    free(samples);

    return true;
}

bool LoadAudioVorbis(const std::filesystem::path& file, AudioData& audioData)
{
    const auto fileData = File::ReadRaw(file);

    if (fileData.data == nullptr || fileData.size == 0)
    {
        PError(fmt::format("Failed to read Ogg Vorbis file {}", file.string()));
        return false;
    }

    const auto bytes = reinterpret_cast<const std::uint8_t*>(fileData.data);

    // Decoded here too, even though only the bytes are stored: a file that does not decode has to
    // fail its import, and the clip's length and channel count come from the decode.
    if (!DecodeVorbis(bytes, fileData.size, audioData))
    {
        PError(fmt::format("Failed to decode {} as Ogg Vorbis.", file.string()));
        return false;
    }

    audioData.Encoding = AudioEncoding::Vorbis;
    audioData.EncodedData.assign(bytes, bytes + fileData.size);

    return true;
}

}

#include "Pine/Assets/AudioFile/Importer/AudioLoader/AudioLoader.hpp"

#include "Pine/Core/Log/Log.hpp"

// stb_vorbis decodes straight to interleaved 16-bit samples, which is what AudioData holds, so
// there is no conversion step here the way the wave loader needs one.
#include <stb/stb_vorbis.c>

namespace Pine::Importer::AudioLoader
{

bool LoadAudioVorbis(const std::filesystem::path& file, AudioData& audioData)
{
    int channels = 0;
    int sampleRate = 0;
    short* samples = nullptr;

    const auto sampleCount = stb_vorbis_decode_filename(file.string().c_str(), &channels, &sampleRate, &samples);

    if (sampleCount <= 0 || samples == nullptr)
    {
        PError(fmt::format("Failed to decode {} as Ogg Vorbis.", file.string()));

        free(samples);

        return false;
    }

    if (channels <= 0 || sampleRate <= 0)
    {
        PError(fmt::format("Ogg Vorbis file {} declares no channels or no sample rate.", file.string()));

        free(samples);

        return false;
    }

    audioData.Channels = channels;
    audioData.SampleRate = sampleRate;
    audioData.SampleCount = sampleCount;

    audioData.Samples.assign(samples, samples + static_cast<std::size_t>(sampleCount) * channels);

    // stb_vorbis hands back a malloc'd block that is ours to release.
    free(samples);

    return true;
}

}

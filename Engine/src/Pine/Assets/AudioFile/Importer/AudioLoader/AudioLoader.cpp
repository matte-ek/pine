#include "AudioLoader.hpp"

#include "Pine/Core/String/String.hpp"

namespace Pine::Importer::AudioLoader
{
    bool LoadAudioWave(const std::filesystem::path& file, AudioData& audioData);
    bool LoadAudioVorbis(const std::filesystem::path& file, AudioData& audioData);
}

bool Pine::Importer::AudioLoader::LoadAudio(const std::filesystem::path& file, AudioData& audioData)
{
    const auto extension = String::ToLower(file.extension().string());

    if (extension == ".wav" || extension == ".wave")
    {
        return LoadAudioWave(file, audioData);
    }

    // '.oga' is an Ogg container that may hold something other than Vorbis (FLAC, Opus). The
    // decoder rejects those, which is the right answer until there is one that reads them.
    if (extension == ".ogg" || extension == ".oga")
    {
        return LoadAudioVorbis(file, audioData);
    }

    return false;
}

void Pine::Importer::AudioLoader::DownmixToMono(AudioData& audioData)
{
    const auto channels = audioData.Channels;

    // Averages across channels. Accumulating in a wider type matters: summing eight channels of
    // loud 16-bit samples overflows one, and the result of that is a burst of noise rather than a
    // quieter clip.
    for (int frame = 0; frame < audioData.SampleCount; frame++)
    {
        std::int32_t total = 0;

        for (int channel = 0; channel < channels; channel++)
        {
            total += audioData.Samples[static_cast<std::size_t>(frame) * channels + channel];
        }

        audioData.Samples[frame] = static_cast<std::int16_t>(total / channels);
    }

    audioData.Samples.resize(audioData.SampleCount);
    audioData.Channels = 1;
}

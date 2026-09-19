#include "AudioImporter.hpp"

#include "AudioLoader/AudioLoader.hpp"
#include "Pine/Core/Log/Log.hpp"

using namespace Pine;

namespace
{
    // Collapses an interleaved multi-channel clip down to a single channel by averaging across
    // channels, in place. Accumulating in a wider type matters: summing eight channels of loud
    // 16-bit samples overflows one, and the result of that is a burst of noise rather than a
    // quieter clip.
    void DownmixToMono(Importer::AudioLoader::AudioData& audioData)
    {
        const auto channels = audioData.Channels;

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
}

bool Pine::Importer::AudioImporter::Import(AudioFile* audioFile)
{
    if (audioFile->m_SourceFiles.size() != 1)
    {
        PWarning("Ignoring AudioFile import, an audio asset is built from exactly one source file.");
        return false;
    }

    const auto& file = audioFile->m_SourceFiles.front().FilePath;

    AudioLoader::AudioData audioData;

    if (!AudioLoader::LoadAudio(file, audioData))
    {
        return false;
    }

    // OpenAL only pans and attenuates mono buffers, so a stereo clip on a positioned source plays
    // flat wherever its entity is. ForceMono is how a stereo source file is made usable for 3D
    // sound; anything above two channels has no format to be stored as at all, so it is folded
    // down whether or not it was asked for.
    if (audioData.Channels > 2)
    {
        PWarning(fmt::format(
            "Audio file {} has {} channels, downmixing to mono.", file, audioData.Channels));

        DownmixToMono(audioData);
    }
    else if (audioFile->m_ImportConfiguration.ForceMono && audioData.Channels > 1)
    {
        DownmixToMono(audioData);
    }

    audioFile->m_Format = audioData.Channels == 2 ? Audio::AudioFormat::Stereo16 : Audio::AudioFormat::Mono16;
    audioFile->m_SampleRate = audioData.SampleRate;
    audioFile->m_SampleCount = audioData.SampleCount;
    audioFile->m_ImportSamples = std::move(audioData.Samples);

    PInfo(fmt::format("Imported audio {}: {}, {} Hz, {:.2f} seconds.",
        file,
        Audio::AudioFormatToString(audioFile->m_Format),
        audioFile->m_SampleRate,
        audioFile->GetDuration()));

    return true;
}

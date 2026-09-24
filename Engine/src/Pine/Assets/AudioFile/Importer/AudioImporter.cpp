#include "AudioImporter.hpp"

#include "AudioLoader/AudioLoader.hpp"
#include "Pine/Core/Log/Log.hpp"

using namespace Pine;

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
    }

    const auto playsAsMono = audioData.Channels != 2 || audioFile->m_ImportConfiguration.ForceMono;

    audioFile->m_Encoding = audioData.Encoding;
    audioFile->m_Format = playsAsMono ? Audio::AudioFormat::Mono16 : Audio::AudioFormat::Stereo16;
    audioFile->m_SampleRate = audioData.SampleRate;
    audioFile->m_SampleCount = audioData.SampleCount;

    // A Vorbis clip keeps its source's bytes as they are and is folded down on every load instead.
    if (audioData.Encoding == AudioEncoding::Vorbis)
    {
        audioFile->m_ImportEncodedData = std::move(audioData.EncodedData);
    }
    else
    {
        if (playsAsMono && audioData.Channels > 1)
        {
            AudioLoader::DownmixToMono(audioData);
        }

        audioFile->m_ImportSamples = std::move(audioData.Samples);
    }

    PInfo(fmt::format("Imported audio {}: {}, {}, {} Hz, {:.2f} seconds.",
        file,
        AudioEncodingToString(audioFile->m_Encoding),
        Audio::AudioFormatToString(audioFile->m_Format),
        audioFile->m_SampleRate,
        audioFile->GetDuration()));

    return true;
}

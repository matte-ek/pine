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

#include "AudioFile.hpp"

namespace Pine
{
    namespace
    {
        AudioFileFormat GetAudioFileFormat(const std::string& fileExtension)
        {
            static const std::map<std::string, AudioFileFormat> fileFormat =
            {
                {".wav", AudioFileFormat::Wave},
                {".wave", AudioFileFormat::Wave},
                {".flac", AudioFileFormat::Flac},
                {".ogg", AudioFileFormat::Ogg},
                {".oga", AudioFileFormat::Ogg},
                {".spx", AudioFileFormat::Ogg},
            };

            const auto foundExtension = fileFormat.find(fileExtension);

            return (foundExtension != fileFormat.end()) ? foundExtension->second : AudioFileFormat::Unknown;
        }
    }

    AudioFile::AudioFile()
    {
        m_Type = AssetType::Audio;

        // Loading and parsing the actual audio files can probably be done multi-threaded,
        // but let's do single threaded for now.
        m_LoadMode = AssetLoadMode::SingleThread;
    }

    bool AudioFile::ProcessFile()
    {
        if (m_FilePath.empty())
            return false;

        const auto fileExtension = m_FilePath.extension().string();

        m_AudioFileFormat = GetAudioFileFormat(fileExtension);

        switch (m_AudioFileFormat)
        {
            case AudioFileFormat::Wave:
                m_AudioObject = new Audio::WaveFile(m_FilePath.string());
                break;
            case AudioFileFormat::Flac:
                //m_AudioObject = new Pine::Audio::FlacFile(m_FilePath);
            case AudioFileFormat::Ogg:
                return false;
            case AudioFileFormat::Unknown:
                Log::Error(fmt::format("AudioFile::Setup(): Failed to get audio format from extension, {}", fileExtension));
                return false;
        }

        if (!m_AudioObject->Setup())
        {
            Log::Error("AudioFile::Setup(): Failed to setup audio object");
            m_AudioObject->Dispose();
            return false;
        }

        return true;
    }

    float AudioFile::GetDuration() const
    {
        return m_AudioObject->GetDuration();
    }

    ALuint AudioFile::GetNewSource() const
    {
        if (m_AudioObject == nullptr)
            return 0;

        ALuint sourceId = 0;
        alGenSources(1, &sourceId);
        if (alGetError() != AL_NO_ERROR)
            return 0;

        alSourcei(sourceId, AL_BUFFER, m_AudioObject->GetBufferID());
        if (alGetError() != AL_NO_ERROR) {
            alDeleteSources(1, &sourceId);
            return 0;
        }

        return sourceId;
    }

    bool AudioFile::LoadFromFile(AssetLoadStage stage)
    {
        const bool ret = ProcessFile();

        m_State = AssetState::Loaded;

        return ret;
    }

    void AudioFile::Dispose()
    {
        if (m_AudioObject)
            m_AudioObject->Dispose();

        delete m_AudioObject;

        m_AudioObject = nullptr;
    }
}

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

        m_AudioSource = new Audio::AudioSourceObject(m_AudioObject->GetID());

        return true;
    }

    bool AudioFile::Transcode()
    {
        return false;
    }

    void AudioFile::Play()
    {
        //m_AudioObject->Play();
        if (m_AudioSource)
        {
            alSourcePlay(m_AudioSource->GetID());
            m_AudioState = AudioState::Playing;
        }
    }

    void AudioFile::Stop()
    {
        //m_AudioObject->Play();
        if (m_AudioSource)
        {
            alSourceStop(m_AudioSource->GetID());
            m_AudioState = AudioState::Stopped;
        }
    }

    void AudioFile::Pause()
    {
        if (m_AudioSource)
        {
            alSourcePause(m_AudioSource->GetID());
            m_AudioState = AudioState::Paused;
        }
    }

    AudioState AudioFile::GetAudioState() const
    {
        return m_AudioState;
    }

    float AudioFile::GetTime() const
    {
        return m_AudioSource->GetSeconds();
    }

    float AudioFile::GetDuration() const
    {
        return m_AudioObject->GetDuration();
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
        delete m_AudioSource;

        m_AudioObject = nullptr;
        m_AudioSource = nullptr;
    }
}

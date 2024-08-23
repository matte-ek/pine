#include "AudioFile.hpp"

namespace Pine
{
    AudioFile::AudioFile()
    {
        m_Type = AssetType::Audio;
    }

    bool AudioFile::Setup()
    {
        if(m_FilePath.empty())
            return false;

        m_FileExtension = m_FilePath.extension().string();

        m_AudioFileFormat = GetAudioFileFormat();

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
                Log::Error("[AudioFile] Failed to get audio format from extension");
                return false;
        }

        if(!m_AudioObject->Setup())
        {
            Log::Error("[AudioFile] Failed to setup audio object");
            m_AudioObject->Dispose();
            return false;
        }

        m_AudioSource = new Audio::AudioSource(m_AudioObject->GetID());

        return true;
    }

    AudioFileFormat AudioFile::GetAudioFileFormat() const {
        static std::map<std::string, AudioFileFormat> fileFormat =
        {
                {".wav", AudioFileFormat::Wave},
                {".wave", AudioFileFormat::Wave},
                {".flac", AudioFileFormat::Flac},
                {".ogg", AudioFileFormat::Ogg},
                {".oga", AudioFileFormat::Ogg},
                {".spx", AudioFileFormat::Ogg},
        };

        auto foundExtension = fileFormat.find(m_FileExtension);

        return (foundExtension != fileFormat.end()) ? foundExtension->second : AudioFileFormat::Unknown;
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


    AudioState AudioFile::GetState() const
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

    bool AudioFile::LoadFromFile(Pine::AssetLoadStage stage)
    {
        Setup();
        m_State = AssetState::Loaded;
        // TODO: Actually load the asset from here.
        return true;
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

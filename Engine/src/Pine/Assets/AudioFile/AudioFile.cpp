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
                m_AudioObject = new Pine::Audio::WaveFile(m_FilePath.string());
                break;
            case AudioFileFormat::Flac:
                //m_AudioObject = new Pine::Audio::FlacFile(m_FilePath);
            case AudioFileFormat::Ogg:
                return false;
            case AudioFileFormat::Unknown:
                Pine::Log::Error("[AudioFile] Failed to get audio format from extension");
                return false;
        }

        if(!m_AudioObject->Setup())
        {
            return false;
        }

        m_AudioSource = new Audio::AudioSource(m_AudioObject->GetID());

        return true;
    }

    AudioFileFormat AudioFile::GetAudioFileFormat()
    {
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
            alSourcePlay(m_AudioSource->GetID());
    }

    bool AudioFile::LoadFromFile(Pine::AssetLoadStage stage)
    {
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

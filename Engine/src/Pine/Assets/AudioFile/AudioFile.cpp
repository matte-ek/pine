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

        m_FileExtension = m_FilePath.extension();

        m_AudioFileFormat = GetAudioFileFormat();

        switch (m_AudioFileFormat)
        {
            case AudioFileFormat::Wave:
                m_AudioObject = new Pine::Audio::WaveFile(m_FilePath);
                break;
            case AudioFileFormat::Flac:
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

    }

    bool AudioFile::LoadFromFile(Pine::AssetLoadStage stage)
    {
        m_State = AssetState::Loaded;
        return true;
    }

    void AudioFile::Dispose()
    {
            m_AudioObject->Dispose();
            delete m_AudioObject;
    }
}

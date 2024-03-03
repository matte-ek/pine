#include "AudioFile.hpp"

namespace Pine
{
    AudioFile::AudioFile()
    {
        m_Type = AssetType::Audio;
    }

    /**
     * @brief Performs the setup for the AudioFile object.
     *
     * This function is responsible for setting up the AudioFile object by performing the following steps:
     * 1. Open the audio file.
     * 2. Check the wave format of the audio file.
     * 3. Read the chunk data until both the FMT header and Data header have been read.
     *
     * @return True if the setup is successful, false otherwise.
     */
    bool AudioFile::Setup()
    {
        if (!OpenFile())
            return false;

        if (!CheckWaveFormat())
            return false;

        while (!m_FMTRead || !m_DataRead)
        {
            if (!ReadChunk())
            {
                return false;
            }
        }
        return true;
    }

    bool AudioFile::OpenFile()
    {
        m_File.open(m_FilePath.string(), std::ios_base::binary);

        if (!m_File.is_open())
        {
            Pine::Log::Warning("Failed to load AudioFile");
            return false;
        }
        return true;
    }

    bool AudioFile::CheckWaveFormat()
    {
        m_File.read((char *) (&m_RiffHeader), sizeof(m_RiffHeader));

        if (!m_File || memcmp(m_RiffHeader.chunk, "RIFF", 4) != 0 || memcmp(m_RiffHeader.format, "WAVE", 4) != 0)
        {
            Pine::Log::Error("Faulty wave formatting");
            return false;
        }
        return true;
    }

    bool AudioFile::ReadChunk()
    {
        if (!m_File.read((char *) (&m_TmpHeader), sizeof(m_TmpHeader)))
        {
            Pine::Log::Error("Couldn't read sound file header");
            return false;
        }

        if (memcmp(m_TmpHeader.chunk, "fmt ", 4) == 0)
        {
            if (m_TmpHeader.chunkSize >= sizeof(m_FMTHeader) && !m_FMTRead)
            {
                m_File.read((char *) (&m_FMTHeader), sizeof(m_FMTHeader));
                m_FMTRead = true;
            }
        }
        else if (memcmp(m_TmpHeader.chunk, "data", 4) == 0)
        {
            m_DataHeader.size = m_TmpHeader.chunkSize;
            strncpy(m_DataHeader.chunk, m_TmpHeader.chunk, 4);
            if (m_DataHeader.data != nullptr)
            {
                delete[] m_DataHeader.data;
                m_DataHeader.data = nullptr;
            }

            m_DataHeader.data = new char[m_DataHeader.size];
            m_File.read(m_DataHeader.data, m_DataHeader.size);
            m_DataRead = true;
        }
        else
        {
            m_File.seekg(m_TmpHeader.chunkSize, std::ios_base::cur);
        }
        return true;
    }

    bool AudioFile::LoadFromFile(Pine::AssetLoadStage stage)
    {
        m_State = AssetState::Loaded;
        return true;
    }

    void AudioFile::Dispose()
    {
        delete[] m_DataHeader.data;
    }
}
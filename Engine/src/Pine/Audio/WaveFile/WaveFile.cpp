#include "WaveFile.hpp"

#include <utility>

namespace Pine::Audio
{

    WaveFile::WaveFile(std::string filePath) : m_FilePath(std::move(filePath)) {}


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
    bool WaveFile::Setup()
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

        alGenBuffers(1, &m_ALBuffer);

        if (!LoadAudioData())
            return false;

        return true;
    }

    bool WaveFile::OpenFile()
    {
        m_File.open(m_FilePath, std::ios_base::binary);

        if (!m_File.is_open())
        {
            Pine::Log::Warning("Failed to load AudioWaveFile");
            return false;
        }

        return true;
    }

    bool WaveFile::CheckWaveFormat()
    {
        m_File.read((char *) (&m_RiffHeader), sizeof(m_RiffHeader));

        if (!m_File || memcmp(m_RiffHeader.chunk, "RIFF", 4) != 0 || memcmp(m_RiffHeader.format, "WAVE", 4) != 0)
        {
            Pine::Log::Error("Faulty wave formatting");
            return false;
        }

        return true;
    }

    bool WaveFile::ReadChunk()
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

    bool WaveFile::LoadAudioData()
    {
        ALenum format = 0;

        if (m_FMTHeader.bitsPerSample == 8)
            format = (m_FMTHeader.numChannels == 1) ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
        else if (m_FMTHeader.bitsPerSample == 16)
            format = (m_FMTHeader.numChannels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        ALvoid* data = static_cast<ALvoid*>(m_DataHeader.data);

        alBufferData(m_ALBuffer, format, data, m_DataHeader.size, m_FMTHeader.sampleRate);

        /* Make this a utility function */
        ALenum err = alGetError();
        if(err != AL_NO_ERROR)
        {
            std::string errStr;
            switch(err)
            {
                case AL_INVALID_NAME:
                    errStr = "AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function";
                    break;
                case AL_INVALID_ENUM:
                    errStr = "AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function";
                    break;
                case AL_INVALID_VALUE:
                    errStr = "AL_INVALID_VALUE: an invalid value was passed to an OpenAL function";
                    break;
                case AL_INVALID_OPERATION:
                    errStr = "AL_INVALID_OPERATION: the requested operation is not valid";
                    break;
                case AL_OUT_OF_MEMORY:
                    errStr = "AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory";
                    break;
                default:
                    errStr = "UNKNOWN ERROR: Unknown OpenAL error";
            }

            Pine::Log::Error(errStr);

            return false;
        }

        return true;
    }

    bool WaveFile::Transcode()
    {
        return false;
    }

    void WaveFile::Play()
    {

    }

    void WaveFile::Stop()
    {

    }

    int WaveFile::GetID()
    {
        return static_cast<int>(m_ALBuffer);
    }

    void WaveFile::Dispose()
    {
        delete[] m_DataHeader.data;
        m_DataHeader.data = nullptr;
        alDeleteBuffers(1, &m_ALBuffer);
    }
}
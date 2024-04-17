#include "FlacFile.hpp"
#include "Pine/Core/Log/Log.hpp"

namespace Pine::Audio
{
    FlacFile::FlacFile(std::string filePath) : m_FilePath(std::move(filePath)) {}

    bool FlacFile::OpenFile()
    {
        m_File.open(m_FilePath, std::ios_base::binary);
        if (!m_File.is_open())
        {
            Pine::Log::Warning("Failed to load AudioFile");
            return false;
        }
        return true;
    }

    bool FlacFile::Setup()
    {
        if (!OpenFile())
            return false;

        m_Decoder = new FLAC::Decoder();
        if (!InitDecoder())
            return false;


        return true;
    }

    bool FlacFile::InitDecoder()
    {
        FLAC__StreamDecoderInitStatus init_status = m_Decoder->init(m_FilePath);
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        {
            Pine::Log::Warning("Failed to initialize FLAC decoder");
            return false;
        }
        m_Decoder->set_md5_checking(true);
        return true;
    }

    void FlacFile::Dispose()
    {
        delete[] m_Decoder;
        m_Decoder = nullptr;
    }
}
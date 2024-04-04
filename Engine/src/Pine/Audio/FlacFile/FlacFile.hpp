#pragma once

#include <cstdint>
#include <fstream>
#include <FLAC++/all.h>
#include <memory>
#include "Pine/Audio/FlacDecoder/FlacDecoder.hpp"
#include "Pine/Audio/Interfaces/IAudioObject.hpp"

namespace Pine::Audio
{
    class FlacFile : public IAudioObject
    {
    private:
        const std::string m_FilePath;
        std::ifstream m_File;
        FLAC::Decoder* m_Decoder = nullptr;

        bool OpenFile();
        bool InitDecoder();
        bool Decode();
        void CloseDecoder();
    public:
        explicit FlacFile(std::string filePath);

        bool Setup() override;
        void Play() override;
        void Stop() override;
        bool Transcode() override;
        void Dispose() override;
    };
}
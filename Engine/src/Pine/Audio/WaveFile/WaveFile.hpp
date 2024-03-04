#pragma once

#include <cstdint>
#include <fstream>
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Audio/Interfaces/IAudioObject.hpp"

#pragma pack(push, 1)

struct RIFFHeader {
    char chunk[4];
    uint32_t chunkSize;
    char format[4];
};

struct FMTHeader {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct DataHeader {
    char chunk[4];
    char* data;
    uint32_t size;
};

struct TmpHeader {
    char chunk[4];
    uint32_t chunkSize;
};
#pragma pack(pop)

namespace Pine::Audio
{
    class WaveFile : public IAudioObject
    {
    private:
        RIFFHeader m_RiffHeader{};
        FMTHeader m_FMTHeader{};
        TmpHeader m_TmpHeader{};
        DataHeader m_DataHeader{};

        const std::string m_FilePath;
        std::ifstream m_File;

        bool m_FMTRead = false;
        bool m_DataRead = false;

        bool OpenFile();
        bool CheckWaveFormat();
        bool ReadChunk();

    public:
        explicit WaveFile(std::string filePath);

        bool Setup() override;
        bool Transcode() override;
        void Dispose() override;

    };
}


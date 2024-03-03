#pragma once

#include <cstring>
#include <cstdio>
#include <fstream>
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

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

namespace Pine
{
    class AudioFile : public IAsset
    {
    private:
        RIFFHeader m_RiffHeader{};
        FMTHeader m_FMTHeader{};
        TmpHeader m_TmpHeader{};
        DataHeader m_DataHeader{};

        std::ifstream m_File;

        bool m_FMTRead = false;
        bool m_DataRead = false;

        bool OpenFile();
        bool CheckWaveFormat();
        bool ReadChunk();

    public:
        AudioFile();

        bool Setup();
        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };
}



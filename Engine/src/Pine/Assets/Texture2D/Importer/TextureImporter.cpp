#include "TextureImporter.hpp"

#ifdef PINE_RUNTIME

bool Pine::Importer::TextureImporter::Import(Texture2D* texture)
{
    // Stub.
    return false;
}

#else

#include <stb_image.h>

#include "nvtt/nvtt.h"
#include "nvtt/nvtt_wrapper.h"

using namespace Pine;

namespace
{

    void* LoadImageBytes(const std::string& fileName, int& width, int& height, int& channels, Graphics::TextureFormat& textureFormat)
    {
        auto data = stbi_load(fileName.c_str(), &width, &height, &channels, 0);

        if (data == nullptr)
        {
            Log::Error(fmt::format("Failed to load texture {}", fileName));

            return nullptr;
        }

        switch (channels)
        {
            case 1:
                textureFormat = Graphics::TextureFormat::SingleChannel;
                break;
            case 3:
                textureFormat = Graphics::TextureFormat::RGB;
                break;
            case 4:
                textureFormat = Graphics::TextureFormat::RGBA;
                break;
            default:
                Log::Error(fmt::format("Unknown texture format, channel count: {}", channels));
                stbi_image_free(data);
                return nullptr;
        }

        return data;
    }

    /*
    nvtt::InputFormat TranslateTextureFormat(Graphics::TextureFormat textureFormat)
    {
        switch (textureFormat)
        {
            case Graphics::TextureFormat::RGB:
                return nvtt::InputFormat_BGRA_8UB;
            case Graphics::TextureFormat::RGBA:
                return CMP_FORMAT_RGBA_8888;
            case Graphics::TextureFormat::SingleChannel:
                return CMP_FORMAT_R_8;
            default:
                throw std::invalid_argument("Invalid texture format");
        }
    }
    */

    Graphics::TextureCompressionFormat DetermineCompressionFormat(TextureUsageHint textureUsageHint)
    {
        switch (textureUsageHint)
        {
            case TextureUsageHint::Uncompressed:
                return Graphics::TextureCompressionFormat::Raw;
            case TextureUsageHint::Albedo:
                return Graphics::TextureCompressionFormat::BC7;
            case TextureUsageHint::AlbedoFaster:
                return Graphics::TextureCompressionFormat::BC1;
            case TextureUsageHint::NormalMap:
                return Graphics::TextureCompressionFormat::BC5;
            case TextureUsageHint::Grayscale:
                return Graphics::TextureCompressionFormat::BC4;
            default:
                throw std::invalid_argument("Invalid usage hint");
        }
    }

    NvttFormat TranslateCompressionFormat(Graphics::TextureCompressionFormat textureCompressionFormat, int channels)
    {
        switch (textureCompressionFormat)
        {
            case Graphics::TextureCompressionFormat::BC1:
                return channels == 3 ? NVTT_Format_BC1 : NVTT_Format_BC1a;
            case Graphics::TextureCompressionFormat::BC4:
                return NVTT_Format_BC4;
            case Graphics::TextureCompressionFormat::BC5:
                return NVTT_Format_BC5;
            case Graphics::TextureCompressionFormat::BC7:
                return NVTT_Format_BC7;
            default:
                throw std::invalid_argument("Invalid texture compression format");
        }
    }

    NvttQuality TranslateQuality(TextureCompressionQuality textureCompressionQuality)
    {
        switch (textureCompressionQuality)
        {
            case TextureCompressionQuality::Normal:
                return NVTT_Quality_Normal;
            case TextureCompressionQuality::Fastest:
                return NVTT_Quality_Fastest;
            case TextureCompressionQuality::Production:
                return NVTT_Quality_Production;
        }

        throw std::invalid_argument("Invalid texture quality.");
    }
}

bool Importer::TextureImporter::Import(Texture2D* texture)
{
    if (texture->m_SourceFiles.empty() || texture->m_SourceFiles.size() > 1)
    {
        Pine::Log::Warning("Ignoring Texture2D import, too many source files.");
        return false;
    }

    const auto& file = texture->m_SourceFiles.front();

    Log::Info(fmt::format("Importing Texture2D from source file {}...", file.FilePath));

    int width, height, channels;
    Graphics::TextureFormat format;

    void* imageDataPtr = LoadImageBytes(file.FilePath, width, height, channels, format);

    if (imageDataPtr == nullptr)
    {
        return false;
    }

    texture->m_Width = width;
    texture->m_Height = height;
    texture->m_Format = format;

    auto compressionFormat = DetermineCompressionFormat(texture->m_ImportConfiguration.UsageHint);

    if (compressionFormat == Graphics::TextureCompressionFormat::Raw)
    {
        texture->m_TextureData = imageDataPtr;
        texture->m_TextureDataSize = width * height * channels;
        texture->m_CompressionFormat = Graphics::TextureCompressionFormat::Raw;

        return true;
    }

    const auto nvCompressionLevel = TranslateCompressionFormat(compressionFormat, channels);
    const auto nvQuality = TranslateQuality(texture->m_ImportConfiguration.CompressionQuality);

    // Prepare texture for encoding
    NvttRefImage ref;

    ref.data = imageDataPtr;
    ref.width = width;
    ref.height = height;
    ref.num_channels = channels;
    ref.depth = 1;

    ref.channel_swizzle[0] = NVTT_ChannelOrder_Red;
    ref.channel_swizzle[1] = NVTT_ChannelOrder_Green;
    ref.channel_swizzle[2] = NVTT_ChannelOrder_Blue;
    ref.channel_swizzle[3] = NVTT_ChannelOrder_Alpha;

    ref.channel_interleave = NVTT_True;

    auto cpuInputBuffer = nvttCreateCPUInputBuffer(
        &ref,
        NVTT_ValueType_UINT8,
        1,
        4, 4,
        1.f, 1.f, 1.f, 1.f,
        nullptr, nullptr);

    // Create context and try to enable CUDA
    auto context = nvttCreateContext();
    nvttSetContextCudaAcceleration(context, NVTT_True);
    if (!nvttIsCudaSupported())
    {
        Log::Warning("CUDA unsupported during compression.");
    }

    // Prepare compression options so we can estimate the buffer size
    auto options = nvttCreateCompressionOptions();

    nvttResetCompressionOptions(options);
    nvttSetCompressionOptionsFormat(options, nvCompressionLevel);
    nvttSetCompressionOptionsQuality(options, nvQuality);
    nvttSetCompressionOptionsPixelType(options, NVTT_PixelType_UnsignedNorm);
    nvttSetCompressionOptionsPixelFormat(options, 32, 0, 0, 0, 0);

    auto dataSize = nvttContextEstimateSizeData(context, ref.width, ref.height, ref.depth, 1, options);
    auto data = malloc(dataSize);

    NvttEncodeSettings settings;

    settings.encode_flags = 0;
    settings.format = nvCompressionLevel;
    settings.quality = nvQuality;
    settings.sType = NVTT_EncodeSettings_Version_1;
    settings.timing_context = nullptr;
    settings.rgb_pixel_type = NVTT_PixelType_UnsignedNorm;
    settings.encode_flags = NVTT_EncodeFlags_UseGPU;

    if (nvttEncodeCPU(cpuInputBuffer, data, &settings) != NVTT_True)
    {
        return false;
    }

    texture->m_TextureData = data;
    texture->m_TextureDataSize = dataSize;
    texture->m_CompressionFormat = compressionFormat;

    free(imageDataPtr);

    nvttDestroyCompressionOptions(options);
    nvttDestroyCPUInputBuffer(cpuInputBuffer);
    nvttDestroyContext(context);

    return true;
}

#endif
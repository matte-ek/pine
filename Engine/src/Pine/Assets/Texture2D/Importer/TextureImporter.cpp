#include "TextureImporter.hpp"

#include <stb_image.h>

#include "compressonator.h"

using namespace Pine;

namespace
{

    bool LoadImageBytes(const std::string& fileName, void** data, int& width, int& height, int& channels, Graphics::TextureFormat& textureFormat)
    {
        *data = stbi_load(fileName.c_str(), &width, &height, &channels, 0);

        if (*data == nullptr)
        {
            Log::Error(fmt::format("Failed to load texture {}", fileName));

            stbi_image_free(*data);
            return false;
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
                return false;
        }

        return true;
    }

    CMP_FORMAT TranslateTextureFormat(Graphics::TextureFormat textureFormat)
    {
        switch (textureFormat)
        {
            case Graphics::TextureFormat::RGB:
                return CMP_FORMAT_RGB_888;
            case Graphics::TextureFormat::RGBA:
                return CMP_FORMAT_RGBA_8888;
            case Graphics::TextureFormat::SingleChannel:
                return CMP_FORMAT_R_8;
            default:
                throw std::invalid_argument("Invalid texture format");
        }
    }

    TextureCompressionFormat DetermineCompressionFormat(TextureUsageHint textureUsageHint)
    {
        switch (textureUsageHint)
        {
            case TextureUsageHint::Uncompressed:
                return TextureCompressionFormat::Raw;
            case TextureUsageHint::Albedo:
                return TextureCompressionFormat::BC7;
            case TextureUsageHint::AlbedoFaster:
                return TextureCompressionFormat::BC1;
            case TextureUsageHint::NormalMap:
                return TextureCompressionFormat::BC5;
            case TextureUsageHint::Grayscale:
                return TextureCompressionFormat::BC4;
            default:
                throw std::invalid_argument("Invalid usage hint");
        }
    }

    CMP_FORMAT TranslateCompressionFormat(TextureCompressionFormat textureCompressionFormat)
    {
        switch (textureCompressionFormat)
        {
            case TextureCompressionFormat::BC1:
                return CMP_FORMAT_BC1;
            case TextureCompressionFormat::BC4:
                return CMP_FORMAT_BC4;
            case TextureCompressionFormat::BC5:
                return CMP_FORMAT_BC5;
            case TextureCompressionFormat::BC7:
                return CMP_FORMAT_BC7;
            default:
                throw std::invalid_argument("Invalid texture compression format");
        }
    }

    CMP_Texture CreateCmpSourceTexture(int width, int height, int channels, void* imageDataPtr, Graphics::TextureFormat format)
    {
        CMP_Texture cmpTexture{};

        cmpTexture.dwSize = sizeof(cmpTexture);
        cmpTexture.dwWidth = width;
        cmpTexture.dwHeight = height;
        cmpTexture.dwPitch = width * channels;
        cmpTexture.format = TranslateTextureFormat(format);
        cmpTexture.pData = static_cast<CMP_BYTE*>(imageDataPtr);

        return cmpTexture;
    }

    CMP_Texture CreateCmpDestinationTexture(int width, int height, TextureCompressionFormat textureCompressionFormat, std::byte* destinationBuffer, size_t destinationBufferSize)
    {
        CMP_Texture cmpTexture{};

        cmpTexture.dwSize = sizeof(cmpTexture);
        cmpTexture.dwWidth = width;
        cmpTexture.dwHeight = height;
        cmpTexture.format = TranslateCompressionFormat(textureCompressionFormat);
        cmpTexture.pData = reinterpret_cast<CMP_BYTE*>(destinationBuffer);
        cmpTexture.dwDataSize = destinationBufferSize;

        return cmpTexture;
    }

}

#ifdef PINE_RUNTIME

bool Pine::Importer::TextureImporter::Import(Texture2D* texture)
{
    // Stub.
    return false;
}

#else

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
    void* imageDataPtr;
    Graphics::TextureFormat format;

    if (!LoadImageBytes(file.FilePath, &imageDataPtr, width, height, channels, format))
    {
        return false;
    }

    texture->m_Width = width;
    texture->m_Height = height;
    texture->m_Format = format;

    auto compressionFormat = DetermineCompressionFormat(texture->m_ImportConfiguration.UsageHint);

    if (compressionFormat == TextureCompressionFormat::Raw)
    {
        texture->m_TextureData = imageDataPtr;
        texture->m_TextureDataSize = width * height * channels;

        return true;
    }

    uint32_t blockSize = 16;
    uint32_t numBlocksX = (width + 3) / 4;
    uint32_t numBlocksY = (height + 3) / 4;
    size_t compressedSize = numBlocksX * numBlocksY * blockSize;

    // I would use new here, but to maintain compatibility with stb
    auto compressedData = static_cast<std::byte*>(malloc(compressedSize));

    auto sourceTexture = CreateCmpSourceTexture(width, height, channels, imageDataPtr, format);
    auto destTexture = CreateCmpDestinationTexture(width, height, compressionFormat, compressedData, compressedSize);

    CMP_CompressOptions compressOptions{};
    compressOptions.dwSize = sizeof(compressOptions);

    auto ret = CMP_ConvertTexture(&sourceTexture, &destTexture, nullptr, nullptr);
    if (ret != CMP_OK)
    {
        Pine::Log::Error(fmt::format("CMP_ConvertTexture failed: {}", static_cast<int>(ret)));

        free(imageDataPtr);
        free(compressedData);

        return false;
    }

    texture->m_TextureData = compressedData;
    texture->m_TextureDataSize = compressedSize;

    return true;
}

#endif
#include "TextureImporter.hpp"

#include "ImageLoader/ImageLoader.hpp"
#include "Pine/Assets/Assets.hpp"

#ifdef PINE_RUNTIME

bool Pine::Importer::TextureImporter::Import(Texture2D* texture)
{
    // Stub.
    return false;
}

#else

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <vector>

#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>
#include <stb/stb_image_write.h>

#include "nvtt/nvtt.h"
#include "nvtt/nvtt_wrapper.h"

using namespace Pine;

namespace
{

    void* LoadImageBytes(const std::string& fileName, int& width, int& height, int& channels, Graphics::TextureFormat& textureFormat)
    {
        const auto data = Importer::ImageLoader::LoadImage(fileName, width,  height, channels);

        if (data == nullptr)
        {
            PError(fmt::format("Failed to load texture {}", fileName));

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
                PError(fmt::format("Unknown texture format, channel count: {}", channels));
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

    // A pixel at or above this is treated as solid, at or below the other as a hole. The slack on
    // either end is for textures that were authored through a tool that left 254 or 1 behind.
    constexpr unsigned char OpaqueAlphaThreshold = 250;
    constexpr unsigned char ClearAlphaThreshold = 5;

    // How much of a texture has to be partially see-through before it is worth asking what that
    // partial alpha is doing. Below this the texture is a mask whatever the shape of it.
    constexpr float TransparentPixelFraction = 0.1f;

    // ...and how many of those partially see-through pixels have to sit inside a fading region,
    // rather than on the antialiased outline of a cutout, before the texture counts as Transparent.
    // Measured across foliage the two populations are nowhere near each other: grass, ferns and
    // pine branches come in under 30%, a genuinely translucent texture over 95%.
    constexpr float InteriorPartialPixelFraction = 0.5f;

    // What a single pixel does with its alpha channel.
    enum class PixelAlpha
    {
        Solid,
        Partial,
        Hole
    };

    std::vector<PixelAlpha> ClassifyPixels(
        const void* imageData,
        const unsigned int width,
        const unsigned int height,
        const unsigned int channels)
    {
        const auto* pixels = static_cast<const unsigned char*>(imageData);
        const size_t pixelCount = static_cast<size_t>(width) * height;

        std::vector<PixelAlpha> classified(pixelCount);

        for (size_t i{}; i < pixelCount; i++)
        {
            const auto alpha = pixels[i * channels + 3];

            if (alpha >= OpaqueAlphaThreshold)
            {
                classified[i] = PixelAlpha::Solid;
                continue;
            }

            if (alpha <= ClearAlphaThreshold)
            {
                classified[i] = PixelAlpha::Hole;
                continue;
            }

            classified[i] = PixelAlpha::Partial;
        }

        return classified;
    }

    // A partially transparent pixel next to a solid or a fully clear one is almost certainly on the
    // antialiased outline of a cutout. One with nothing but partial pixels around it sits inside a
    // region that genuinely fades, and that is what needs blending to look right.
    bool IsInteriorPartialPixel(
        const std::vector<PixelAlpha>& classified,
        const unsigned int width,
        const unsigned int height,
        const unsigned int x,
        const unsigned int y)
    {
        for (int offsetY = -1; offsetY <= 1; offsetY++)
        {
            for (int offsetX = -1; offsetX <= 1; offsetX++)
            {
                const int neighbourX = static_cast<int>(x) + offsetX;
                const int neighbourY = static_cast<int>(y) + offsetY;

                const bool outsideImage =
                    neighbourX < 0 || neighbourX >= static_cast<int>(width) ||
                    neighbourY < 0 || neighbourY >= static_cast<int>(height);

                if (outsideImage)
                {
                    continue;
                }

                if (classified[static_cast<size_t>(neighbourY) * width + neighbourX] != PixelAlpha::Partial)
                {
                    return false;
                }
            }
        }

        return true;
    }

    size_t CountInteriorPartialPixels(
        const std::vector<PixelAlpha>& classified,
        const unsigned int width,
        const unsigned int height)
    {
        size_t interiorPartialPixels = 0;

        for (unsigned int y{}; y < height; y++)
        {
            for (unsigned int x{}; x < width; x++)
            {
                if (classified[static_cast<size_t>(y) * width + x] != PixelAlpha::Partial)
                {
                    continue;
                }

                if (IsInteriorPartialPixel(classified, width, height, x, y))
                {
                    interiorPartialPixels++;
                }
            }
        }

        return interiorPartialPixels;
    }

    // Works out what a texture does with its alpha channel, from the source image - before block
    // compression gets a chance to quantise it away.
    //
    // The question that decides Cutout against Transparent is not how much partial alpha there is,
    // but how it is laid out. Foliage - grass, ferns, a pine branch - is all perimeter, so its
    // antialiased outline alone covers a tenth to a third of the image while the texture is still a
    // plain mask. It belongs in the discard pass: ShadowPass builds draw lists for Opaque and
    // Discard only, so calling it Transparent would cost it its shadow. Glass, water and smoke fade
    // across their interior instead, and that is what this looks for.
    TextureAlphaMode DetectAlphaMode(
        const void* imageData,
        const unsigned int width,
        const unsigned int height,
        const unsigned int channels)
    {
        if (channels < 4)
        {
            return TextureAlphaMode::Opaque;
        }

        const auto classified = ClassifyPixels(imageData, width, height, channels);
        const size_t pixelCount = classified.size();

        size_t holePixels = 0;
        size_t partialPixels = 0;

        for (const auto pixelAlpha : classified)
        {
            if (pixelAlpha == PixelAlpha::Hole)
            {
                holePixels++;
            }
            else if (pixelAlpha == PixelAlpha::Partial)
            {
                partialPixels++;
            }
        }

        if (holePixels == 0 && partialPixels == 0)
        {
            return TextureAlphaMode::Opaque;
        }

        // Too little partial alpha to be anything but a mask, so skip the walk over the image.
        if (static_cast<float>(partialPixels) <= static_cast<float>(pixelCount) * TransparentPixelFraction)
        {
            return TextureAlphaMode::Cutout;
        }

        const auto interiorPartialPixels = CountInteriorPartialPixels(classified, width, height);

        if (static_cast<float>(interiorPartialPixels) > static_cast<float>(partialPixels) * InteriorPartialPixelFraction)
        {
            return TextureAlphaMode::Transparent;
        }

        return TextureAlphaMode::Cutout;
    }

    Graphics::TextureCompressionFormat DetermineCompressionFormat(const TextureUsageHint textureUsageHint)
    {
        switch (textureUsageHint)
        {
            case TextureUsageHint::Uncompressed:
            case TextureUsageHint::DataMap:
                return Graphics::TextureCompressionFormat::Raw;
            case TextureUsageHint::Albedo:
            case TextureUsageHint::LinearColor:
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

    NvttFormat TranslateCompressionFormat(
        const Graphics::TextureCompressionFormat textureCompressionFormat,
        const TextureAlphaMode alphaMode)
    {
        switch (textureCompressionFormat)
        {
            // BC1 carries no alpha at all and BC1a carries a single bit of it, at the cost of some
            // colour quality in every block that uses the punch-through mode. Which one a texture
            // wants is a question about its alpha, not about its channel count: the PNG loader
            // hands every image back as RGBA, opaque ones included.
            case Graphics::TextureCompressionFormat::BC1:
                return alphaMode == TextureAlphaMode::Opaque ? NVTT_Format_BC1 : NVTT_Format_BC1a;
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

    NvttQuality TranslateQuality(const TextureCompressionQuality textureCompressionQuality)
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

TextureImportData Importer::TextureImporter::CompressImage(
    Texture2D* texture,
    const nvtt::Context* context,
    const void* inputData,
    const unsigned int width,
    const unsigned int height,
    const unsigned int channels,
    const bool hasGpuAcceleration)
{
    auto compressionFormat = DetermineCompressionFormat(texture->m_ImportConfiguration.UsageHint);

    if (compressionFormat == Graphics::TextureCompressionFormat::Raw)
    {
        TextureImportData ret;

        ret.m_TextureDataSize = width * height * channels;
        ret.m_TextureData = malloc(ret.m_TextureDataSize);

        memcpy(ret.m_TextureData, inputData, ret.m_TextureDataSize);

        ret.m_Width = width;
        ret.m_Height = height;

        texture->m_CompressionFormat = Graphics::TextureCompressionFormat::Raw;

        return ret;
    }

    if (context == nullptr)
    {
        return {nullptr, 0, 0, 0};
    }

    texture->m_CompressionFormat = compressionFormat;

    const auto nvCompressionLevel = TranslateCompressionFormat(compressionFormat, texture->m_AlphaMode);
    const auto nvQuality = TranslateQuality(texture->m_ImportConfiguration.CompressionQuality);

    // Prepare texture for encoding
    NvttRefImage ref;

    ref.data = inputData;
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
    settings.encode_flags = hasGpuAcceleration ? NVTT_EncodeFlags_UseGPU : NVTT_EncodeFlags_None;

    if (nvttEncodeCPU(cpuInputBuffer, data, &settings) != NVTT_True)
    {
        return {nullptr, 0};
    }

    nvttDestroyCompressionOptions(options);
    nvttDestroyCPUInputBuffer(cpuInputBuffer);

    return {data, static_cast<size_t>(dataSize), width, height};
}

bool Importer::TextureImporter::Import(Texture2D* texture)
{
    if (texture->m_SourceFiles.empty() || texture->m_SourceFiles.size() > 1)
    {
        PWarning("Ignoring Texture2D import, too many source files.");
        return false;
    }

    //texture->m_ImportConfiguration.UsageHint = TextureUsageHint::Uncompressed;

    const auto& file = texture->m_SourceFiles.front().FilePath;

    int width, height, channels;
    Graphics::TextureFormat format;

    void* imageDataPtr = LoadImageBytes(file, width, height, channels, format);

    if (imageDataPtr == nullptr)
    {
        return false;
    }

    texture->m_Width = width;
    texture->m_Height = height;
    texture->m_Format = format;
    texture->m_AlphaMode = DetectAlphaMode(imageDataPtr, width, height, channels);

    // BC1 stores one bit of alpha, which is enough for a cutout and nowhere near enough for a
    // surface that fades. Move a texture that actually needs the gradient onto BC7. This goes in
    // through the usage hint's own ranking, so it can override the default and the file-name
    // heuristic, but never a hint the model file supplied or the user picked by hand.
    if (texture->m_AlphaMode == TextureAlphaMode::Transparent &&
        texture->m_ImportConfiguration.UsageHint == TextureUsageHint::AlbedoFaster)
    {
        ApplyTextureUsageHint(
            texture->m_ImportConfiguration, TextureUsageHint::Albedo, TextureUsageHintSource::Heuristic);
    }

    // Create context and try to enable CUDA
    NvttContext* context = nullptr;
    bool hasGpuAcceleration = false;

    if (!(texture->m_ImportConfiguration.UsageHint == TextureUsageHint::Uncompressed || texture->m_ImportConfiguration.UsageHint == TextureUsageHint::DataMap))
    {
        context = nvttCreateContext();

        if (!nvttIsCudaSupported())
        {
            PWarning("CUDA unsupported during compression.");
        }
        else
        {
            nvttSetContextCudaAcceleration(context, NVTT_True);
            hasGpuAcceleration = true;
        }
    }

    auto mipSize = Vector2i(width, height);
    while (mipSize.x > 1 && mipSize.y > 1)
    {
        void* mipImageData = nullptr;

        if (mipSize.x != width)
        {
            mipImageData = stbir_resize_uint8_linear(
                static_cast<unsigned char*>(imageDataPtr),
                width, height,
                channels * width,
                nullptr,
                mipSize.x, mipSize.y,
                channels * mipSize.x,
                STBIR_RGBA);
        }
        else
        {
            mipImageData = imageDataPtr;
        }

        auto compressedImage = CompressImage(texture, context, mipImageData, mipSize.x, mipSize.y, channels, hasGpuAcceleration);
        if (compressedImage.m_TextureData == nullptr)
        {
            free(mipImageData);
            return false;
        }

        texture->m_ImportData.push_back(compressedImage);

        if (!texture->m_ImportConfiguration.GenerateMipmaps || texture->GetImportConfiguration().UsageHint == TextureUsageHint::DataMap)
        {
            break;
        }

        mipSize /= 2;

        PInfo(fmt::format("Generating mip with size {}x{}", mipSize.x, mipSize.y));
    }

    free(imageDataPtr);

    if (context != nullptr)
    {
        nvttDestroyContext(context);
    }

    return true;
}

#endif
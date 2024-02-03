#pragma once
#include "Pine/Core/Math/Math.hpp"
#include <array>

namespace Pine::Graphics
{

    enum class TextureFormat
    {
        SingleChannel,
        RGB,
        RGBA,
        RGB16F,
        RGBA16F,
        Depth,
        DepthStencil,
        Alpha
    };

    enum class TextureType
    {
        Texture2D,
        CubeMap
    };

    enum class TextureUploadTarget
    {
        Default,
        Right,
        Left,
        Top,
        Bottom,
        Front,
        Back
    };

    enum class TextureDataFormat
    {
        UnsignedByte,
        Float,
        UnsignedInt24_8
    };

    enum class TextureFilteringMode
    {
	    Nearest,
        Linear
    };

    enum class SwizzleMaskChannel
    {
        Red,
        Green,
        Blue,
        Alpha,
        Zero,
        One
    };

    inline const char* TextureFormatToString(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::SingleChannel:
            return "SingleChannel";
        case TextureFormat::RGB:
            return "RGB";
        case TextureFormat::RGBA:
            return "RGBA";
        case TextureFormat::RGB16F:
            return "RGB16F";
        case TextureFormat::RGBA16F:
            return "RGBA16F";
        case TextureFormat::Depth:
            return "Depth";
        case TextureFormat::Alpha:
            return "Alpha";
        case TextureFormat::DepthStencil:
            return "Depth Stencil";
        default:
            return "Unknown";
        }
    }

    class ITexture
    {
    protected:
        TextureType m_Type = TextureType::Texture2D;

        TextureFilteringMode m_FilteringMode = TextureFilteringMode::Linear;
        TextureFilteringMode m_MipmapFilteringMode = TextureFilteringMode::Linear;

        int m_Width = 0;
        int m_Height = 0;

        TextureFormat m_TextureFormat = TextureFormat::SingleChannel;
        TextureDataFormat m_TextureDataFormat = TextureDataFormat::UnsignedByte;

        bool m_HasCustomSwizzleMask = false;
        std::array<SwizzleMaskChannel, 4> m_SwizzleMask = { SwizzleMaskChannel::Red, SwizzleMaskChannel::Green, SwizzleMaskChannel::Blue, SwizzleMaskChannel::Alpha };

        bool m_HasMipmaps = false;

        bool m_MultiSampled = false;
        int m_Samples = 8;
    public:
        virtual ~ITexture() = default;

        // The type is up to the graphics API being used
        virtual void* GetGraphicsIdentifier() = 0;

        virtual void Bind(int textureIndex = 0) = 0;
        virtual void Dispose() = 0;

        virtual void SetType(TextureType type) = 0;
        virtual TextureType GetType() = 0;

        virtual void SetFilteringMode(TextureFilteringMode mode) = 0;
        virtual TextureFilteringMode GetFilteringMode() = 0;

        virtual void SetMipmapFilteringMode(TextureFilteringMode mode) = 0;
        virtual TextureFilteringMode GetMipmapFilteringMode() = 0;

        virtual void SetMultiSampled(bool multiSampled) = 0;
        virtual bool IsMultiSampled() = 0;

        virtual void SetSamples(int samples) = 0;
        virtual int GetSamples() = 0;

        virtual int GetWidth() = 0;
        virtual int GetHeight() = 0;

        virtual TextureFormat GetTextureFormat() = 0;
        virtual TextureDataFormat GetTextureDataFormat() = 0;

        virtual bool HasCustomSwizzleMask() = 0;
        virtual void SetSwizzleMask(SwizzleMaskChannel r, SwizzleMaskChannel g, SwizzleMaskChannel b, SwizzleMaskChannel a) = 0;
        virtual void ResetSwizzleMask() = 0;

        virtual void UploadTextureData(int width, int height, TextureFormat textureFormat, TextureDataFormat dataFormat, void* data) = 0;
        virtual void CopyTextureData(ITexture* texture, TextureUploadTarget textureUploadTarget, Vector4i srcRect = Vector4i(-1), Vector2i dstPos = Vector2i(0)) = 0;

        virtual void GenerateMipmaps() = 0;
    };

}
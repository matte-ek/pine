#pragma once
#include "Pine/Core/Math/Math.hpp"

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
        Float
    };

    enum class TextureFilteringMode
    {
	    Nearest,
        Linear
    };

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

        virtual void UploadTextureData(int width, int height, TextureFormat textureFormat, TextureDataFormat dataFormat, void* data) = 0;
        virtual void CopyTextureData(ITexture* texture, TextureUploadTarget textureUploadTarget, Vector4i srcRect = Vector4i(-1), Vector2i dstPos = Vector2i(0)) = 0;

        virtual void GenerateMipmaps() = 0;
    };

}
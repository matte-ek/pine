#pragma once

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

    enum class TextureDataFormat
    {
        UnsignedByte,
        Float
    };

    class ITexture
    {
    protected:
        TextureType m_Type = TextureType::Texture2D;
    public:
        virtual ~ITexture() = default;

        // The type is up to the graphics API being used
        virtual void* GetGraphicsIdentifier() = 0;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual void SetType(TextureType type) = 0;
        virtual TextureType GetType() = 0;

        virtual void UploadTextureData(int width, int height, TextureFormat textureFormat, TextureDataFormat dataFormat, void* data) = 0;
    };

}
#pragma once

namespace Pine::Graphics
{

    enum class TextureFormat
    {
        SingleChannel,
        RGB,
        RGBA
    };

    enum class TextureType
    {
        Texture2D,
        Cubemap
    };

    class ITexture
    {
    protected:
        TextureType m_Type = TextureType::Texture2D;
    public:
        virtual ~ITexture() = default;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual void SetType(TextureType type) = 0;
        virtual TextureType GetType() = 0;

        virtual void UploadTextureData(int width, int height, TextureFormat format, void* data) = 0;
    };

}
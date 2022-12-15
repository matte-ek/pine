#pragma once

namespace Pine::Graphics
{

    enum class TextureFormat
    {
        SingleChannel,
        RGB,
        RGBA
    };

    class ITexture
    {
    private:
    public:
        virtual ~ITexture() = default;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual void UploadTextureData(int width, int height, TextureFormat format, void* data) = 0;
    };

}
#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine::Graphics
{

    class GLTexture : public ITexture
    {
    private:
        std::uint32_t m_Id;
    public:
        GLTexture();

        void Bind() override;
        void Dispose() override;

        void UploadTextureData(int width, int height, TextureFormat format, void* data) override;

    };

}
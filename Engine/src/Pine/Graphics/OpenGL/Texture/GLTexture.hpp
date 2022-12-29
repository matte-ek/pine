#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine::Graphics
{

    class GLTexture : public ITexture
    {
    private:
        std::uint32_t m_Id = 0;
    public:
        GLTexture();

        std::uint32_t GetId() const;

        void Bind() override;
        void Dispose() override;

        TextureType GetType() override;
        void SetType(TextureType type) override;

        void UploadTextureData(int width, int height, TextureFormat format, TextureDataFormat dataFormat, void* data) override;
    };

}
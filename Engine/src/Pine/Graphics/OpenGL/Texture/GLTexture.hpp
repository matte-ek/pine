#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/ITexture.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine::Graphics
{

    class GLTexture : public ITexture
    {
    private:
        std::uint32_t m_Id = 0;

        void UpdateTextureFiltering();
    public:
        GLTexture();

        void* GetGraphicsIdentifier() override;
        std::uint32_t GetId() const;

        void Bind(int textureIndex = 0) override;
        void Dispose() override;

        TextureType GetType() override;
        void SetType(TextureType type) override;

    	void SetFilteringMode(TextureFilteringMode mode) override;
    	TextureFilteringMode GetFilteringMode() override;

        void SetMipmapFilteringMode(TextureFilteringMode mode) override;
        TextureFilteringMode GetMipmapFilteringMode() override;

        int GetWidth() override;
        int GetHeight() override;

        TextureFormat GetTextureFormat() override;
        TextureDataFormat GetTextureDataFormat() override;

        void UploadTextureData(int width, int height, TextureFormat format, TextureDataFormat dataFormat, void* data) override;
        void CopyTextureData(ITexture* texture, TextureUploadTarget textureUploadTarget, Vector4i srcRect = Vector4i(-1), Vector2i dstPos = Vector2i(0)) override;

        void GenerateMipmaps() override;
    };

}
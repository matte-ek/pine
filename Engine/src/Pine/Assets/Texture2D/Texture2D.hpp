#pragma once

#include "Pine/Assets/IGPUAsset/IGPUAsset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine
{

    class Texture2D : public IGPUAsset
    {
    private:
        Graphics::ITexture* m_Texture = nullptr;

        int m_Width = 0;
        int m_Height = 0;

        Graphics::TextureFormat m_Format;
    public:
        int GetWidth() const;
        int GetHeight() const;
        Graphics::TextureFormat GetFormat() const;

        void PrepareGpuData() override;
        void UploadGpuData() override;

        void Dispose() override;
    };

}
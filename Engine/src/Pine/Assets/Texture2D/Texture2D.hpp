#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine
{

    struct TextureLoadPreset
    {
        bool m_GenerateMipmaps = false;
    };

    class Texture2D : public IAsset
    {
    private:
        int m_Width = 0;
        int m_Height = 0;

        bool m_GenerateMipmaps = true;

        Graphics::TextureFormat m_Format = Graphics::TextureFormat::SingleChannel;

        Graphics::ITexture* m_Texture = nullptr;

        void* m_PreparedTextureData = nullptr;

        bool PrepareGpuData();
        void UploadGpuData();
    public:
        Texture2D();

        void SetGenerateMipmaps(bool value);
        bool GetGenerateMipmaps() const;

        int GetWidth() const;
        int GetHeight() const;

        Graphics::TextureFormat GetFormat() const;
        Graphics::ITexture* GetGraphicsTexture() const;

        int GetAudioBufferId() const;

        bool LoadFromFile(AssetLoadStage stage) override;

        void Dispose() override;
    };

}
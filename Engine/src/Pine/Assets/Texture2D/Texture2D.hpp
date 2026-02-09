#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine
{

    class Texture2D : public Asset
    {
    private:
        int m_Width = 0;
        int m_Height = 0;

        bool m_StoreTextureData = false;
        bool m_GenerateMipmaps = true;

        Graphics::TextureFormat m_Format = Graphics::TextureFormat::SingleChannel;

        Graphics::ITexture* m_Texture = nullptr;

        void* m_TextureData = nullptr;

        bool PrepareGpuData();
        void UploadGpuData();
    public:
        Texture2D();

        void SetStoreTextureData(bool storeTextureData);
        bool GetStoreTextureData() const;

        void SetGenerateMipmaps(bool value);
        bool GetGenerateMipmaps() const;

        int GetWidth() const;
        int GetHeight() const;

        void* GetTextureData() const;

        Graphics::TextureFormat GetFormat() const;
        Graphics::ITexture* GetGraphicsTexture() const;

        bool LoadFromFile(AssetLoadStage stage) override;

        void Dispose() override;
    };

}
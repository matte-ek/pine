#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace Pine
{
    enum class TextureCubeSide
    {
        Right,
        Left,
        Top,
        Bottom,
        Front,
        Back
    };

    class Texture3D : public IAsset
    {
    private:
        Graphics::ITexture* m_CubeMapTexture = nullptr;

        AssetHandle<Texture2D> m_Texture;
        std::array<AssetHandle<Texture2D>, 6> m_SideTextures;
    public:
        Texture3D();

        void SetTexture(Texture2D* texture);
        Texture2D* GetTexture() const;

        void SetSideTexture(TextureCubeSide, Texture2D* texture);
        Texture2D* GetSideTexture(TextureCubeSide side) const;

        Graphics::ITexture* GetCubeMap() const;

        void Build();

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };
}
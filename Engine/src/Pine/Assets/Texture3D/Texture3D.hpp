#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
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

    class Texture3D : public Asset
    {
    private:
        Graphics::ITexture* m_CubeMapTexture = nullptr;

        bool m_Valid = false;

        AssetHandle<Texture2D> m_Texture;
        std::array<AssetHandle<Texture2D>, 6> m_SideTextures;
    public:
        Texture3D();

        void SetTexture(Texture2D* texture);
        Texture2D* GetTexture() const;

        void SetSideTexture(TextureCubeSide, Texture2D* texture);
        Texture2D* GetSideTexture(TextureCubeSide side) const;

        bool IsValid() const;

        Graphics::ITexture* GetCubeMap() const;

        bool Build();

        void Dispose() override;
    };
}
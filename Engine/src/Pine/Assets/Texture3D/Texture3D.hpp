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

        std::array<AssetHandle<Texture2D>, 6> m_SideTextures;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct Texture3DSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY_FIXED(SideTextures, UId);
        };
    public:
        Texture3D();

        void SetSideTexture(TextureCubeSide, Texture2D* texture);
        Texture2D* GetSideTexture(TextureCubeSide side) const;

        Graphics::ITexture* GetCubeMap() const;

        bool IsValid() const;

        bool Build();

        void Dispose() override;
    };
}
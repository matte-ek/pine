#pragma once

#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    class TerrainRendererComponent final : public Component
    {
    private:
        AssetHandle<Terrain> m_Terrain;

        struct TerrainSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Terrain);
        };
    public:
        explicit TerrainRendererComponent();

        void SetTerrain(Terrain* terrain);
        Terrain* GetTerrain() const;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}
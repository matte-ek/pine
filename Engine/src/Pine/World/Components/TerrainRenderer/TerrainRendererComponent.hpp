#pragma once

#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    class TerrainRendererComponent final : public Component
    {
    private:
        AssetHandle<Terrain> m_Terrain;
    public:
        explicit TerrainRendererComponent();

        void SetTerrain(Terrain* terrain);
        Terrain* GetTerrain() const;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}
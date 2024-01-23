#pragma once

#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"

namespace Pine
{

    class TilemapRenderer : public IComponent
    {
    private:
        AssetHandle<Tilemap> m_Tilemap;

        int m_Order = 0;
    public:
        explicit TilemapRenderer();

        void SetTilemap(Tilemap* map);
        Tilemap* GetTilemap() const;

        void SetOrder(int order);
        int GetOrder() const;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}
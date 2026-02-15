#pragma once

#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{

    class TilemapRenderer final : public Component
    {
    private:
        AssetHandle<Tilemap> m_Tilemap;

        int m_Order = 0;

        struct TilemapSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Tilemap);
            PINE_SERIALIZE_PRIMITIVE(Order, Serialization::DataType::Int32);
        };
    public:
        explicit TilemapRenderer();

        void SetTilemap(Tilemap* map);
        Tilemap* GetTilemap() const;

        void SetOrder(int order);
        int GetOrder() const;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}
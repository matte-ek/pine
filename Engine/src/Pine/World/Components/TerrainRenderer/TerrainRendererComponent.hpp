#pragma once

#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

#include <optional>

namespace Pine
{

    class TerrainRendererComponent final : public Component
    {
    private:
        AssetHandle<Terrain> m_Terrain;

        // Runtime only, never serialized: where this entity was when the chunks of its terrain were
        // last given light slots. Empty until the renderer has assigned them once.
        //
        // Kept on the component rather than on the asset because it describes a placement, not a
        // height field - and two entities could in principle share one terrain.
        std::optional<Vector3f> m_LightSlotOrigin;

        struct TerrainSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Terrain);
        };
    public:
        explicit TerrainRendererComponent();

        void SetTerrain(Terrain* terrain);
        Terrain* GetTerrain() const;

        const std::optional<Vector3f>& GetLightSlotOrigin() const;
        void SetLightSlotOrigin(const Vector3f& position);

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}
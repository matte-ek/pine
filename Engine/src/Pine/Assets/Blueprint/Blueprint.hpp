#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace Pine
{

    class Blueprint : public IAsset
    {
    private:
        // Stored entity which is the entity the blueprint is describing. This does not
        // point to any existing entity in the world.
        Pine::Entity* m_Entity = nullptr;

        // Copies entity and component data from the source to the destination.
        // If createInstance is set, it will create the source entities components in the world.
        void CopyEntity(Pine::Entity* dst, const Pine::Entity* src, bool createInstance) const;
    public:
        explicit Blueprint();

        // If we currently have an entity stored as a blueprint
        bool HasEntity() const;

        // Create a copy of an existing entity in memory
        void CreateFromEntity(const Pine::Entity* entity);

        // Spawns the stored entity in the world
        Pine::Entity* Spawn();

        // Serializes or de-serializes the stored entity
        void FromJson(const nlohmann::json& j);
        nlohmann::json ToJson() const;

        bool LoadFromFile(AssetLoadStage stage = AssetLoadStage::Default) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}
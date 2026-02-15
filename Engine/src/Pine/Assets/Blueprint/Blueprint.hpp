#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace Pine
{

    class Blueprint : public Asset
    {
    private:
        // Stored entity which is the entity the blueprint is describing. This does not
        // point to any existing entity in the world.
        Entity* m_Entity = nullptr;

        // Copies entity and component data from the source to the destination.
        // If createInstance is set, it will create the source entities components in the world.
        static void CopyEntity(Entity* dst, const Entity* src, bool createInstance);
    public:
        explicit Blueprint();

        // If we currently have an entity stored as a blueprint
        bool HasEntity() const;

        // Gets the stored entity
        Entity* GetEntity() const;

        // Create a copy of an existing entity in memory
        void CreateFromEntity(const Entity* entity);

        // Spawns the stored entity in the world
        Entity* Spawn() const;

        // Serializes or de-serializes the stored entity
        void FromByteSpan(const ByteSpan& byteSpan);
        ByteSpan ToByteSpan() const;

        bool LoadFromFile(AssetLoadStage stage = AssetLoadStage::Default) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}
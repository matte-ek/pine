#pragma once

#include "Pine/World/Entity/Entity.hpp"

namespace Pine::Entities
{
    void Setup();
    void Shutdown();

    Entity* Create();
    Entity* Create(const std::string& name);

    // Restore a previously removed identity. Rejects empty or already-live IDs.
    Entity* CreateWithId(UId id);

    Entity* Find(const std::string& name);
    Entity* Find(UId id);

    Entity* GetByInternalId(std::uint32_t internalId);

    bool Delete(const Entity* entity);
    void DeleteAll(bool includeTemporary = false);

    // Changes whenever DeleteAll resets the scene, even when the scene was already empty.
    std::uint64_t GetSceneGeneration();

    const std::vector<Entity*>& GetList();

    // How many more entities fit. Creating one past that throws, so a caller that cannot let that
    // exception escape checks this first.
    std::uint32_t GetFreeSlotCount();

    // Allows you to move the specified entity.
    // newIndex specifying the element index in the vector itself.
    void MoveEntity(const Entity* entity, std::size_t newIndex);
}

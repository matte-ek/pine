#pragma once

#include "Pine/World/Entity/Entity.hpp"

namespace Pine::Entities
{

    void Setup();
    void Shutdown();

    Entity* Create();
    Entity* Create(const std::string& name);

    Entity* Find(const std::string& name);
    Entity* Find(std::uint32_t id);

    Entity* GetByInternalId(int internalId);

    bool Delete(Entity* entity);
    void DeleteAll(bool includeTemporary = false);

    const std::vector<Entity*>& GetList();

    // Allows you to move the specified entity.
    // newIndex specifying the element index in the vector itself.
    void MoveEntity(Entity* entity, std::size_t newIndex);

}
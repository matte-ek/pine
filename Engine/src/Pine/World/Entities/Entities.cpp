#include "Entities.hpp"
#include "Pine/Engine/Engine.hpp"

using namespace Pine;

namespace
{
    // Incremental counter each time an entity is created, to make sure
    // each entity will have a unique id.
    std::uint32_t m_EntityId = 0;

    // The current set engine configuration for the maximum amount of entities in the scene.
    // Currently, stays constant during the lifespan of the application.
    std::uint32_t m_MaxEntityCount = 0;

    // The array where all the entity data is actually stored, size is m_MaxEntityCount
    Entity* m_Entities;

    // Array which holds if an element index in m_Entities is occupied, also size of m_MaxEntityCount
    bool* m_EntityOccupationArray;

    // Get the current highest element index of an entity
    std::uint32_t GetHighestEntityIndex()
    {
        std::uint32_t highestIndex = 0;

        for (std::uint32_t i = 0; i < m_MaxEntityCount;i++)
        {
            if (m_EntityOccupationArray[i])
                highestIndex = i + 1;
        }

        return highestIndex;
    }

    // Gets the first available element index in m_Entities
    std::uint32_t GetAvailableEntityIndex()
    {
        for (std::uint32_t i = 0; i < m_MaxEntityCount;i++)
        {
            if (!m_EntityOccupationArray[i])
                return i;
        }

        return m_MaxEntityCount;
    }

    // The outwards facing entity list vector, with pointers to m_Entities. This allows us to move
    // pointers around in this list, without having to move entity data around.
    std::vector<Entity*> m_EntityPointerList;

    // See https://stackoverflow.com/a/57399634
    template <typename t> void MoveElementInVector(std::vector<t>& v, size_t oldIndex, size_t newIndex)
    {
        if (oldIndex > newIndex)
            std::rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
        else
            std::rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
    }

}

void Pine::Entities::Setup()
{
    m_MaxEntityCount = Engine::GetEngineConfiguration().m_MaxObjectCount;

    m_Entities = static_cast<Entity*>(malloc(sizeof(Entity) * m_MaxEntityCount));
    m_EntityOccupationArray = new bool[m_MaxEntityCount];

    memset(static_cast<void*>(m_Entities), 0, sizeof(Entity) * m_MaxEntityCount);
    memset(static_cast<void*>(m_EntityOccupationArray), 0, sizeof(bool) * m_MaxEntityCount);

    m_EntityPointerList.reserve(m_MaxEntityCount);
}

void Pine::Entities::Shutdown()
{
    free(m_Entities);
    delete[] m_EntityOccupationArray;

    m_EntityPointerList.clear();
}

Pine::Entity* Pine::Entities::Create()
{
    const auto availableEntityIndex = GetAvailableEntityIndex();

    if (availableEntityIndex == m_MaxEntityCount)
    {
        throw std::runtime_error("Maximum entity count reached.");
    }

    const auto entityPtr = &m_Entities[availableEntityIndex];

    // Call constructor on the entity
    new(entityPtr) Entity(m_EntityId++);

    // Mark the slot as occupied
    m_EntityOccupationArray[availableEntityIndex] = true;

    m_EntityPointerList.push_back(entityPtr);

    return entityPtr;
}

Pine::Entity* Pine::Entities::Create(const std::string& name)
{
    auto entity = Create();

    entity->SetName(name);

    return entity;
}

Entity* Entities::Find(const std::string& name)
{
    for (auto entity : m_EntityPointerList)
    {
        if (entity->GetName() == name)
            return entity;
    }

    return nullptr;
}

Entity* Entities::Find(std::uint32_t id)
{
    for (auto entity : m_EntityPointerList)
    {
        if (entity->GetId() == id)
            return entity;
    }

    return nullptr;
}

bool Entities::Delete(Entity* entity)
{
    bool foundEntity = false;

    // First remove the entity from the pointer list
    for (int i = 0; i < m_EntityPointerList.size();i++)
    {
        if (m_EntityPointerList[i] == entity)
        {
            m_EntityPointerList.erase(m_EntityPointerList.begin() + i);

            foundEntity = true;

            break;
        }
    }

    if (!foundEntity)
    {
        return false;
    }

    for (int i = 0; i < GetHighestEntityIndex();i++)
    {
        if (!m_EntityOccupationArray[i])
            continue;

        if (m_Entities[i].GetId() == entity->GetId())
        {
            m_Entities[i].~Entity();
            m_EntityOccupationArray[i] = false;

            return true;
        }
    }

    // If we've reached to this point, something has gone terribly wrong.

    throw std::runtime_error("Failed to find entity pointer while removing entity.");
}

void Entities::DeleteAll(bool includeTemporary)
{
    if (includeTemporary)
    {
        for (int i = 0; i < GetHighestEntityIndex();i++)
        {
            if (!m_EntityOccupationArray[i])
                continue;

            m_Entities[i].~Entity();
            m_EntityOccupationArray[i] = false;
        }

        m_EntityPointerList.clear();

        return;
    }

    std::vector<const Entity*> entityPointerKeepList;

    for (int i = 0; i < GetHighestEntityIndex();i++)
    {
        if (!m_EntityOccupationArray[i])
            continue;

        const auto& entity = m_Entities[i];

        if (entity.GetTemporary())
        {
            entityPointerKeepList.push_back(&entity);

            continue;
        }

        m_Entities[i].~Entity();
        m_EntityOccupationArray[i] = false;
    }

    for (int i = 0; i < m_EntityPointerList.size();i++)
    {
        bool ignoreEntity = false;

        for (auto keepEntity : entityPointerKeepList)
        {
            if (m_EntityPointerList[i] == keepEntity)
            {
                ignoreEntity = true;
                break;
            }
        }

        if (ignoreEntity)
        {
            continue;
        }

        m_EntityPointerList.erase(m_EntityPointerList.begin() + i);
    }
}

const std::vector<Entity*>& Entities::GetList()
{
    return m_EntityPointerList;
}

void Entities::MoveEntity(Entity* entity, size_t newIndex)
{
    bool foundEntity = false;
    size_t oldIndex;

    for (size_t i = 0; i < m_EntityPointerList.size();i++)
    {
        if (m_EntityPointerList[i]->GetId() == entity->GetId())
        {
            foundEntity = true;
            oldIndex = i;
            break;
        }
    }

    if (!foundEntity)
    {
        throw std::runtime_error("MoveEntity() called on invalid entity pointer.");
    }

    MoveElementInVector(m_EntityPointerList, oldIndex, newIndex);
}
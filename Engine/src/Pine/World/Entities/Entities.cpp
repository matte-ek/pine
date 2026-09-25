#include "Entities.hpp"
#include "Pine/Engine/Engine.hpp"

#include <algorithm>

using namespace Pine;

namespace
{
    // Incremental counter each time an entity is created, to make sure
    // each entity will have a unique id.
    std::uint32_t m_EntityId = 1;
    std::uint64_t m_SceneGeneration = 0;

    // The current set engine configuration for the maximum number of entities in the scene.
    // Currently, stays constant during the lifespan of the application.
    std::uint32_t m_MaxEntityCount = 0;

    // The array where all the entity data is actually stored, size is m_MaxEntityCount
    Entity* m_Entities;

    // Array which holds if an element index in m_Entities is occupied, also the size of m_MaxEntityCount
    bool* m_EntityOccupationArray;

    // One past the highest occupied slot in m_Entities, or zero when there are no entities.
    std::uint32_t m_HighestEntityIndex = 0;

    // The lowest free slot in m_Entities, or m_MaxEntityCount when every slot is taken.
    std::uint32_t m_FirstFreeEntityIndex = 0;

    // Marking slots goes through these two so the indices above follow along without rescanning
    // the whole array, which would make every create and delete cost the capacity.
    void MarkEntitySlotOccupied(const std::uint32_t index)
    {
        m_EntityOccupationArray[index] = true;

        m_HighestEntityIndex = std::max(m_HighestEntityIndex, index + 1);

        while (m_FirstFreeEntityIndex < m_MaxEntityCount && m_EntityOccupationArray[m_FirstFreeEntityIndex])
        {
            m_FirstFreeEntityIndex++;
        }
    }

    void MarkEntitySlotFree(const std::uint32_t index)
    {
        m_EntityOccupationArray[index] = false;

        m_FirstFreeEntityIndex = std::min(m_FirstFreeEntityIndex, index);

        // Only freeing the top slot moves the highest index, down to the next occupied slot.
        if (index + 1 != m_HighestEntityIndex)
        {
            return;
        }

        while (m_HighestEntityIndex > 0 && !m_EntityOccupationArray[m_HighestEntityIndex - 1])
        {
            m_HighestEntityIndex--;
        }
    }

    // The outwards facing entity list vector, with pointers to m_Entities. This allows us to move
    // pointers around in this list, without having to move entity data around.
    std::vector<Entity*> m_EntityPointerList;

    // See https://stackoverflow.com/a/57399634
    template <typename t> void MoveElementInVector(std::vector<t>& v, std::size_t oldIndex, std::size_t newIndex)
    {
        if (oldIndex > newIndex)
            std::rotate(v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex);
        else
            std::rotate(v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1);
    }

    // Places a new entity in the first free slot. The caller is responsible for the id being unused.
    Entity* PlaceEntity(const UId id)
    {
        const auto availableEntityIndex = m_FirstFreeEntityIndex;

        if (availableEntityIndex == m_MaxEntityCount)
        {
            throw std::runtime_error("Maximum entity count reached.");
        }

        const auto entityPtr = &m_Entities[availableEntityIndex];

        // Call constructor on the entity
        new(entityPtr) Entity(id, availableEntityIndex);

        MarkEntitySlotOccupied(availableEntityIndex);

        m_EntityPointerList.push_back(entityPtr);

        return entityPtr;
    }

}

void Entities::Setup()
{
    m_MaxEntityCount = Engine::GetEngineConfiguration().m_MaxObjectCount;

    m_Entities = static_cast<Entity*>(malloc((sizeof(Entity) * m_MaxEntityCount) + 1));
    m_EntityOccupationArray = new bool[m_MaxEntityCount];

    if (m_Entities == nullptr || m_MaxEntityCount == 0)
    {
        throw std::runtime_error("Failed to allocate entity data.");
    }

    memset(m_Entities, 0, sizeof(Entity) * m_MaxEntityCount);
    memset(m_EntityOccupationArray, 0, sizeof(bool) * m_MaxEntityCount);

    m_EntityPointerList.reserve(m_MaxEntityCount);
}

void Entities::Shutdown()
{
    free(m_Entities);
    delete[] m_EntityOccupationArray;

    m_EntityPointerList.clear();
}

Entity* Entities::Create()
{
    // A fresh UId is a nanosecond timestamp plus 64 random bits, so it does not need the uniqueness
    // scan CreateWithId() runs - which would make creating n entities cost O(n^2).
    return PlaceEntity(UId::New());
}

Entity* Entities::CreateWithId(const UId id)
{
    if (!id.IsValid() || Find(id) != nullptr)
    {
        throw std::runtime_error("Entity restoration requires a valid, unused ID.");
    }

    return PlaceEntity(id);
}

Entity* Entities::Create(const std::string& name)
{
    auto entity = Create();

    entity->SetName(name);

    return entity;
}

Entity* Entities::Find(const std::string& name)
{
    for (const auto entity : m_EntityPointerList)
    {
        if (entity->GetName() == name)
            return entity;
    }

    return nullptr;
}

Entity* Entities::Find(const UId id)
{
    for (const auto entity : m_EntityPointerList)
    {
        if (entity->GetId() == id)
            return entity;
    }

    return nullptr;
}

bool Entities::Delete(const Entity* entity)
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

    // Every entity in the pointer list lives in m_Entities, at the slot its internal id names.
    const auto slot = entity->GetInternalId();

    if (slot >= m_MaxEntityCount || &m_Entities[slot] != entity)
    {
        // If we've reached this point, something has gone terribly wrong.
        throw std::runtime_error("Failed to find entity pointer while removing entity.");
    }

    m_Entities[slot].~Entity();
    MarkEntitySlotFree(slot);

    return true;
}

void Entities::DeleteAll(const bool includeTemporary)
{
    ++m_SceneGeneration;

    const auto highestEntityIndex = m_HighestEntityIndex;

    if (includeTemporary)
    {
        for (std::uint32_t i = 0; i < highestEntityIndex;i++)
        {
            if (!m_EntityOccupationArray[i])
                continue;

            m_Entities[i].~Entity();
            MarkEntitySlotFree(i);
        }

        m_EntityPointerList.clear();

        return;
    }

    std::vector<Entity*> entitiesToRestore;

    for (std::uint32_t i = 0; i < highestEntityIndex;i++)
    {
        if (!m_EntityOccupationArray[i])
            continue;

        if (m_Entities[i].GetTemporary())
        {
            entitiesToRestore.push_back(&m_Entities[i]);
        }
        else
        {
            m_Entities[i].~Entity();
            MarkEntitySlotFree(i);
        }
    }

    m_EntityPointerList.clear();

    for (auto entity : entitiesToRestore)
    {
        m_EntityPointerList.push_back(entity);
    }
}

const std::vector<Entity*>& Entities::GetList()
{
    return m_EntityPointerList;
}

std::uint32_t Entities::GetFreeSlotCount()
{
    return m_MaxEntityCount - static_cast<std::uint32_t>(m_EntityPointerList.size());
}

std::uint64_t Entities::GetSceneGeneration()
{
    return m_SceneGeneration;
}

void Entities::MoveEntity(const Entity* entity, const std::size_t newIndex)
{
    bool foundEntity = false;
    std::size_t oldIndex = 0;

    for (std::size_t i = 0; i < m_EntityPointerList.size();i++)
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

Entity *Entities::GetByInternalId(const std::uint32_t internalId)
{
    assert(internalId < m_MaxEntityCount);

    if (!m_EntityOccupationArray[internalId])
    {
        return nullptr;
    }

    return &m_Entities[internalId];
}

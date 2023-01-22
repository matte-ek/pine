#include "Entities.hpp"
#include "Pine/Engine/Engine.hpp"

using namespace Pine;

namespace
{
    // Incremental counter each time an entity is created, to make sure
    // each entity will have a unique id.
    std::uint32_t m_EntityId = 0;

    // The vector where all the entity data is actually stored.
    std::vector<Entity> m_Entities;

    // The outwards facing entity list vector, with pointers to m_Entities. This allows us to move
    // pointers around in this list, without having to move entity data around.
    std::vector<Entity*> m_EntityPointerList;

}

void Pine::Entities::Setup()
{
    m_Entities.reserve(Engine::GetEngineConfiguration().m_MaxObjectCount);
    m_EntityPointerList.reserve(Engine::GetEngineConfiguration().m_MaxObjectCount);
}

void Pine::Entities::Shutdown()
{
    m_Entities.clear();
    m_EntityPointerList.clear();
}

Pine::Entity* Pine::Entities::Create()
{
    m_Entities.emplace_back(m_EntityId++);

    auto entityPtr = &m_Entities[m_Entities.size() - 1];

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
    for (auto& entity : m_Entities)
    {
        if (entity.GetName() == name)
            return &entity;
    }

    return nullptr;
}

Entity* Entities::Find(std::uint32_t id)
{
    for (auto& entity : m_Entities)
    {
        if (entity.GetId() == id)
            return &entity;
    }

    return nullptr;
}

bool Entities::Delete(Entity* entity)
{
    bool foundEntity = false;

    // First remove the entity data
    for (int i = 0; i < m_Entities.size();i++)
    {
        if (&m_Entities[i] == entity)
        {
            m_Entities.erase(m_Entities.begin() + i);

            foundEntity = true;

            break;
        }
    }

    if (!foundEntity)
    {
        return false;
    }

    for (int i = 0; i < m_EntityPointerList.size();i++)
    {
        if (m_EntityPointerList[i] == entity)
        {
            m_EntityPointerList.erase(m_EntityPointerList.begin() + i);

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
        m_Entities.clear();
        m_EntityPointerList.clear();
        return;
    }

    std::vector<const Entity*> entityPointerKeepList;

    for (int i = 0; i < m_Entities.size();i++)
    {
        const auto& entity = m_Entities[i];

        if (entity.GetTemporary())
        {
            entityPointerKeepList.push_back(&entity);

            continue;
        }

        m_Entities.erase(m_Entities.begin() + i);
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
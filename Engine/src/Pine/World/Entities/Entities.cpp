#include "Entities.hpp"
#include "Pine/Engine/Engine.hpp"

using namespace Pine;

namespace
{
    // Incremental counter each time an entity is created, to make sure
    // each entity will have a unique id.
    std::uint32_t m_EntityId = 0;

    std::vector<Entity> m_Entities;
}

void Pine::Entities::Setup()
{
    m_Entities.reserve(Engine::GetEngineConfiguration().m_MaxObjectCount);
}

void Pine::Entities::Shutdown()
{
    m_Entities.clear();
}

Pine::Entity* Pine::Entities::Create()
{
    m_Entities.emplace_back(m_EntityId++);

    return &m_Entities[m_Entities.size() - 1];
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
    for (int i = 0; i < m_Entities.size();i++)
    {
        if (&m_Entities[i] == entity)
        {
            m_Entities.erase(m_Entities.begin() + i);

            return true;
        }
    }

    return false;
}

void Entities::DeleteAll(bool includeTemporary)
{
    if (includeTemporary)
    {
        m_Entities.clear();
        return;
    }

    for (int i = 0; i < m_Entities.size();i++)
    {
    }
}

const std::vector<Entity>& Entities::GetList()
{
    return m_Entities;
}
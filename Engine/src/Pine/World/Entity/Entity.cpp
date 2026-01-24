#include "Entity.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Entities/Entities.hpp"

Pine::Entity::Entity(std::uint32_t id)
    : m_Id(id)
{
}

Pine::Entity::Entity(std::uint32_t id, std::uint32_t internalId)
        : m_Id(id), m_InternalId(internalId)
{
    CreateScriptHandle();

    AddComponent<Transform>();
}

Pine::Entity::~Entity()
{
    for (auto& component : m_Components)
    {
        if (!Components::Destroy(component))
        {
            Log::Error("~Entity(): Error destroying component.");
        }

        component = nullptr;
    }

    m_Components.clear();

    // If the id is zero, this entity is not part of the world, therefore we'll have to do things
    // a bit more manually.
    if (m_Id == 0)
    {
        auto children = m_Children;

        for (auto child : children)
        {
            delete child;
        }

        m_Children.clear();
    }
    else
    {
        while (!m_Children.empty())
        {
            Entities::Delete(m_Children.front());
        }
    }

    if (m_Parent != nullptr)
    {
        m_Parent->RemoveChild(this);
    }

    DestroyScriptHandle();
}

std::uint32_t Pine::Entity::GetId() const
{
    return m_Id;
}

std::uint32_t Pine::Entity::GetInternalId() const
{
    return m_InternalId;
}

void Pine::Entity::SetActive(bool value)
{
    m_Active = value;
}

bool Pine::Entity::GetActive() const
{
    return m_Active;
}

void Pine::Entity::SetStatic(bool value)
{
    m_Static = value;
}

bool Pine::Entity::GetStatic() const
{
    return m_Static;
}

void Pine::Entity::SetTags(std::uint64_t tags)
{
    m_Tags = tags;
}

std::uint64_t Pine::Entity::GetTags() const
{
    return m_Tags;
}

void Pine::Entity::SetDirty(bool value)
{
    m_Dirty = value;
}

bool Pine::Entity::IsDirty() const
{
    return m_Dirty;
}

void Pine::Entity::SetTemporary(bool value)
{
    m_Temporary = value;
}

bool Pine::Entity::GetTemporary() const
{
    return m_Temporary;
}

void Pine::Entity::SetName(const std::string& name)
{
    m_Name = name;
}

const std::string& Pine::Entity::GetName() const
{
    return m_Name;
}

void Pine::Entity::SetParent(Entity* entity)
{
    m_Parent = entity;
}

Pine::Entity* Pine::Entity::GetParent() const
{
    return m_Parent;
}

/*
void Pine::Entity::SetBlueprint(Blueprint *blueprint)
{
    m_AssetBlueprint = blueprint;
}

Pine::Blueprint * Pine::Entity::GetBlueprint() const
{
    return m_AssetBlueprint.Get();
}
*/

Pine::IComponent* Pine::Entity::AddComponent(ComponentType type)
{
    const auto component = Components::Create(type);

    component->SetParent(this);
    component->OnCreated();

    m_Components.push_back(component);

    return component;
}

Pine::IComponent* Pine::Entity::AddComponent(IComponent* component)
{
    component->SetParent(this);
    component->OnCreated();

    m_Components.push_back(component);

    return component;
}

bool Pine::Entity::RemoveComponent(const IComponent* targetComponent)
{
    for (int i = 0; i < m_Components.size();i++)
    {
        const auto component = m_Components[i];

        if (component == targetComponent)
        {
            m_Components.erase(m_Components.begin() + i);

            if (Components::Destroy(component))
                return true;
        }
    }

    return false;
}

void Pine::Entity::ClearComponents()
{
    for (const auto component : m_Components)
    {
        Components::Destroy(component);
    }

    m_Components.clear();
}

Pine::IComponent * Pine::Entity::GetComponent(ComponentType type) const
{
    for (auto component : m_Components)
    {
        if (component && component->GetType() == type)
        {
            return component;
        }
    }

    return nullptr;
}

bool Pine::Entity::HasComponent(ComponentType type) const
{
    return GetComponent(type) != nullptr;
}

Pine::Transform* Pine::Entity::GetTransform() const
{
    if (m_Components.empty())
    {
        throw std::runtime_error("Entity does not contain Transform component");
    }

    // The transform component should always be the first component
    // and should be available in all entities.
    return dynamic_cast<Transform*>(m_Components[0]);
}

const std::vector<Pine::IComponent*>& Pine::Entity::GetComponents() const
{
    return m_Components;
}

Pine::Entity* Pine::Entity::CreateChild()
{
    const auto entity = Entities::Create();

    AddChild(entity);

    return entity;
}

void Pine::Entity::AddChild(Entity* entity)
{
    entity->SetParent(this);

    m_Children.push_back(entity);
}

void Pine::Entity::RemoveChild(Entity* entity)
{
    entity->SetParent(nullptr);

    for (int i = 0; i < m_Children.size();i++)
    {
        if (m_Children[i] == entity)
        {
            m_Children.erase(m_Children.begin() + i);
            break;
        }
    }
}

const std::vector<Pine::Entity*>& Pine::Entity::GetChildren() const
{
    return m_Children;
}

void Pine::Entity::Delete()
{
    Entities::Delete(this);
}

Pine::Entity* Pine::Entity::Create()
{
    return Entities::Create();
}

Pine::Entity* Pine::Entity::Create(const std::string& name)
{
    return Entities::Create(name);
}

Pine::Script::ObjectHandle *Pine::Entity::GetScriptHandle()
{
    return &m_EntityScriptHandle;
}

void Pine::Entity::CreateScriptHandle()
{
    m_EntityScriptHandle = Script::ObjectFactory::CreateEntity(m_Id, m_InternalId);
}

void Pine::Entity::DestroyScriptHandle()
{
    if (m_EntityScriptHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeEntity(&m_EntityScriptHandle);
}

#include "Entity.hpp"
#include "Pine/World/Entities/Entities.hpp"

Pine::Entity::Entity(std::uint32_t id)
    : m_Id(id)
{
    AddComponent<Transform>();
}

Pine::Entity::~Entity()
{
    for (auto component : m_Components)
    {
        Components::Destroy(component);
    }

    for (auto child : m_Children)
    {
        Entities::Delete(child);
    }

    if (m_Parent != nullptr)
    {
        m_Parent->RemoveChild(this);
    }
}

std::uint32_t Pine::Entity::GetId() const
{
    return m_Id;
}

void Pine::Entity::SetActive(bool value)
{
    m_Active = value;
}

bool Pine::Entity::GetActive() const
{
    return m_Active;
}

void Pine::Entity::SetName(const std::string& name)
{
    m_Name = name;
}

const std::string& Pine::Entity::GetName() const
{
    return m_Name;
}

void Pine::Entity::SetParent(Pine::Entity* entity)
{
    m_Parent = entity;
}

Pine::Entity* Pine::Entity::GetParent() const
{
    return m_Parent;
}

Pine::IComponent* Pine::Entity::AddComponent(Pine::ComponentType type)
{
    auto component = Components::Create(type);

    component->SetParent(this);

    m_Components.push_back(component);

    return component;
}

Pine::IComponent* Pine::Entity::AddComponent(Pine::IComponent* component)
{
    component->SetParent(this);

    m_Components.push_back(component);

    return component;
}

bool Pine::Entity::RemoveComponent(Pine::IComponent* targetComponent)
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

Pine::Transform* Pine::Entity::GetTransform() const
{
    if (m_Components.empty())
        throw std::runtime_error("Entity does not contain Transform component");

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
    auto entity = Entities::Create();

    AddChild(entity);

    return entity;
}

void Pine::Entity::AddChild(Pine::Entity* entity)
{
    entity->SetParent(this);

    m_Children.push_back(entity);
}

void Pine::Entity::RemoveChild(Pine::Entity* entity)
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
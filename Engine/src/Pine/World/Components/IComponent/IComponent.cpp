#include "IComponent.hpp"
#include "Pine/World/Entity/Entity.hpp"

Pine::IComponent::IComponent(ComponentType type)
    : m_Type(type)
{
}

void Pine::IComponent::SetActive(bool value)
{
    m_Active = value;
}

bool Pine::IComponent::GetActive() const
{
    return m_Active;
}

Pine::ComponentType Pine::IComponent::GetType() const
{
    return m_Type;
}

void Pine::IComponent::SetStandalone(bool value)
{
    m_Standalone = value;
}

bool Pine::IComponent::GetStandalone() const
{
    return m_Standalone;
}

void Pine::IComponent::SetParent(Entity* entity)
{
    m_Parent = entity;
}

Pine::Entity* Pine::IComponent::GetParent() const
{
    return m_Parent;
}

void Pine::IComponent::OnCreated()
{
}

void Pine::IComponent::OnDestroyed()
{
}

void Pine::IComponent::OnCopied()
{
}

void Pine::IComponent::OnSetup()
{
}

void Pine::IComponent::OnUpdate(float deltaTime)
{
}

void Pine::IComponent::OnRender(float deltaTime)
{
}

void Pine::IComponent::OnPrePhysicsUpdate()
{
}

void Pine::IComponent::OnPostPhysicsUpdate()
{
}

void Pine::IComponent::LoadData(const nlohmann::json& j)
{
}

void Pine::IComponent::SaveData(nlohmann::json& j)
{
}

bool Pine::IComponent::IsWorldEnabled() const
{
    return m_Active && m_Parent->GetActive();
}

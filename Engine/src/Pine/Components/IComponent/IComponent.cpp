#include "IComponent.hpp"

Pine::IComponent::IComponent(Pine::Entity* parent, Pine::ComponentType type)
    : m_Parent(parent),
      m_Type(type)
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

void Pine::IComponent::LoadData(const nlohmann::json& j)
{
}

void Pine::IComponent::SaveData(nlohmann::json& j)
{
}

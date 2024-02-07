#include "IComponent.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
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
    if (m_Standalone)
        return;

    CreateScriptInstance();
}

void Pine::IComponent::OnDestroyed()
{
    if (m_Standalone)
        return;

    DestroyScriptInstance();
}

void Pine::IComponent::OnCopied()
{
    m_ScriptObjectHandle = { nullptr, 0 };
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

Pine::Script::ObjectHandle *Pine::IComponent::GetComponentScriptHandle()
{
    return &m_ScriptObjectHandle;
}

void Pine::IComponent::CreateScriptInstance()
{
    m_ScriptObjectHandle = Script::ObjectFactory::CreateComponent(this);
}

void Pine::IComponent::DestroyScriptInstance()
{
    if (m_ScriptObjectHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeObject(&m_ScriptObjectHandle);
}

void Pine::IComponent::SetInternalId(std::uint32_t id)
{
    m_InternalId = id;
}

std::uint32_t Pine::IComponent::GetInternalId() const
{
    return m_InternalId;
}

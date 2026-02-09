#include "Component.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Entity/Entity.hpp"

Pine::Component::Component(ComponentType type)
    : m_Type(type)
{
}

void Pine::Component::SetUniqueId(std::uint64_t id)
{
    m_UniqueId = id;
}

std::uint64_t Pine::Component::GetUniqueId() const
{
    return m_UniqueId;
}

void Pine::Component::SetActive(bool value)
{
    m_Active = value;
}

bool Pine::Component::GetActive() const
{
    return m_Active;
}

Pine::ComponentType Pine::Component::GetType() const
{
    return m_Type;
}

void Pine::Component::SetStandalone(bool value)
{
    m_Standalone = value;
}

bool Pine::Component::GetStandalone() const
{
    return m_Standalone;
}

void Pine::Component::SetParent(Entity* entity)
{
    m_Parent = entity;
}

Pine::Entity* Pine::Component::GetParent() const
{
    return m_Parent;
}

void Pine::Component::OnCreated()
{
    if (m_Standalone)
        return;

    CreateScriptInstance();
}

void Pine::Component::OnDestroyed()
{
    if (m_Standalone)
        return;

    DestroyScriptInstance();
}

void Pine::Component::OnCopied()
{
    m_ScriptObjectHandle = { nullptr, 0 };
}

void Pine::Component::OnSetup()
{
}

void Pine::Component::OnUpdate(float deltaTime)
{
}

void Pine::Component::OnRender(float deltaTime)
{
}

void Pine::Component::OnPrePhysicsUpdate()
{
}

void Pine::Component::OnPostPhysicsUpdate()
{
}

void Pine::Component::LoadData(const nlohmann::json& j)
{
}

void Pine::Component::SaveData(nlohmann::json& j)
{
}

bool Pine::Component::IsWorldEnabled() const
{
    return m_Active && m_Parent->GetActive();
}

Pine::Script::ObjectHandle *Pine::Component::GetComponentScriptHandle()
{
    return &m_ScriptObjectHandle;
}

void Pine::Component::CreateScriptInstance()
{
    m_ScriptObjectHandle = Script::ObjectFactory::CreateComponent(this);
}

void Pine::Component::DestroyScriptInstance()
{
    if (m_ScriptObjectHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeComponent(this, &m_ScriptObjectHandle);
}

void Pine::Component::SetInternalId(std::uint32_t id)
{
    m_InternalId = id;
}

std::uint32_t Pine::Component::GetInternalId() const
{
    return m_InternalId;
}

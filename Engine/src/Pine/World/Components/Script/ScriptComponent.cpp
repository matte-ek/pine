#include "ScriptComponent.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::ScriptComponent::ScriptComponent()
    : IComponent(ComponentType::Script)
{
}

void Pine::ScriptComponent::SetScript(Pine::CSharpScript *script)
{
    bool createInstance = false;

    if (!m_Standalone)
    {
        if (script)
        {
            script->RegisterScriptComponent(this);
            createInstance = true;
        }
        else if (m_Script.Get())
        {
            m_Script->UnregisterScriptComponent(this);
            DestroyInstance();
        }
    }

    m_Script = dynamic_cast<IAsset*>(script);

    if (createInstance)
    {
        CreateInstance();
    }
}

Pine::CSharpScript *Pine::ScriptComponent::GetScript() const
{
    return m_Script.Get();
}

void Pine::ScriptComponent::OnCreated()
{
    IComponent::OnCreated();

    CreateInstance();
}

void Pine::ScriptComponent::OnCopied()
{
    IComponent::OnCopied();

    m_ScriptObjectHandle.Object = nullptr;
    m_ScriptObjectHandle.Handle = 0;
}

void Pine::ScriptComponent::OnDestroyed()
{
    IComponent::OnDestroyed();

    DestroyInstance();
}

void Pine::ScriptComponent::CreateInstance()
{
    if (m_Standalone)
    {
        return;
    }

    if (m_ScriptObjectHandle.Object != nullptr)
    {
        return;
    }

    if (m_Script.Get() == nullptr)
    {
        return;
    }

    m_ScriptObjectHandle = Script::ObjectFactory::CreateScriptObject(m_Script.Get(), this);
}

void Pine::ScriptComponent::DestroyInstance()
{
    if (m_ScriptObjectHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeObject(&m_ScriptObjectHandle);
}

Pine::Script::ObjectHandle *Pine::ScriptComponent::GetScriptObjectHandle()
{
    return &m_ScriptObjectHandle;
}

void Pine::ScriptComponent::LoadData(const nlohmann::json &j)
{
    Serialization::LoadAsset(j, "script", m_Script);
}

void Pine::ScriptComponent::SaveData(nlohmann::json &j)
{
    j["script"] = Serialization::StoreAsset(m_Script);
}

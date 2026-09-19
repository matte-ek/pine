#include "ScriptComponent.hpp"

#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"

#include <mono/metadata/object.h>

namespace
{
    // Stored values are matched to fields by name, so that renaming a field loses only that field
    // rather than shifting every value after it onto the wrong one.
    Pine::ScriptFieldValue* FindValue(std::vector<Pine::ScriptFieldValue>& values, const std::string& name)
    {
        for (auto& value : values)
        {
            if (value.Name == name)
            {
                return &value;
            }
        }

        return nullptr;
    }
}

Pine::ScriptComponent::ScriptComponent()
    : Component(ComponentType::Script)
{
}

void Pine::ScriptComponent::SetScript(CSharpScript *script)
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

    m_Script = script;

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
    Component::OnCreated();

    CreateInstance();
}

void Pine::ScriptComponent::OnCopied()
{
    Component::OnCopied();

    m_ScriptObjectHandle.Object = nullptr;
    m_ScriptObjectHandle.Handle = 0;
}

void Pine::ScriptComponent::OnDestroyed()
{
    Component::OnDestroyed();

    DestroyInstance();

    // The ECS retires a component by marking its slot free and memcpy'ing a prototype over it when
    // it is next used - no destructor ever runs - so the field values have to be released here or
    // their storage is leaked for the lifetime of the process. swap() rather than clear(), because
    // only swapping with an empty vector is guaranteed to give the capacity back.
    std::vector<ScriptFieldValue>().swap(m_FieldValues);
}

void Pine::ScriptComponent::LoadData(const ByteSpan& span)
{
    ScriptSerializer scriptSerializer;

    scriptSerializer.Read(span);

    scriptSerializer.Script.Read(m_Script);

    m_FieldValues.clear();

    for (std::size_t i = 0; i < scriptSerializer.Fields.GetDataCount(); i++)
    {
        ScriptFieldSerializer fieldSerializer;

        fieldSerializer.Read(scriptSerializer.Fields.GetData(i));

        ScriptFieldValue value;

        fieldSerializer.Name.Read(value.Name);

        std::int32_t type = 0;
        fieldSerializer.Type.Read(type);
        value.Type = static_cast<ScriptFieldType>(type);

        // An empty payload is a real value - an empty string, or a reference to nothing - so a
        // failed read is left as the empty Data the value already carries.
        ByteSpan data;
        if (fieldSerializer.Value.Read(data))
        {
            value.Data.assign(data.data, data.data + data.size);
        }

        m_FieldValues.push_back(std::move(value));
    }
}

Pine::ByteSpan Pine::ScriptComponent::SaveData()
{
    // Saving does not destroy the managed object, so nothing else would have taken its current
    // values - including whatever the properties panel just typed into it.
    CaptureFieldValues();

    ScriptSerializer scriptSerializer;

    scriptSerializer.Script.Write(m_Script);

    for (const auto& value : m_FieldValues)
    {
        ScriptFieldSerializer fieldSerializer;

        fieldSerializer.Name.Write(value.Name);
        fieldSerializer.Type.Write(static_cast<std::int32_t>(value.Type));
        fieldSerializer.Value.Write(ByteSpan(value.Data.data(), value.Data.size()));

        scriptSerializer.Fields.AddData(fieldSerializer.Write());
    }

    return scriptSerializer.Write();
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

    // A fresh object starts on the C# field initializers, which is only right for a script that
    // was never authored. Put back whatever was.
    ApplyFieldValues();
}

void Pine::ScriptComponent::DestroyInstance()
{
    if (m_ScriptObjectHandle.Object == nullptr)
    {
        return;
    }

    Script::ObjectFactory::DisposeObject(&m_ScriptObjectHandle);
}

void Pine::ScriptComponent::CaptureFieldValues()
{
    const auto script = m_Script.Get();

    if (m_ScriptObjectHandle.Object == nullptr || script == nullptr)
    {
        return;
    }

    const auto scriptData = script->GetScriptData();

    if (scriptData == nullptr || !scriptData->IsReady)
    {
        return;
    }

    const auto object = mono_gchandle_get_target(m_ScriptObjectHandle.Handle);

    for (const auto& field : scriptData->Fields)
    {
        ScriptFieldValue value;

        if (!field->ReadValue(object, value))
        {
            continue;
        }

        if (const auto existing = FindValue(m_FieldValues, field->GetName()))
        {
            *existing = std::move(value);
        }
        else
        {
            m_FieldValues.push_back(std::move(value));
        }
    }
}

void Pine::ScriptComponent::ApplyFieldValues() const
{
    const auto script = m_Script.Get();

    if (m_ScriptObjectHandle.Object == nullptr || script == nullptr)
    {
        return;
    }

    const auto scriptData = script->GetScriptData();

    if (scriptData == nullptr || !scriptData->IsReady)
    {
        return;
    }

    const auto object = mono_gchandle_get_target(m_ScriptObjectHandle.Handle);

    for (const auto& field : scriptData->Fields)
    {
        for (const auto& value : m_FieldValues)
        {
            if (value.Name != field->GetName())
            {
                continue;
            }

            // A mismatched type is left alone rather than dropped: a field that changed from float
            // to Vector3 and back should find its old value still waiting for it.
            field->WriteValue(object, value);

            break;
        }
    }
}

Pine::Script::ObjectHandle *Pine::ScriptComponent::GetScriptObjectHandle()
{
    return &m_ScriptObjectHandle;
}

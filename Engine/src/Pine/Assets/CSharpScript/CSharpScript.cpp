#include "CSharpScript.hpp"

Pine::CSharpScript::CSharpScript()
{
    m_Type = AssetType::CSharpScript;
}

void Pine::CSharpScript::Dispose()
{
}

Pine::ScriptData *Pine::CSharpScript::GetScriptData() const
{
    return m_ScriptData;
}

void Pine::CSharpScript::SetScriptData(ScriptData *scriptData)
{
    m_ScriptData = scriptData;
}

const std::string& Pine::CSharpScript::GetTypeName() const
{
    return m_TypeName;
}

void Pine::CSharpScript::SetTypeName(const std::string& typeName)
{
    m_TypeName = typeName;
}

void Pine::CSharpScript::RegisterScriptComponent(ScriptComponent *scriptComponent)
{
    m_ScriptComponents.push_back(scriptComponent);
}

void Pine::CSharpScript::UnregisterScriptComponent(ScriptComponent *scriptComponent)
{
    m_ScriptComponents.erase(std::remove(m_ScriptComponents.begin(), m_ScriptComponents.end(), scriptComponent), m_ScriptComponents.end());
}

bool Pine::CSharpScript::LoadAssetData(const ByteSpan& span)
{
    CSharpScriptSerializer serializer;

    // Older script assets were saved with an empty payload (no stored type name). Treat an
    // unreadable/empty payload as "legacy": leave the type name empty so script resolution
    // falls back to the file-name convention, rather than failing to load the asset.
    if (!serializer.Read(span))
    {
        m_TypeName.clear();
        return true;
    }

    serializer.TypeName.Read(m_TypeName);

    return true;
}

Pine::ByteSpan Pine::CSharpScript::SaveAssetData()
{
    CSharpScriptSerializer serializer;

    serializer.TypeName.Write(m_TypeName);

    return serializer.Write();
}

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

void Pine::CSharpScript::RegisterScriptComponent(ScriptComponent *scriptComponent)
{
    m_ScriptComponents.push_back(scriptComponent);
}

void Pine::CSharpScript::UnregisterScriptComponent(ScriptComponent *scriptComponent)
{
    m_ScriptComponents.erase(std::remove(m_ScriptComponents.begin(), m_ScriptComponents.end(), scriptComponent), m_ScriptComponents.end());
}

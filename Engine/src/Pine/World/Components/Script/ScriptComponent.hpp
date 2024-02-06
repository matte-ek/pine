#pragma once

#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

namespace Pine
{

    class ScriptComponent : public IComponent
    {
    private:
        AssetHandle<CSharpScript> m_Script;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        void CreateInstance();
        void DestroyInstance();
    public:
        ScriptComponent();

        void SetScript(CSharpScript* script);
        CSharpScript* GetScript() const;

        Script::ObjectHandle* GetScriptObjectHandle();

        void OnCreated() override;
        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}
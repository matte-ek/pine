#pragma once

#include "Pine/Assets/Asset/Asset.hpp"

namespace Pine
{
    struct ScriptData;
    class ScriptComponent;

    class CSharpScript : public Asset
    {
    private:
        ScriptData* m_ScriptData = nullptr;

        std::vector<ScriptComponent*> m_ScriptComponents;
    public:
        CSharpScript();

        ScriptData* GetScriptData() const;
        void SetScriptData(ScriptData* scriptData);

        void RegisterScriptComponent(ScriptComponent* scriptComponent);
        void UnregisterScriptComponent(ScriptComponent* scriptComponent);

        bool LoadFromFile(AssetLoadStage stage) override;

        void Dispose() override;
    };
}
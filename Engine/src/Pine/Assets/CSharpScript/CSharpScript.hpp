#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    struct ScriptData;
    class ScriptComponent;

    class CSharpScript : public IAsset
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
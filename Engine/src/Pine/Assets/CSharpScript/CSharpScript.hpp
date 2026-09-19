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

        // Fully-qualified managed type name this script maps to (e.g. "Game.Player"). The actual
        // C# source lives in a sibling '.cs' file registered as an asset source; it is compiled
        // into the project's game assembly separately (see docs/scripting.md).
        std::string m_TypeName;

        struct CSharpScriptSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_STRING(TypeName);
        };
    public:
        CSharpScript();

        ScriptData* GetScriptData() const;
        void SetScriptData(ScriptData* scriptData);

        const std::string& GetTypeName() const;
        void SetTypeName(const std::string& typeName);

        void RegisterScriptComponent(ScriptComponent* scriptComponent);
        void UnregisterScriptComponent(ScriptComponent* scriptComponent);

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        void Dispose() override;
    };
}

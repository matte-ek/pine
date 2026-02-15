#pragma once

#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

namespace Pine
{

    class ScriptComponent final : public Component
    {
    private:
        AssetHandle<CSharpScript> m_Script;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        struct ScriptSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Script);
        };
    public:
        ScriptComponent();

        void SetScript(CSharpScript* script);
        CSharpScript* GetScript() const;

        void CreateInstance();
        void DestroyInstance();

        Script::ObjectHandle* GetScriptObjectHandle();

        void OnCreated() override;
        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}
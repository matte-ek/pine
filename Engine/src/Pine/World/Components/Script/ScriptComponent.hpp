#pragma once

#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/Script/Scripts/ScriptFieldValue.hpp"

#include <vector>

namespace Pine
{

    class ScriptComponent final : public Component
    {
    private:
        AssetHandle<CSharpScript> m_Script;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        // The authored values of the script's public fields.
        //
        // The managed object those fields live in is destroyed and rebuilt constantly - on level
        // load, on blueprint spawn, and on every hot reload, which resets the whole Mono domain -
        // so the values cannot live only in C#. This is the copy that survives all three, and
        // CreateInstance() puts it back into each new object.
        //
        // Values whose name no longer matches a field are kept rather than dropped. A script that
        // fails to compile, or a level loaded before the game assembly is, would otherwise silently
        // discard everything the author set.
        std::vector<ScriptFieldValue> m_FieldValues;

        struct ScriptSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(Script);
            PINE_SERIALIZE_ARRAY(Fields);
        };

        // One element of ScriptSerializer::Fields. Nested the same way a Blueprint nests its
        // components inside an entity, because the field list is as variable as that one is.
        struct ScriptFieldSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_STRING(Name);
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_DATA(Value);
        };
    public:
        ScriptComponent();

        void SetScript(CSharpScript* script);
        CSharpScript* GetScript() const;

        void CreateInstance();
        void DestroyInstance();

        // Move the script's field values between this component and its managed object. The engine
        // calls these either side of anything that destroys the object, so scripts and the editor
        // can keep working on the object itself and never have to think about it.
        void CaptureFieldValues();
        void ApplyFieldValues() const;

        Script::ObjectHandle* GetScriptObjectHandle();

        void OnCreated() override;
        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}

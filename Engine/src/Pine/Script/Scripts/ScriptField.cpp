#include "ScriptField.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"

#include "mono/metadata/appdomain.h"
#include "mono/utils/mono-publib.h"

#include <cstring>
#include <unordered_map>

namespace
{
    // The value types, matched on their full type name because that is all there is to match.
    const std::unordered_map<std::string, Pine::ScriptFieldType> ValueTypeNames =
    {
        { "System.Boolean", Pine::ScriptFieldType::Boolean },
        { "System.Int32", Pine::ScriptFieldType::Integer },
        { "System.Single", Pine::ScriptFieldType::Float },
        { "Pine.Math.Vector2", Pine::ScriptFieldType::Vector2 },
        { "Pine.Math.Vector3", Pine::ScriptFieldType::Vector3 },
        { "Pine.Math.Vector4", Pine::ScriptFieldType::Vector4 }
    };

    // Resolved on each use rather than cached: a MonoClass does not survive a domain reset, and
    // this only runs while reflecting a script class, which happens a handful of times per reload.
    MonoClass* GetAssetBaseClass()
    {
        const auto image = Pine::Script::Runtime::GetPineImage();

        if (image == nullptr)
        {
            return nullptr;
        }

        return mono_class_from_name(image, "Pine.Assets", "Asset");
    }

    std::string GetTypeName(MonoType* type)
    {
        auto name = mono_type_get_name(type);

        if (name == nullptr)
        {
            return {};
        }

        std::string result = name;

        mono_free(name);

        return result;
    }

    Pine::ScriptFieldType ClassifyField(MonoType* type, MonoClass** valueClass)
    {
        *valueClass = nullptr;

        if (type == nullptr)
        {
            return Pine::ScriptFieldType::Invalid;
        }

        const auto typeName = GetTypeName(type);

        if (const auto match = ValueTypeNames.find(typeName); match != ValueTypeNames.end())
        {
            return match->second;
        }

        if (typeName == "System.String")
        {
            return Pine::ScriptFieldType::String;
        }

        const auto fieldClass = mono_class_from_mono_type(type);

        if (fieldClass == nullptr)
        {
            return Pine::ScriptFieldType::Invalid;
        }

        if (typeName == "Pine.World.Entity")
        {
            *valueClass = fieldClass;

            return Pine::ScriptFieldType::Entity;
        }

        // An asset field is nearly always declared as the concrete asset (Model, Material, ...)
        // rather than as the base, so this has to be a subclass test. Matching on the type name
        // would only ever find Asset itself.
        const auto assetClass = GetAssetBaseClass();

        if (assetClass != nullptr && mono_class_is_subclass_of(fieldClass, assetClass, false))
        {
            *valueClass = fieldClass;

            return Pine::ScriptFieldType::Asset;
        }

        return Pine::ScriptFieldType::Invalid;
    }
}

Pine::ScriptField::ScriptField(const std::string& name, MonoClassField* field, ScriptData* parent, MonoType* type)
    : m_Name(name), m_Parent(parent), m_Field(field)
{
    m_Type = ClassifyField(type, &m_ValueClass);
}

Pine::ScriptFieldType Pine::ScriptField::GetType() const
{
    return m_Type;
}

const std::string& Pine::ScriptField::GetName() const
{
    return m_Name;
}

Pine::AssetType Pine::ScriptField::GetAssetType() const
{
    if (m_Type != ScriptFieldType::Asset || m_ValueClass == nullptr)
    {
        return AssetType::Invalid;
    }

    const auto className = mono_class_get_name(m_ValueClass);

    // Matched against AssetTypeToString rather than a second lookup table, so the two cannot drift,
    // and so the editor offers exactly the assets the object factory is able to hand to C#.
    for (auto candidate = static_cast<int>(AssetType::Invalid) + 1;
         candidate < static_cast<int>(AssetType::Count);
         candidate++)
    {
        const auto type = static_cast<AssetType>(candidate);

        if (std::strcmp(AssetTypeToString(type), className) == 0)
        {
            return type;
        }
    }

    return AssetType::Invalid;
}

bool Pine::ScriptField::ReadValue(MonoObject* object, ScriptFieldValue& value) const
{
    if (object == nullptr || m_Type == ScriptFieldType::Invalid)
    {
        return false;
    }

    value.Name = m_Name;
    value.Type = m_Type;
    value.Data.clear();

    if (m_Type == ScriptFieldType::String)
    {
        MonoString* string = nullptr;

        mono_field_get_value(object, m_Field, &string);

        // A null string and an empty one are both stored as no bytes; C# sees "" either way.
        if (string == nullptr)
        {
            return true;
        }

        auto text = mono_string_to_utf8(string);
        const auto bytes = reinterpret_cast<const std::byte*>(text);

        value.Data.assign(bytes, bytes + std::strlen(text));

        mono_free(text);

        return true;
    }

    if (m_Type == ScriptFieldType::Asset)
    {
        MonoObject* assetObject = nullptr;

        mono_field_get_value(object, m_Field, &assetObject);

        // A reference to nothing, stored as no bytes.
        if (assetObject == nullptr)
        {
            return true;
        }

        const auto idField = mono_class_get_field_from_name(mono_object_get_class(assetObject), "Id");

        if (idField == nullptr)
        {
            return false;
        }

        value.Data.resize(sizeof(UId));

        mono_field_get_value(assetObject, idField, value.Data.data());

        return true;
    }

    const auto size = ScriptFieldTypeSize(m_Type);

    // Entity, whose reference cannot be stored yet - see docs/scripting.md.
    if (size == 0)
    {
        return false;
    }

    value.Data.resize(size);

    mono_field_get_value(object, m_Field, value.Data.data());

    return true;
}

bool Pine::ScriptField::WriteValue(MonoObject* object, const ScriptFieldValue& value) const
{
    if (object == nullptr || value.Type != m_Type)
    {
        return false;
    }

    if (m_Type == ScriptFieldType::String)
    {
        const auto text = value.Data.empty() ? "" : reinterpret_cast<const char*>(value.Data.data());
        const auto string = mono_string_new_len(mono_domain_get(), text, value.Data.size());

        // A reference type is assigned the object pointer itself, not its address.
        mono_field_set_value(object, m_Field, string);

        return true;
    }

    if (m_Type == ScriptFieldType::Asset)
    {
        MonoObject* assetObject = nullptr;

        if (value.Data.size() == sizeof(UId))
        {
            const UId id(ByteSpan(value.Data.data(), value.Data.size()));

            if (const auto asset = Assets::GetAssetByUId(id))
            {
                assetObject = mono_gchandle_get_target(asset->GetScriptHandle()->Handle);
            }
        }

        // Deliberately assigned even when it resolved to nothing: the asset may since have been
        // deleted, and leaving the C# initializer in place would be a quieter kind of wrong.
        mono_field_set_value(object, m_Field, assetObject);

        return true;
    }

    const auto size = ScriptFieldTypeSize(m_Type);

    if (size == 0 || value.Data.size() != size)
    {
        return false;
    }

    mono_field_set_value(object, m_Field, const_cast<std::byte*>(value.Data.data()));

    return true;
}

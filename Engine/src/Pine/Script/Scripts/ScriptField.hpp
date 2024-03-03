#pragma once

#include "ScriptData.hpp"
#include "mono/metadata/class.h"
#include "mono/metadata/object-forward.h"
#include "mono/metadata/object.h"
#include <string>
#include <unordered_map>

namespace Pine
{
    enum class ScriptFieldType
    {
        Invalid,
        Boolean,
        Integer,
        Float,
        Vector2,
        Vector3,
        Vector4,
        Entity,
        Asset
    };

    inline static std::unordered_map<std::string, ScriptFieldType> ScriptFieldTypeMap = 
    {
        {"System.Boolean", ScriptFieldType::Boolean},
        {"System.Int32", ScriptFieldType::Integer},
        {"System.Single", ScriptFieldType::Float},
        {"Pine.Math.Vector2", ScriptFieldType::Vector2},
        {"Pine.Math.Vector3", ScriptFieldType::Vector3},
        {"Pine.Math.Vector4", ScriptFieldType::Vector4},
        {"Pine.World.Entity", ScriptFieldType::Entity},
        {"Pine.Asset", ScriptFieldType::Asset}
    };

    inline static ScriptFieldType TranslateScriptFieldType(const std::string& type)
    {
        if (ScriptFieldTypeMap.count(type) == 0)
            return ScriptFieldType::Invalid;
        
        return ScriptFieldTypeMap[type];
    }

    inline const char* ScriptFieldTypeToString(ScriptFieldType type)
    {
        switch (type) 
        {
            case ScriptFieldType::Invalid:
                return "Invalid";
            case ScriptFieldType::Boolean:
                return "Boolean";
            case ScriptFieldType::Integer:
                return "Integers";
            case ScriptFieldType::Float:
                return "Float";
            case ScriptFieldType::Vector2:
                return "Vector2";
            case ScriptFieldType::Vector3:
                return "Vector3";
            case ScriptFieldType::Vector4:
                return "Vector4";
            case ScriptFieldType::Entity:
                return "Entity";
            case ScriptFieldType::Asset:
                return "Asset";                
            default:
                return "Invalid";
        }
    }

    class ScriptField
    {
    private:
        std::string m_Name;
        ScriptFieldType m_Type;
        ScriptData* m_Parent;
        MonoClassField* m_Field;
    public:

        ScriptField(const std::string& name, MonoClassField* field, ScriptData* parent, const std::string& type)
            : m_Name(name), m_Field(field), m_Parent(parent)
        {
            m_Type = TranslateScriptFieldType(type);
        }

        ScriptFieldType GetType() const
        {
            return m_Type;
        }

        const std::string& GetName() const
        {
            return m_Name;
        }

        template<typename T>
        T Get(MonoObject* object)
        {
            T value;

            mono_field_get_value(object, m_Field, &value);

            return value;
        }

        template<typename T>
        void Set(MonoObject* object, T value)
        {
            mono_field_set_value(object, m_Field, (void*)&value);
        }
    };
}
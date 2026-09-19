#pragma once

#include "Pine/Core/UId/UId.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Pine
{
    // The C# field types a script may expose to the editor. A public field of any other type is
    // simply not reflected - it still works in C#, it just cannot be authored or saved.
    //
    // These values are written into saved components, so they are pinned rather than positional:
    // append new types, never renumber the existing ones.
    enum class ScriptFieldType : std::int32_t
    {
        Invalid = 0,
        Boolean = 1,
        Integer = 2,
        Float = 3,
        Vector2 = 4,
        Vector3 = 5,
        Vector4 = 6,
        Entity = 7,     // Reflected for the editor, but not yet stored - see ScriptField::ReadValue.
        Asset = 8,
        String = 9
    };

    inline const char* ScriptFieldTypeToString(const ScriptFieldType type)
    {
        switch (type)
        {
            case ScriptFieldType::Boolean:
                return "Boolean";
            case ScriptFieldType::Integer:
                return "Integer";
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
            case ScriptFieldType::String:
                return "String";
            default:
                return "Invalid";
        }
    }

    // How many bytes this type occupies in ScriptFieldValue::Data, or zero for the variable-length
    // types (String) and for anything that isn't stored.
    inline std::size_t ScriptFieldTypeSize(const ScriptFieldType type)
    {
        switch (type)
        {
            case ScriptFieldType::Boolean:
                return 1; // Stored as one byte, whatever width a managed bool marshals as.
            case ScriptFieldType::Integer:
                return sizeof(std::int32_t);
            case ScriptFieldType::Float:
                return sizeof(float);
            case ScriptFieldType::Vector2:
                return sizeof(float) * 2;
            case ScriptFieldType::Vector3:
                return sizeof(float) * 3;
            case ScriptFieldType::Vector4:
                return sizeof(float) * 4;
            case ScriptFieldType::Asset:
                return sizeof(UId);
            default:
                return 0;
        }
    }

    // One script field's authored value, held on the engine side so that it outlives the managed
    // object it was read from. The managed object a script field lives in is destroyed and rebuilt
    // constantly - on level load, on blueprint spawn, and on every hot reload, which resets the
    // whole domain - so the value cannot live only in C#.
    //
    // Data holds the value in the form it is saved in, which is also the form it is kept in: the
    // raw bytes of the value type for the fixed-size types, the UTF-8 characters without a
    // terminator for String, and the referenced asset's UId for Asset. Type says how to read it.
    // Empty Data is meaningful - it is an empty string, or a reference to nothing.
    struct ScriptFieldValue
    {
        std::string Name;
        ScriptFieldType Type = ScriptFieldType::Invalid;
        std::vector<std::byte> Data;
    };
}

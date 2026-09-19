#pragma once

#include "ScriptData.hpp"
#include "ScriptFieldValue.hpp"

#include "mono/metadata/class.h"
#include "mono/metadata/object-forward.h"
#include "mono/metadata/object.h"

#include <string>

namespace Pine
{
    // One public field on a script class, reflected out of the game assembly so the editor can show
    // it and the engine can store it.
    //
    // A field is bound to the class, not to an instance: every method here takes the managed object
    // to read from or write to. That object is not stable - see ScriptFieldValue for why values are
    // kept outside it.
    class ScriptField
    {
    private:
        std::string m_Name;
        ScriptFieldType m_Type = ScriptFieldType::Invalid;
        ScriptData* m_Parent;
        MonoClassField* m_Field;

        // The field's declared class, for the reference types. Null for the value types.
        MonoClass* m_ValueClass = nullptr;
    public:
        ScriptField(const std::string& name, MonoClassField* field, ScriptData* parent, MonoType* type);

        ScriptFieldType GetType() const;
        const std::string& GetName() const;

        // The asset type an Asset field is declared as, so the editor can restrict its picker to
        // it. Invalid for every other field type, and for an asset class the engine has no
        // AssetType for.
        AssetType GetAssetType() const;

        // Read and write the field directly, for the value types only. A reference type needs the
        // object pointer itself rather than its address, so Set() would corrupt one - use
        // WriteValue() for those.
        template<typename T>
        T Get(MonoObject* object) const
        {
            T value;

            mono_field_get_value(object, m_Field, &value);

            return value;
        }

        template<typename T>
        void Set(MonoObject* object, T value) const
        {
            mono_field_set_value(object, m_Field, static_cast<void*>(&value));
        }

        // Read the field into, and write it back out of, the type-tagged form that gets stored.
        // Unlike Get/Set these handle every supported type, including the reference ones.
        //
        // WriteValue ignores a value whose type no longer matches the field's - a script that
        // changed `float Speed` into `Vector3 Speed` must not have the old four bytes read as a
        // vector - and reports whether it wrote anything.
        bool ReadValue(MonoObject* object, ScriptFieldValue& value) const;
        bool WriteValue(MonoObject* object, const ScriptFieldValue& value) const;
    };
}

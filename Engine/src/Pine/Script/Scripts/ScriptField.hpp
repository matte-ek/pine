#pragma once

#include "ScriptData.hpp"
#include "ScriptFieldValue.hpp"

#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

#include <string>

namespace Pine
{
    namespace Script
    {
        struct ScriptFieldDescriptor;
    }

    // One field on a script class, reflected out of the game assembly so the editor can show it and
    // the engine can store it. Which fields those are, and how they should be shown, is decided in
    // Pine.dll - see Pine.Core.Reflection.FieldRegistry.
    //
    // A field is bound to the class, not to an instance: every method here takes a handle to the
    // managed object to read from or write to. That object is not stable - see ScriptFieldValue
    // for why values are kept outside it.
    class ScriptField
    {
    private:
        std::string m_Name;
        ScriptFieldType m_Type = ScriptFieldType::Invalid;
        AssetType m_AssetType = AssetType::Invalid;
        ScriptData* m_Parent;

        // Where this field sits in the scripting runtime's own registry. Both stay good until the
        // next reload, which rebuilds every ScriptField anyway.
        int m_ClassId = -1;
        int m_FieldIndex = -1;

        // How the editor should draw the field, from the attributes it was declared with. None of
        // this affects what is stored, so a script may gain or lose an attribute freely.
        bool m_HasRange = false;
        float m_RangeMin = 0.f;
        float m_RangeMax = 0.f;
        bool m_HasSpace = false;
        std::string m_Header;
        std::string m_Tooltip;
    public:
        ScriptField(ScriptData* parent, int classId, int fieldIndex, const Script::ScriptFieldDescriptor& descriptor);

        ScriptField(const ScriptField&) = delete;
        ScriptField& operator=(const ScriptField&) = delete;

        ScriptFieldType GetType() const;
        const std::string& GetName() const;

        // The asset type an Asset field is declared as, so the editor can restrict its picker to
        // it. Invalid for every other field type, and for an asset class the engine has no
        // AssetType for.
        AssetType GetAssetType() const;

        // Draw the field as a slider between these two, rather than as an input field. Only the
        // Float and Integer fields are drawn that way; the range is ignored on any other type.
        bool HasRange() const;
        float GetRangeMin() const;
        float GetRangeMax() const;

        // Blank space above the field's row, from [Space].
        bool HasSpace() const;

        // A separator labelled with this above the field's row, from [Header]. Empty when the field
        // has none, as is the tooltip when it has none.
        const std::string& GetHeader() const;
        const std::string& GetTooltip() const;

        // Read the field into, and write it back out of, the type-tagged form that gets stored.
        // These handle every supported type, including the reference ones.
        //
        // WriteValue ignores a value whose type no longer matches the field's - a script that
        // changed `float Speed` into `Vector3 Speed` must not have the old four bytes read as a
        // vector - and reports whether it wrote anything.
        bool ReadValue(const Script::ObjectHandle& object, ScriptFieldValue& value) const;
        bool WriteValue(const Script::ObjectHandle& object, const ScriptFieldValue& value) const;
    };
}

#include "ScriptField.hpp"

#include "Pine/Script/Scripts/ScriptFieldRegistry.hpp"

namespace
{
    // The descriptor's strings belong to the managed registry and only last until it is reset, so
    // everything read out of one is copied on the spot.
    std::string CopyOf(const char* text)
    {
        return text == nullptr ? std::string() : std::string(text);
    }
}

Pine::ScriptField::ScriptField(ScriptData* parent, const int classId, const int fieldIndex,
    const Script::ScriptFieldDescriptor& descriptor)
    : m_Name(CopyOf(descriptor.Name)),
      m_Type(static_cast<ScriptFieldType>(descriptor.Type)),
      m_AssetType(static_cast<AssetType>(descriptor.AssetType)),
      m_Parent(parent),
      m_ClassId(classId),
      m_FieldIndex(fieldIndex),
      m_HasRange((descriptor.Flags & Script::ScriptFieldFlag_HasRange) != 0),
      m_RangeMin(descriptor.RangeMin),
      m_RangeMax(descriptor.RangeMax),
      m_HasSpace((descriptor.Flags & Script::ScriptFieldFlag_HasSpace) != 0),
      m_Header(CopyOf(descriptor.Header)),
      m_Tooltip(CopyOf(descriptor.Tooltip))
{
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
    return m_AssetType;
}

bool Pine::ScriptField::HasRange() const
{
    return m_HasRange;
}

float Pine::ScriptField::GetRangeMin() const
{
    return m_RangeMin;
}

float Pine::ScriptField::GetRangeMax() const
{
    return m_RangeMax;
}

bool Pine::ScriptField::HasSpace() const
{
    return m_HasSpace;
}

const std::string& Pine::ScriptField::GetHeader() const
{
    return m_Header;
}

const std::string& Pine::ScriptField::GetTooltip() const
{
    return m_Tooltip;
}

bool Pine::ScriptField::ReadValue(const Script::ObjectHandle& object, ScriptFieldValue& value) const
{
    std::vector<std::byte> data;

    if (!Script::FieldRegistry::ReadValue(object, m_ClassId, m_FieldIndex, data))
    {
        return false;
    }

    value.Name = m_Name;
    value.Type = m_Type;
    value.Data = std::move(data);

    return true;
}

bool Pine::ScriptField::WriteValue(const Script::ObjectHandle& object, const ScriptFieldValue& value) const
{
    if (value.Type != m_Type)
    {
        return false;
    }

    return Script::FieldRegistry::WriteValue(object, m_ClassId, m_FieldIndex, value.Data);
}

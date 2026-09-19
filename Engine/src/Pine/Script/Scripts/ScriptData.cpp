#include "ScriptData.hpp"

#include "Pine/Script/Scripts/ScriptField.hpp"

Pine::ScriptData::~ScriptData()
{
    for (const auto field : Fields)
    {
        delete field;
    }
}

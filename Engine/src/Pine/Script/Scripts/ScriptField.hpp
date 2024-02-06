#pragma once

#include "ScriptData.hpp"

namespace Pine
{

    template<class T>
    class ScriptField
    {
    private:
        std::string m_Name;
        ScriptData* m_Parent;
    public:

        ScriptField(const std::string& name, ScriptData* parent)
            : m_Name(name), m_Parent(parent)
        {
        }

        T Get()
        {
            return T();
        }

        void Set(T value)
        {
        }
    };
}
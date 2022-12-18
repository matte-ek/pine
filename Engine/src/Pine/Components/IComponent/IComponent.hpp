#pragma once

namespace Pine
{

    enum class ComponentType
    {
        Invalid,
        Transform
    };

    class Entity;

    class IComponent
    {
    protected:

        bool m_Active = true;

        ComponentType m_Type = ComponentType::Invalid;

        Entity* m_Parent = nullptr;

    public:
    };

}
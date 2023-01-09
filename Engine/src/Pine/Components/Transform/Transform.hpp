#pragma once

#include "Pine/Components/IComponent/IComponent.hpp"

namespace Pine
{

    class Transform : public IComponent
    {
    private:
    public:
        explicit Transform(Entity* parent);
    };

}
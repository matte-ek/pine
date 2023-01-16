#pragma once

#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine::Pipeline2D
{

    void Setup();
    void Shutdown();

    void Run(RenderingContext& context);

}
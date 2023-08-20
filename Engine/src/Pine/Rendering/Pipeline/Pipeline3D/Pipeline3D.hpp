#pragma once

#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine::Pipeline3D
{

    void Setup();
    void Shutdown();

    void Run(const RenderingContext& context);

}
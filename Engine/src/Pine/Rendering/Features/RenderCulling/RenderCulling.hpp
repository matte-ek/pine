#pragma once
#include "Pine/World/Components/Camera/Camera.hpp"

namespace Pine::Rendering::RenderCulling
{
    void RunFrustumCulling(const Camera* camera);
}

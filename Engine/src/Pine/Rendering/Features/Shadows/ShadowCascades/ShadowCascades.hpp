#pragma once

#include <vector>

#include "Pine/Rendering/Features/Shadows/ShadowView/ShadowView.hpp"

namespace Pine
{
    class Camera;
    class Light;
}

namespace Pine::Rendering::ShadowCascades
{
    // Pins the cascades' atlas tiles for the process lifetime. Call after ShadowAtlas::Setup.
    void Setup();

    // Builds one ShadowView per cascade for 'sceneCamera', into those pinned tiles, and points the
    // light at the head of the shader's view array. Called per rendering context - see
    // Shadows::RenderPassLight.
    //
    // Returns false when there is no camera or no reservation, leaving the views and the light
    // untouched.
    bool BuildViews(Light* light, Camera* sceneCamera);

    // The views BuildViews last filled, in cascade order. Mutable because the caller culls into
    // and renders them.
    //
    // Kept alive between frames because each ShadowView owns a VisibilitySet sized to
    // m_MaxObjectCount, which should be allocated once rather than per context per frame.
    std::vector<ShadowView>& GetViews();
}

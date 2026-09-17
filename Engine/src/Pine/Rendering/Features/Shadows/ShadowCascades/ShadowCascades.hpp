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
    // light at the head of the shader's view array.
    //
    // Called per rendering context rather than per frame, unlike the local shadow views: a cascade
    // is derived from the camera frustum, so an editor viewport and a game camera genuinely need
    // different ones, and each context builds and renders its own immediately before it draws.
    //
    // Returns false when there is nothing to build into - no camera, or no reservation - in which
    // case the views are left alone and the light is not pointed at them.
    bool BuildViews(Light* light, Camera* sceneCamera);

    // The views BuildViews last filled, in cascade order.
    //
    // Mutable, because the caller culls into them and then renders them: shadow statistics and the
    // tile cache both belong to Shadows, so this module builds projections and leaves the rest
    // alone. The storage lives here rather than at the call site because a ShadowView owns a
    // VisibilitySet sized to m_MaxObjectCount, and these have to survive between frames for that
    // allocation to be made once rather than every context, every frame.
    std::vector<ShadowView>& GetViews();
}

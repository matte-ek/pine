#pragma once
#include <array>

#include "Pine/Core/Math/Math.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/World/Components/Components.hpp"

namespace Pine
{
    class Light;

    namespace Renderer3D
    {
        // The lights one drawn thing is lit by: one entry per slot the generic shader carries,
        // holding the light that won that slot or an invalid handle where nothing did. Filled by
        // the scene processor, read by AddInstance and RenderMesh when they write the instance.
        //
        // Its own type rather than a part of ModelRendererHintData because a terrain chunk needs
        // exactly this and none of the rest of that struct: a chunk is not a component, has no
        // transform and keeps its bounds on the chunk itself.
        struct LightSlotData
        {
            // False until the slots have been filled, and cleared again whenever something they
            // depend on moves. Slots survive from one frame to the next precisely because this
            // says whether they are still good; see SceneProcessor::Lights::Prepare.
            bool HasComputedData = false;

            // The point the slots were picked from. Slots are picked by distance, so they only go
            // stale when this point or a light moves.
            Vector3f Origin = Vector3f(0.f);

            std::array<ComponentHandle<Light>, Specifications::ObjectLightSlots::COUNT> Index = {};
        };
    }
}

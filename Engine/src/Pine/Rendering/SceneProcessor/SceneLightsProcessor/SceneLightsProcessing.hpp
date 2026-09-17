#pragma once
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Rendering/Renderer3D/LightSlotData.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace Pine::Rendering::SceneProcessor
{
    struct SceneProcessorContext;
}

namespace Pine::Rendering::SceneProcessor::Lights
{
    void Prepare(SceneProcessorContext& context);

    // Fills the slots with the nearest lights to a point, and marks them computed.
    //
    // Takes a position rather than the thing at that position because two rather different things
    // are lit by this rule: a model renderer, which is lit at its origin, and a terrain chunk,
    // which is lit at the centre of its box and is not a component at all. Whether the slots
    // *needed* recomputing is the caller's question - it knows what its own inputs are.
    void AssignSlots(const SceneProcessorContext& context, const Vector3f& position, Renderer3D::LightSlotData& slots);

    void ProcessModelRenderer(const SceneProcessorContext& context, ModelRenderer* modelRenderer);
}

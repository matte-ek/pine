#pragma once
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace Pine::Rendering::SceneProcessor
{
    struct SceneProcessorContext;
}

namespace Pine::Rendering::SceneProcessor::Lights
{
    void Prepare(SceneProcessorContext& context);

    void ProcessModelRenderer(const SceneProcessorContext& context, ModelRenderer* modelRenderer);
}

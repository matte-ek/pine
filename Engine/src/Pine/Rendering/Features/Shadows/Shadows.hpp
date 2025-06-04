#pragma once

namespace Pine
{
    namespace Pipeline3D
    {
        struct ObjectBatchData;
    }

    class Light;
}

namespace Pine::Rendering::Shadows
{
    void Setup();
    void Shutdown();

    void NewFrame();

    void RenderPassLight(const Light *light, const Pipeline3D::ObjectBatchData &batchData);
}

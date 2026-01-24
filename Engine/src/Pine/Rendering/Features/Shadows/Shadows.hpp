#pragma once
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"

namespace Pine
{
    namespace Graphics
    {
        class ITexture;
    }

    namespace Rendering
    {
        struct ObjectBatchData;
    }

    class Light;
    class Camera;
}

namespace Pine::Rendering::Shadows
{
    struct ShadowConfiguration
    {
    };

    void Setup();
    void Shutdown();

    void NewFrame(Camera* sceneCamera);

    void RenderPassLight(const Light *light, const ObjectBatchData &batchData);

    void UploadShadowData(const Light* light);

    Graphics::ITexture* GetShadowMap(const Light *light);
}

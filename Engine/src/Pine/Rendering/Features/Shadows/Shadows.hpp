#pragma once

namespace Pine
{
    namespace Pipeline3D
    {
        struct ObjectBatchData;
    }

    namespace Graphics
    {
        class ITexture;
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

    void RenderPassLight(const Light *light, const Pipeline3D::ObjectBatchData &batchData);

    void UploadShadowData(const Light* light);

    Graphics::ITexture* GetShadowMap(const Light *light);
}

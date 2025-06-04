#include "Shadows.hpp"

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/World/Components/Light/Light.hpp"

constexpr int SHADOW_MAP_RESOLUTION = 512;

using namespace Pine;

namespace
{
    Graphics::IFrameBuffer* m_DirectionalShadowMapBuffer = nullptr;

    void RenderScene(const Pipeline3D::ObjectBatchMap& mapBatch)
    {

    }

    void HandleDirectionalShadowMap(const Light* light, const Pipeline3D::ObjectBatchData &batchData)
    {
        Graphics::GetGraphicsAPI()->SetBlendingEnabled(false);


    }
}

void Rendering::Shadows::Setup()
{
    m_DirectionalShadowMapBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_DirectionalShadowMapBuffer->Bind();
    m_DirectionalShadowMapBuffer->Create(SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, Graphics::DepthBuffer);
}

void Rendering::Shadows::Shutdown()
{
    Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_DirectionalShadowMapBuffer);
}

void Rendering::Shadows::NewFrame()
{
}

void Rendering::Shadows::RenderPassLight(const Light* light, const Pipeline3D::ObjectBatchData &batchData)
{
    if (light->GetLightType() == LightType::Directional)
    {
        HandleDirectionalShadowMap(light, batchData);
    }
}
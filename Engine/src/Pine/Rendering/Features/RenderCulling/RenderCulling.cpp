#include "RenderCulling.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

void Pine::Rendering::RenderCulling::RunFrustumCulling(const Camera* camera)
{
    const auto frustumCorners = camera->GetFrustumCorners();

    auto min = Vector3f(std::numeric_limits<float>::max());
    auto max = Vector3f(std::numeric_limits<float>::lowest());

    for (const auto& corner : frustumCorners)
    {
        min = glm::min(min, corner);
        max = glm::max(max, corner);
    }

    const auto center = (min + max) * 0.5f;
    const float size = glm::distance2(min, max);

    int culledObjects = 0;

    for (auto& modelRenderer : Components::Get<ModelRenderer>())
    {
        auto& renderingHintData = modelRenderer.GetRenderingHintData();

        if (!modelRenderer.GetModel())
        {
            continue;
        }

        auto model = modelRenderer.GetModel();
        auto transform = modelRenderer.GetParent()->GetTransform();

        const auto position = transform->GetPosition();

        const auto modelMin = model->GetBoundingBoxMin() * transform->GetScale();
        const auto modelMax = model->GetBoundingBoxMax() * transform->GetScale();
        const auto modelCenter = (modelMin + modelMax) * 0.5f + position;

        if (glm::distance2(modelCenter, center) < size)
        {
            renderingHintData.HasPassedFrustumCulling = true;
        }
        else
        {
            renderingHintData.HasPassedFrustumCulling = false;
            culledObjects++;
        }
    }
}

#include "SceneLightsProcessing.hpp"

#include <limits>

#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    bool m_WorldLightRecomputationRequired = false;

    std::vector<Pine::Light*> GetWorldLights()
    {
        std::vector<Pine::Light*> lights;

        for (auto& light : Pine::Components::Get<Pine::Light>())
        {
            if (light.GetParent()->IsDirty())
            {
                m_WorldLightRecomputationRequired = true;
            }

            // TODO: Add checks to check if this light is relevant. I'm sure we can come up with some things.
            lights.push_back(&light);
        }

        return lights;
    }

    void PrepareModelRendererInstance(const Pine::Rendering::SceneProcessor::SceneProcessorContext& context, Pine::ModelRenderer* modelRenderer)
    {
        constexpr float float_max = std::numeric_limits<float>::max();

        auto& data = modelRenderer->GetRenderingHintData();
        bool computationRequired = !data.HasComputedData || !modelRenderer->GetParent()->GetStatic() || modelRenderer->GetParent()->IsDirty();

        if (!computationRequired)
        {
            return;
        }

        std::array<Pine::Light*, 4> lightCandidates {nullptr, nullptr, nullptr, nullptr};
        std::array<float, 3> lightDistances {float_max, float_max, float_max};

        for (auto light : context.Lights)
        {
            auto lightType = light->GetLightType();

            if (lightType == Pine::LightType::Directional)
            {
                continue;
            }

            if (lightType == Pine::LightType::SpotLight)
            {
                lightCandidates[3] = light;
                continue;
            }

            const auto modelPosition = modelRenderer->GetParent()->GetTransform()->GetPosition();
            const auto lightPosition = light->GetParent()->GetTransform()->GetPosition();
            const auto length = glm::distance2(modelPosition, lightPosition);

            if (!lightCandidates[0] || length < lightDistances[0])
            {
                lightCandidates[2] = lightCandidates[1];
                lightCandidates[1] = lightCandidates[0];
                lightCandidates[0] = light;

                lightDistances[2] = lightDistances[1];
                lightDistances[1] = lightDistances[0];
                lightDistances[0] = length;
            }
            else if (!lightCandidates[1] || length < lightDistances[1])
            {
                lightCandidates[2] = lightCandidates[1];
                lightCandidates[1] = light;

                lightDistances[2] = lightDistances[1];
                lightDistances[1] = length;
            }
            else if (!lightCandidates[2] || length < lightDistances[2])
            {
                lightDistances[2] = length;
                lightCandidates[2] = light;
            }
        }

        if (lightCandidates[0])
        {
            data.LightSlotIndex[0] = lightCandidates[0];
        }

        if (lightCandidates[1])
        {
            data.LightSlotIndex[1] = lightCandidates[1];
        }

        if (lightCandidates[2])
        {
            data.LightSlotIndex[2] = lightCandidates[2];
        }

        data.HasComputedData = true;
    }
}

void Pine::Rendering::SceneProcessor::Lights::Prepare(SceneProcessorContext& context)
{
    m_WorldLightRecomputationRequired = false;

    context.Lights = GetWorldLights();

    if (m_WorldLightRecomputationRequired)
    {
        for (auto& modelRenderer : Components::Get<ModelRenderer>())
        {
            modelRenderer.GetRenderingHintData().HasComputedData = false;
        }
    }
}

void Pine::Rendering::SceneProcessor::Lights::ProcessModelRenderer(const SceneProcessorContext& context, ModelRenderer* modelRenderer)
{
    PrepareModelRendererInstance(context, modelRenderer);
}

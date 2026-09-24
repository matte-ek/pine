#include "SceneLightsProcessing.hpp"

#include <array>
#include <limits>

#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Pine;

    namespace Slots = Renderer3D::Specifications::ObjectLightSlots;

    // Keeps the 'Count' nearest lights inserted so far, sorted by ascending distance. 'Count' is a
    // light slot count, so it stays small enough that a linear insert beats anything smarter.
    template <int Count>
    class NearestLights
    {
        std::array<Light*, Count> m_Lights = {};
        std::array<float, Count> m_Distances = {};
    public:
        NearestLights()
        {
            m_Distances.fill(std::numeric_limits<float>::max());
        }

        void Insert(Light* light, const float distanceSqr)
        {
            for (int i = 0; i < Count; i++)
            {
                if (distanceSqr >= m_Distances[i])
                {
                    continue;
                }

                // Move the lights we're closer than one slot back, dropping the furthest one.
                for (int j = Count - 1; j > i; j--)
                {
                    m_Lights[j] = m_Lights[j - 1];
                    m_Distances[j] = m_Distances[j - 1];
                }

                m_Lights[i] = light;
                m_Distances[i] = distanceSqr;

                return;
            }
        }

        // Returns nullptr if fewer than 'index' lights were inserted.
        Light* Get(const int index) const
        {
            return m_Lights[index];
        }
    };

    // Gathers the lights of the world for this frame. Returns true if the set of lights changed in a
    // way that invalidates the light slots computed for the objects during the previous frame.
    bool CollectWorldLights(std::vector<Light*>& lights)
    {
        const std::size_t previousLightCount = lights.size();

        // Keeps the allocated capacity around, this runs every frame.
        lights.clear();

        bool lightsChanged = false;

        for (auto& light : Components::Get<Light>())
        {
            // Not culled by range against the camera here, even though Range now makes that a
            // two-line test. The light set feeds ProcessModelRenderer's per-object slot cache,
            // which is scene-level state shared by every rendering context - so filtering it by one
            // camera's frustum would drop lights the *other* context can still see. In the editor
            // the primary context is the game view (RenderHandler.cpp), not the viewport being
            // looked at, so this would visibly pop lights out of the viewport.
            //
            // Doing it properly means either a per-context light set (and per-context slot caches
            // with it) or culling against the union of active frustums. Neither is a prerequisite
            // for shadows, so it is deliberately not bundled in here.
            lights.push_back(&light);

            // Slots are picked by distance, so moving a light changes them and turning it does not.
            // The entity flag is the general "something about this entity changed" signal, which
            // Light::SetLightType raises since the type decides which slot bucket a light competes
            // for. SceneProcessor::EndFrame clears it each frame.
            auto& hintData = light.GetLightHintData();
            const auto position = light.GetTransform()->GetPosition();

            if (hintData.SlotPosition != position || light.GetParent()->IsDirty())
            {
                lightsChanged = true;
            }

            hintData.SlotPosition = position;
        }

        // A light being created or destroyed changes which lights are the nearest ones as well.
        return lightsChanged || lights.size() != previousLightCount;
    }
}

void Pine::Rendering::SceneProcessor::Lights::Prepare(SceneProcessorContext& context)
{
    PINE_PF_SCOPE();

    context.LightSetChanged = CollectWorldLights(context.Lights);

    if (!context.LightSetChanged)
    {
        return;
    }

    // Only the model renderers are invalidated here. Terrain chunks are lit by the same rule but
    // are not components, and the terrains holding them are reached through the renderer rather
    // than through a component block - so they read the flag above and clear their own.
    for (auto& modelRenderer : Components::Get<ModelRenderer>())
    {
        modelRenderer.GetRenderingHintData().Lights.HasComputedData = false;
    }
}

void Pine::Rendering::SceneProcessor::Lights::AssignSlots(const SceneProcessorContext& context,
                                                          const Vector3f& position,
                                                          Renderer3D::LightSlotData& slots)
{
    PINE_PF_SCOPE();

    NearestLights<Slots::POINT_LIGHT_COUNT> pointLights;
    NearestLights<Slots::SPOT_LIGHT_COUNT> spotLights;

    for (const auto light : context.Lights)
    {
        const auto lightType = light->GetLightType();

        // Directional lights are global, they always occupy light index 0 and never an object slot.
        if (lightType == LightType::Directional)
        {
            continue;
        }

        const auto distanceSqr = glm::distance2(position, light->GetTransform()->GetPosition());

        if (lightType == LightType::SpotLight)
        {
            spotLights.Insert(light, distanceSqr);
        }
        else
        {
            pointLights.Insert(light, distanceSqr);
        }
    }

    // Slots without a light are assigned nullptr, which invalidates the handle.
    for (int i = 0; i < Slots::POINT_LIGHT_COUNT; i++)
    {
        slots.Index[Slots::POINT_LIGHT_OFFSET + i] = pointLights.Get(i);
    }

    for (int i = 0; i < Slots::SPOT_LIGHT_COUNT; i++)
    {
        slots.Index[Slots::SPOT_LIGHT_OFFSET + i] = spotLights.Get(i);
    }

    slots.HasComputedData = true;
    slots.Origin = position;
}

void Pine::Rendering::SceneProcessor::Lights::ProcessModelRenderer(const SceneProcessorContext& context, ModelRenderer* modelRenderer)
{
    auto& slots = modelRenderer->GetRenderingHintData().Lights;
    const auto position = modelRenderer->GetTransform()->GetPosition();

    // Which lights an object ends up with only depends on where it and the lights are, so unless one
    // of them moved (Prepare clears HasComputedData then) the previous frame's slots still hold.
    if (slots.HasComputedData && slots.Origin == position)
    {
        return;
    }

    AssignSlots(context, position, slots);
}

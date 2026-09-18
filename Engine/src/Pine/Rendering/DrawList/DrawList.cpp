#include "DrawList.hpp"

#include <algorithm>
#include <cmath>

#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace
{
    using namespace Pine;

    // Distance from the viewer to the nearest point of the object's world bounds, 0 while the
    // viewer is inside them. What front-to-back ordering wants: early depth rejection turns on
    // which surface the viewer meets first, not on where the object's middle happens to be. A
    // ground plane whose centre is far away still occludes everything the moment it is underfoot.
    float NearestBoundsDistance(const Vector3f& viewPosition, const Renderer3D::ModelRendererHintData& bounds)
    {
        const auto nearestPoint = glm::clamp(viewPosition, bounds.BoundsMin, bounds.BoundsMax);

        return glm::length(nearestPoint - viewPosition);
    }

    // Distance to the middle of the object's world bounds. Blending sorts on this rather than on
    // the nearest point because it is the stabler of the two: the nearest point jumps from one face
    // of the box to another as the viewer moves around it, and every jump is a chance for two
    // objects to swap places and pop.
    float CentreDistance(const Vector3f& viewPosition, const Renderer3D::ModelRendererHintData& bounds)
    {
        const auto centre = (bounds.BoundsMin + bounds.BoundsMax) * 0.5f;

        return glm::length(centre - viewPosition);
    }
}

void Pine::Rendering::DrawList::Build(const ObjectBatchMap& batch,
                                      const MaterialRenderingMode mode,
                                      const RenderCulling::VisibilitySet& visibility,
                                      const DrawOrdering& ordering)
{
    PINE_PF_SCOPE();

    m_Items.clear();

    for (const auto& [modelGroup, instances] : batch)
    {
        int meshIndex = -1;

        for (const auto mesh : modelGroup.ModelPtr->GetMeshes())
        {
            meshIndex++;

            auto* material = modelGroup.OverrideMaterial != nullptr ? modelGroup.OverrideMaterial : mesh->GetMaterial();

            // A mesh without a material is left in whichever mode is asking, which is what the
            // batch loop this replaced did with it.
            if (material != nullptr && material->GetRenderingMode() != mode)
            {
                continue;
            }

            for (const auto& [renderer] : instances)
            {
                if (!visibility.IsVisible(renderer->GetInternalId()))
                {
                    continue;
                }

                // An object can be pointed at a single mesh of its model, in which case every
                // other mesh of that model is not its to draw.
                const int rendererMeshIndex = renderer->GetModelMeshIndex();
                if (rendererMeshIndex >= 0 && rendererMeshIndex != meshIndex)
                {
                    continue;
                }

                m_Items.push_back({ mesh, material, renderer, 0.f });
            }
        }
    }

    if (ordering.Order == DrawOrder::Batched)
    {
        // The batch is already grouped by mesh and material, so collecting it in order produced
        // the runs the submitter is looking for. Nothing left to do, and nothing to sort.
        return;
    }

    const bool frontToBack = ordering.Order == DrawOrder::FrontToBack;

    for (auto& item : m_Items)
    {
        const auto& bounds = item.Renderer->GetRenderingHintData();

        item.Distance = frontToBack
            ? NearestBoundsDistance(ordering.ViewPosition, bounds)
            : CentreDistance(ordering.ViewPosition, bounds);
    }

    // Bucketing replaces each item's distance with the distance of the bucket it falls in, so the
    // sort below orders by bucket and then, within one, by what draws the item - which is what
    // keeps a kit of repeated models batched. Done here rather than in the comparator because a
    // comparator that quantizes has to do it twice per comparison and must stay transitive.
    if (ordering.DepthBuckets > 0 && !m_Items.empty())
    {
        auto nearest = m_Items.front().Distance;
        auto furthest = m_Items.front().Distance;

        for (const auto& item : m_Items)
        {
            nearest = std::min(nearest, item.Distance);
            furthest = std::max(furthest, item.Distance);
        }

        // Spread across what is actually on screen rather than a fixed world distance: the same
        // bucket count then means the same thing in a corridor and across a valley.
        const float span = furthest - nearest;

        if (span > 0.f)
        {
            const float bucketSize = span / static_cast<float>(ordering.DepthBuckets);

            for (auto& item : m_Items)
            {
                item.Distance = std::floor((item.Distance - nearest) / bucketSize) * bucketSize + nearest;
            }
        }
    }

    std::sort(m_Items.begin(), m_Items.end(), [frontToBack](const DrawItem& left, const DrawItem& right)
    {
        if (left.Distance != right.Distance)
        {
            return frontToBack
                ? left.Distance < right.Distance
                : left.Distance > right.Distance;
        }

        // Items the sort cannot separate - the same bucket, or genuinely the same distance - are
        // ordered by what draws them, so copies of one model stay next to each other and still
        // submit as a single draw. It also makes the result of an unstable sort the same from one
        // frame to the next, and an order that shuffles between frames is what makes blended
        // geometry flicker.
        if (left.MeshPtr != right.MeshPtr)
        {
            return left.MeshPtr < right.MeshPtr;
        }

        return left.MaterialPtr < right.MaterialPtr;
    });
}

const std::vector<Pine::Rendering::DrawItem>& Pine::Rendering::DrawList::GetItems() const
{
    return m_Items;
}

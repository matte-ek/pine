#include "RenderCulling.hpp"

#include "Pine/Engine/Engine.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace
{
    constexpr std::size_t BitsPerWord = 64;

    // Past its model's cull distance, as the scene processor decided for this frame. Not drawn for
    // any view, so it counts as culled in every one of them.
    bool IsHiddenByDistance(const Pine::Renderer3D::ModelRendererHintData& data)
    {
        return data.LodModel == nullptr;
    }

    bool IsCandidate(const Pine::ModelRenderer& modelRenderer, const Pine::Rendering::RenderCulling::Candidates candidates)
    {
        if (candidates == Pine::Rendering::RenderCulling::Candidates::ShadowCasters)
        {
            return modelRenderer.GetCastShadows();
        }

        return true;
    }
}

void Pine::Rendering::RenderCulling::VisibilitySet::Reset(const std::size_t capacity)
{
    const std::size_t wordCount = (capacity + BitsPerWord - 1) / BitsPerWord;

    // Keeps the allocation across frames; only the first call (or a grow) actually allocates.
    if (m_Bits.size() < wordCount)
    {
        m_Bits.resize(wordCount);
    }

    std::fill(m_Bits.begin(), m_Bits.end(), 0ull);
}

void Pine::Rendering::RenderCulling::VisibilitySet::Set(const std::uint32_t index)
{
    const std::size_t word = index / BitsPerWord;

    if (word >= m_Bits.size())
    {
        return;
    }

    m_Bits[word] |= 1ull << (index % BitsPerWord);
}

bool Pine::Rendering::RenderCulling::VisibilitySet::IsVisible(const std::uint32_t index) const
{
    const std::size_t word = index / BitsPerWord;

    if (word >= m_Bits.size())
    {
        return false;
    }

    return (m_Bits[word] & (1ull << (index % BitsPerWord))) != 0;
}

Pine::Rendering::RenderCulling::CullingResult Pine::Rendering::RenderCulling::Cull(
    const Frustum& frustum,
    VisibilitySet& visibility,
    const Candidates candidates,
    const VisibilitySet* restrictTo)
{
    PINE_PF_SCOPE();

    visibility.Reset(Engine::GetEngineConfiguration().m_MaxObjectCount);

    CullingResult result;

    for (auto& modelRenderer : Components::Get<ModelRenderer>())
    {
        if (!modelRenderer.GetModel() || !IsCandidate(modelRenderer, candidates))
        {
            continue;
        }

        if (restrictTo != nullptr && !restrictTo->IsVisible(modelRenderer.GetInternalId()))
        {
            continue;
        }

        const auto& data = modelRenderer.GetRenderingHintData();

        if (IsHiddenByDistance(data))
        {
            result.CulledObjectCount++;
            continue;
        }

        if (frustum.Intersects(data.BoundsMin, data.BoundsMax))
        {
            visibility.Set(modelRenderer.GetInternalId());

            result.VisibleObjectCount++;
        }
        else
        {
            result.CulledObjectCount++;
        }
    }

    return result;
}

Pine::Rendering::RenderCulling::CullingResult Pine::Rendering::RenderCulling::Cull(
    const Vector3f& center,
    const float radius,
    VisibilitySet& visibility,
    const Candidates candidates)
{
    PINE_PF_SCOPE();

    visibility.Reset(Engine::GetEngineConfiguration().m_MaxObjectCount);

    CullingResult result;

    const float radiusSqr = radius * radius;

    for (auto& modelRenderer : Components::Get<ModelRenderer>())
    {
        if (!modelRenderer.GetModel() || !IsCandidate(modelRenderer, candidates))
        {
            continue;
        }

        const auto& data = modelRenderer.GetRenderingHintData();

        if (IsHiddenByDistance(data))
        {
            result.CulledObjectCount++;
            continue;
        }

        // Closest point on the box to the sphere centre. Clamping the centre into the box gives it
        // directly, with no case analysis over faces, edges and corners.
        const auto closest = glm::clamp(center, data.BoundsMin, data.BoundsMax);

        if (glm::distance2(closest, center) <= radiusSqr)
        {
            visibility.Set(modelRenderer.GetInternalId());

            result.VisibleObjectCount++;
        }
        else
        {
            result.CulledObjectCount++;
        }
    }

    return result;
}

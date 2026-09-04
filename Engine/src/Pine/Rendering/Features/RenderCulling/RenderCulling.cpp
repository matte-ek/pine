#include "RenderCulling.hpp"

#include "Pine/Engine/Engine.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace
{
    constexpr std::size_t BitsPerWord = 64;
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
    VisibilitySet& visibility)
{
    PINE_PF_SCOPE();

    visibility.Reset(Engine::GetEngineConfiguration().m_MaxObjectCount);

    CullingResult result;

    for (auto& modelRenderer : Components::Get<ModelRenderer>())
    {
        if (!modelRenderer.GetModel())
        {
            continue;
        }

        const auto& data = modelRenderer.GetRenderingHintData();

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

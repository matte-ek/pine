#include "InternalResolution.hpp"

#include <algorithm>
#include <vector>

#include "Pine/Core/Log/Log.hpp"

using namespace Pine;

namespace
{
    Vector2i m_Resolution = Vector2i(1280, 720);

    std::vector<std::function<void()>> m_ResizeCallbacks;

    // Growth is rounded up to a multiple of this, so dragging a window edge or a viewport splitter
    // rebuilds the scene buffers a handful of times instead of on every pixel crossed. The price is
    // up to this many unused pixels per axis.
    constexpr int GROWTH_STEP = 128;

    int RoundUpToGrowthStep(const int value)
    {
        return ((value + GROWTH_STEP - 1) / GROWTH_STEP) * GROWTH_STEP;
    }
}

Vector2i Rendering::InternalResolution::Get()
{
    return m_Resolution;
}

void Rendering::InternalResolution::AddResizeCallback(const std::function<void()>& callback)
{
    m_ResizeCallbacks.push_back(callback);
}

void Rendering::InternalResolution::Internal::Setup(const Vector2i resolution)
{
    m_Resolution = Vector2i(std::max(1, resolution.x), std::max(1, resolution.y));
}

void Rendering::InternalResolution::Internal::GrowTo(const Vector2i size)
{
    if (size.x <= m_Resolution.x && size.y <= m_Resolution.y)
    {
        return;
    }

    // Both axes share one allocation, so an axis that already fits keeps what it has rather than
    // being rounded up alongside the one that grew.
    m_Resolution = Vector2i(
        std::max(m_Resolution.x, RoundUpToGrowthStep(size.x)),
        std::max(m_Resolution.y, RoundUpToGrowthStep(size.y)));

    PInfo(fmt::format("Internal render resolution grew to {}x{}", m_Resolution.x, m_Resolution.y));

    for (const auto& callback : m_ResizeCallbacks)
    {
        callback();
    }
}

#pragma once

#include <cstdint>
#include <vector>

#include "Pine/Core/Math/Frustum/Frustum.hpp"

namespace Pine::Rendering::RenderCulling
{
    // Which objects passed culling for ONE frustum.
    //
    // Visibility is a property of (object, frustum), not of the object, so it is stored here rather
    // than on the component - that is what lets several frustums be live at once (the scene camera,
    // each shadow cascade, each shadow-casting light, a second viewport). Whoever owns the frustum
    // owns the matching set.
    //
    // Indexed by Component::GetInternalId(), the component's slot in its pool. That id is already
    // stable and unique per component type and is bounded by EngineConfiguration::m_MaxObjectCount,
    // so a set is m_MaxObjectCount bits - 512 bytes at the default 4096 - and needs no per-frame
    // index bookkeeping of its own.
    class VisibilitySet
    {
        std::vector<std::uint64_t> m_Bits;
    public:
        // Clears every bit, growing to hold 'capacity' objects. Cheap to call per frame.
        void Reset(std::size_t capacity);

        void Set(std::uint32_t index);
        bool IsVisible(std::uint32_t index) const;
    };

    struct CullingResult
    {
        int VisibleObjectCount = 0;
        int CulledObjectCount = 0;
    };

    // Tests every ModelRenderer against the frustum and records the survivors in 'visibility'.
    //
    // Takes a frustum and fills a set, deliberately knowing nothing about cameras, lights or render
    // batches: a shadow pass calls this exactly as the scene camera does, with its own frustum and
    // its own set.
    CullingResult Cull(const Frustum& frustum, VisibilitySet& visibility);
}

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
    // so a set is m_MaxObjectCount bits - 4 KB at the default 32768 - and needs no per-frame
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

    // Which renderers a cull tests. A shadow view draws only the renderers that cast shadows, so
    // it culls only those, and its set then holds exactly what the view draws.
    //
    // Renderers left out are not counted as culled, for the same reason as 'restrictTo' below.
    enum class Candidates
    {
        AllRenderers,
        ShadowCasters
    };

    // Tests every ModelRenderer against the frustum and records the survivors in 'visibility'.
    //
    // Takes a frustum and fills a set, deliberately knowing nothing about cameras, lights or render
    // batches: a shadow pass calls this exactly as the scene camera does, with its own frustum and
    // its own set.
    //
    // 'restrictTo', when given, limits the test to objects already in that set. It reads as "cull a
    // subset", not "cull for shadows" - a point light culls once against its sphere of influence and
    // restricts each of its six face culls to what that found, and any caller with a cheaper
    // superset test can do the same. Objects excluded by it are not counted as culled: they were
    // never candidates, and counting them would make the two numbers mean different things
    // depending on whether a restriction was passed.
    CullingResult Cull(const Frustum& frustum,
                       VisibilitySet& visibility,
                       Candidates candidates = Candidates::AllRenderers,
                       const VisibilitySet* restrictTo = nullptr);

    // The same, against a sphere.
    //
    // Not a special case of the frustum test: a sphere is the natural bound for anything that
    // radiates rather than projects, and testing one is a fraction of the cost of six planes.
    CullingResult Cull(const Vector3f& center,
                       float radius,
                       VisibilitySet& visibility,
                       Candidates candidates = Candidates::AllRenderers);
}

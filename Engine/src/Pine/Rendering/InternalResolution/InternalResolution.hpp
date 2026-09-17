#pragma once
#include <functional>

#include "Pine/Core/Math/Math.hpp"

namespace Pine::Rendering::InternalResolution
{

    // The size the engine's shared scene buffers are allocated at: the HDR scene target, the depth
    // pre-pass target, and - divided down - the ambient occlusion and bloom buffers.
    //
    // A rendering context does not own a scene buffer. It renders into a Size-sized corner of these
    // shared ones, and every pass that reads the scene back scales its texture coordinates by
    // Size / Get() to compensate. That only holds while a context fits inside the allocation, which
    // is what GrowTo() is for - without it an oversized context samples past the edge of the scene
    // texture and the frame comes out tiled.
    Vector2i Get();

    // Registered by the features that allocate from Get(), to rebuild their buffers once it has
    // grown. Callbacks run in registration order, from inside GrowTo().
    void AddResizeCallback(const std::function<void()>& callback);

    namespace Internal
    {
        // Sets the starting resolution. Call before anything has allocated from it; no callbacks
        // are fired.
        void Setup(Vector2i resolution);

        // Makes sure the allocation can hold a context of the given size, rebuilding every
        // registered buffer if it cannot.
        //
        // Only ever grows. The allocation is a high-water mark for the session: going back to a
        // smaller window would otherwise pay for a rebuild that the next resize just undoes, and
        // the memory an oversized buffer holds is cheaper than that churn.
        void GrowTo(Vector2i size);
    }

}

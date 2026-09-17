#pragma once
#include <string>
#include <vector>

#include "Pine/Performance/ScopedTimer/ScopedTimer.hpp"

namespace Pine
{

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#define PINE_PF_SCOPE() static Pine::Performance::TrackedScope* CONCAT(scope, __LINE__) = Pine::Performance::CreateTrackedScopeFromSignature(__PRETTY_FUNCTION__); \
    Pine::ScopedTimer CONCAT(timer, __LINE__)(CONCAT(scope, __LINE__))

#define PINE_PF_SCOPE_MANUAL(x) static Pine::Performance::TrackedScope* CONCAT(scope, __LINE__) = Pine::Performance::CreateTrackedScope(x); \
Pine::ScopedTimer CONCAT(timer, __LINE__)(CONCAT(scope, __LINE__))

}

namespace Pine::Performance
{
    // A scope is created once, the first time its PINE_PF_SCOPE() is reached, and then keeps
    // collecting timings for as long as the engine runs.
    //
    // Everything below is measured per frame rather than per call: a scope that runs once for every
    // rendering context reports what it cost the frame, not what its last invocation happened to
    // cost. The timing assumes a scope is never entered from two threads at once, which holds for
    // every scope the engine currently has - they all run on the main thread.
    struct TrackedScope
    {
        // The name the scope was created with. For PINE_PF_SCOPE() that is the full function
        // signature, and it is what FindTrackedScopeByName() matches against.
        const char* Name = nullptr;

        // 'Name' cut down to something worth putting in a list, e.g. "Shadows::RenderAllViews".
        std::string ShortName;

        // The scope that was running when this one was last entered, or nullptr when it runs at the
        // top level. This is what gives the profiler its tree. A scope called from more than one
        // place reports the caller it saw most recently, and a scope that has not run for a while
        // keeps the last caller it did run under.
        TrackedScope* Parent = nullptr;

        // The last completed frame: every invocation of that frame summed, and how many there were.
        double Time = 0.0;
        int CallCount = 0;

        // 'Time' smoothed across frames. Raw per-frame timings jitter far too much to read while
        // anything in the scene is moving, so this is the number the profiler lists.
        double SmoothedTime = 0.0;

        // The frame in progress. EndFrame() folds these into the fields above and clears them.
        double PendingTime = 0.0;
        int PendingCallCount = 0;
        TrackedScope* PendingParent = nullptr;
    };

    // Creates a scope named exactly as given, minus a leading "Pine::". Use this when the name is
    // written by hand and already reads well, such as "Pipeline3D::Run(PipelineStage::Prepass)".
    TrackedScope* CreateTrackedScope(const char* name);

    // Creates a scope from a full function signature (__PRETTY_FUNCTION__), which gets reduced to a
    // display name of the form "Class::Function".
    TrackedScope* CreateTrackedScopeFromSignature(const char* signature);

    TrackedScope* FindTrackedScopeByName(const char* name);

    const std::vector<TrackedScope*>& GetTrackedScopes();

    // Closes off the frame: the sums collected during it become that frame's totals, the smoothed
    // times move, and the next frame starts from zero. Called once per frame by Engine::Run().
    void EndFrame();

    namespace Internal
    {
        // Called by ScopedTimer around the scope it times.
        void EnterScope(TrackedScope* scope);
        void ExitScope(TrackedScope* scope, double elapsedTime);
    }
}

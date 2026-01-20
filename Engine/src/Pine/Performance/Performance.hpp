#pragma once
#include <vector>

#include "Pine/Performance/ScopedTimer/ScopedTimer.hpp"

namespace Pine
{

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#define PINE_PF_SCOPE() static Pine::Performance::TrackedScope* CONCAT(scope, __LINE__) = Pine::Performance::CreateTrackedScope(__FUNCTION__); \
    Pine::ScopedTimer CONCAT(timer, __LINE__)(CONCAT(scope, __LINE__))

#define PINE_PF_SCOPE_MANUAL(x) static Pine::Performance::TrackedScope* CONCAT(scope, __LINE__) = Pine::Performance::CreateTrackedScope(x); \
Pine::ScopedTimer CONCAT(timer, __LINE__)(CONCAT(scope, __LINE__))

}

namespace Pine::Performance
{
    struct TrackedScope
    {
        const char* Name;
        double Time;
    };

    TrackedScope* CreateTrackedScope(const char* name);

    const std::vector<TrackedScope*>& GetTrackedScopes();
}
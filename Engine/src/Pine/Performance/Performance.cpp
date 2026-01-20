#include "Performance.hpp"

namespace
{
    std::vector<Pine::Performance::TrackedScope*> m_TrackedScopes;
}

Pine::Performance::TrackedScope* Pine::Performance::CreateTrackedScope(const char* name)
{
    auto trackedScope = new TrackedScope();

    trackedScope->Name = name;

    m_TrackedScopes.push_back(trackedScope);

    return trackedScope;
}

const std::vector<Pine::Performance::TrackedScope*>& Pine::Performance::GetTrackedScopes()
{
    return m_TrackedScopes;
}

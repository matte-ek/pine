#include "Performance.hpp"

#include <cstring>

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

Pine::Performance::TrackedScope* Pine::Performance::FindTrackedScopeByName(const char* name)
{
    for (const auto& scope : m_TrackedScopes)
    {
        if (strcmp(scope->Name, name) == 0)
        {
            return scope;
        }
    }

    return nullptr;
}

const std::vector<Pine::Performance::TrackedScope*>& Pine::Performance::GetTrackedScopes()
{
    return m_TrackedScopes;
}

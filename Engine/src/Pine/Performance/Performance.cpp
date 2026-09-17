#include "Performance.hpp"

#include <cstring>

namespace
{
    std::vector<Pine::Performance::TrackedScope*> m_TrackedScopes;

    // The scopes we are currently inside of, innermost last. Whatever sits on top of this when a
    // scope is entered is that scope's parent. Thread local so that a scope timed on a worker
    // thread cannot end up parented to whatever the main thread happened to be doing at the time.
    thread_local std::vector<Pine::Performance::TrackedScope*> m_ActiveScopes;

    // How much of a new frame's timing is taken into a scope's smoothed time. At 60 FPS this
    // settles within a few tenths of a second: slow enough to read, fast enough that a change you
    // just made still shows up while you are looking at it.
    constexpr double SMOOTHING_FACTOR = 0.1;

    void RemoveAll(std::string& text, const std::string& pattern)
    {
        for (auto position = text.find(pattern); position != std::string::npos; position = text.find(pattern))
        {
            text.erase(position, pattern.size());
        }
    }

    // "Pine::Rendering::Shadows::RenderView" -> "Shadows::RenderView". Every scope in the engine
    // shares the outer namespaces, so they cost width without telling any two rows apart; the class
    // or namespace directly around the function is the part that does.
    std::string KeepLastNameParts(const std::string& qualifiedName, const int partCount)
    {
        std::size_t partStart = 0;
        std::size_t searchEnd = qualifiedName.size();

        for (int i = 0; i < partCount; i++)
        {
            if (searchEnd == 0)
            {
                break;
            }

            const auto separator = qualifiedName.rfind("::", searchEnd - 1);

            if (separator == std::string::npos)
            {
                // Fewer parts than asked for, so the name is already as short as it gets.
                return qualifiedName;
            }

            partStart = separator + 2;
            searchEnd = separator;
        }

        return qualifiedName.substr(partStart);
    }

    // __PRETTY_FUNCTION__ hands us a full signature, e.g.
    //   "void Pine::Rendering::Shadows::{anonymous}::RenderView(Pine::RenderingContext&, float)"
    // which is unreadable in a list, and identical to its neighbours for the first forty characters
    // at that. The full signature stays on the scope for the profiler to show on hover.
    std::string CreateShortNameFromSignature(const char* signature)
    {
        std::string text = signature;

        // Before anything goes looking for the argument list, since the Clang spelling of an
        // anonymous namespace brings parentheses of its own.
        RemoveAll(text, "(anonymous namespace)::");
        RemoveAll(text, "{anonymous}::");

        const auto argumentStart = text.find('(');

        if (argumentStart != std::string::npos)
        {
            text.erase(argumentStart);
        }

        // Whatever is left in front of the last space is the return type.
        const auto returnTypeEnd = text.rfind(' ');

        if (returnTypeEnd != std::string::npos)
        {
            text.erase(0, returnTypeEnd + 1);
        }

        return KeepLastNameParts(text, 2);
    }

    Pine::Performance::TrackedScope* CreateScope(const char* name, std::string shortName)
    {
        auto trackedScope = new Pine::Performance::TrackedScope();

        trackedScope->Name = name;
        trackedScope->ShortName = std::move(shortName);

        m_TrackedScopes.push_back(trackedScope);

        return trackedScope;
    }
}

Pine::Performance::TrackedScope* Pine::Performance::CreateTrackedScope(const char* name)
{
    static const std::string enginePrefix = "Pine::";
    static const std::string emptyArgumentList = "()";

    std::string shortName = name;

    if (shortName.rfind(enginePrefix, 0) == 0)
    {
        shortName.erase(0, enginePrefix.size());
    }

    // An empty argument list says nothing, and dropping it lines these names up with the ones taken
    // from a signature. An argument that is actually there stays: it is usually the whole point of
    // the manual name, as in "Pipeline3D::Run(PipelineStage::Prepass)".
    if (shortName.size() > emptyArgumentList.size() &&
        shortName.compare(shortName.size() - emptyArgumentList.size(), emptyArgumentList.size(), emptyArgumentList) == 0)
    {
        shortName.erase(shortName.size() - emptyArgumentList.size());
    }

    return CreateScope(name, std::move(shortName));
}

Pine::Performance::TrackedScope* Pine::Performance::CreateTrackedScopeFromSignature(const char* signature)
{
    return CreateScope(signature, CreateShortNameFromSignature(signature));
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

void Pine::Performance::EndFrame()
{
    for (const auto scope : m_TrackedScopes)
    {
        scope->Time = scope->PendingTime;
        scope->CallCount = scope->PendingCallCount;

        // A scope that did not run this frame keeps the parent it last ran under, so that it stays
        // where the reader last saw it in the tree instead of jumping to the top level.
        if (scope->PendingCallCount > 0)
        {
            scope->Parent = scope->PendingParent;
        }

        scope->SmoothedTime += (scope->Time - scope->SmoothedTime) * SMOOTHING_FACTOR;

        scope->PendingTime = 0.0;
        scope->PendingCallCount = 0;
    }
}

void Pine::Performance::Internal::EnterScope(TrackedScope* scope)
{
    TrackedScope* parent = m_ActiveScopes.empty() ? nullptr : m_ActiveScopes.back();

    // A recursive scope keeps the parent it first entered with. Pointing it at itself would turn
    // the profiler's tree into a cycle.
    if (parent != scope)
    {
        scope->PendingParent = parent;
    }

    m_ActiveScopes.push_back(scope);
}

void Pine::Performance::Internal::ExitScope(TrackedScope* scope, double elapsedTime)
{
    scope->PendingTime += elapsedTime;
    scope->PendingCallCount++;

    if (!m_ActiveScopes.empty())
    {
        m_ActiveScopes.pop_back();
    }
}

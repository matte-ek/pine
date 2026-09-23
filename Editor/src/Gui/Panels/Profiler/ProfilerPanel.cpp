#include "ProfilerPanel.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include "imgui.h"
#include "IconsMaterialDesign.h"

#include "Gui/Gui.hpp"
#include "Rendering/RenderHandler.hpp"

#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    using Pine::Performance::TrackedScope;

    // The columns of the scope table, in the order they are set up. Sorting reads these back from
    // ImGui, so the two have to agree.
    enum ScopeColumn
    {
        ScopeColumn_Name,
        ScopeColumn_Time,
        ScopeColumn_FrameShare,
        ScopeColumn_CallCount
    };

    bool m_Active = true;

    // Frame times in milliseconds, kept as a ring buffer for the graph. Two seconds of history at
    // 60 FPS, which is long enough to catch a hitch scrolling past without turning the graph into
    // noise.
    constexpr int FRAME_HISTORY_SIZE = 120;

    std::array<float, FRAME_HISTORY_SIZE> m_FrameTimes = {};
    int m_FrameTimeCursor = 0;

    char m_ScopeFilter[64] = {};
    bool m_ShowIdleScopes = false;

    // The scope tree as it is drawn, rebuilt every frame because a scope's parent is whatever
    // called it last. Only scopes that pass the filter are in here.
    std::vector<TrackedScope*> m_RootScopes;
    std::unordered_map<const TrackedScope*, std::vector<TrackedScope*>> m_ChildScopes;

    // Total time spent inside the render manager, which is nearly the whole frame in the editor.
    // Looked up once, by the name RenderManager::Run() passes to PINE_PF_SCOPE_MANUAL.
    TrackedScope* m_RenderManagerScope = nullptr;
    bool m_HasLookedUpScopes = false;

    double ToMilliseconds(const double seconds)
    {
        return seconds * 1000.0;
    }

    std::string FormatCount(const std::uint64_t count)
    {
        if (count >= 1000000)
        {
            return fmt::format("{:.2f}M", static_cast<double>(count) / 1000000.0);
        }

        if (count >= 1000)
        {
            return fmt::format("{:.1f}K", static_cast<double>(count) / 1000.0);
        }

        return fmt::format("{}", count);
    }

    // Numbers are far easier to compare down a column when their digits line up, and ImGui has no
    // column alignment of its own.
    void TextRightAligned(const std::string& text)
    {
        // Short of the cell's edge by its own padding, so that the last column's numbers do not end
        // up pressed against the table border and the scroll bar behind it.
        const float available = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().CellPadding.x;
        const float textWidth = ImGui::CalcTextSize(text.c_str()).x;

        if (textWidth < available)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - textWidth);
        }

        ImGui::TextUnformatted(text.c_str());
    }

    bool ContainsIgnoreCase(const std::string& text, const std::string& pattern)
    {
        const auto match = std::search(text.begin(), text.end(), pattern.begin(), pattern.end(),
            [](const char a, const char b)
            {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });

        return match != text.end();
    }

    void RecordFrameTime(const double deltaTime)
    {
        m_FrameTimes[m_FrameTimeCursor] = static_cast<float>(ToMilliseconds(deltaTime));
        m_FrameTimeCursor = (m_FrameTimeCursor + 1) % FRAME_HISTORY_SIZE;
    }

    float GetAverageFrameTime()
    {
        float total = 0.f;

        for (const float frameTime : m_FrameTimes)
        {
            total += frameTime;
        }

        return total / static_cast<float>(FRAME_HISTORY_SIZE);
    }

    float GetWorstFrameTime()
    {
        return *std::max_element(m_FrameTimes.begin(), m_FrameTimes.end());
    }

    void RenderFrameSummary()
    {
        RecordFrameTime(Pine::RenderManager::GetGlobalDeltaTime());

        const float averageFrameTime = GetAverageFrameTime();
        const float worstFrameTime = GetWorstFrameTime();

        ImGui::PushFont(Editor::Gui::GetBoldFont());
        ImGui::Text("%.2f ms", averageFrameTime);
        ImGui::PopFont();

        ImGui::SameLine();
        ImGui::TextDisabled("(%d FPS)", averageFrameTime > 0.f ? static_cast<int>(1000.f / averageFrameTime) : 0);

        // On its own line rather than appended to the one above, since the summary sits in a narrow
        // column whenever the panel is wide enough to split.
        if (m_RenderManagerScope != nullptr)
        {
            ImGui::TextDisabled("%.2f ms rendering", ToMilliseconds(m_RenderManagerScope->SmoothedTime));
            ImGui::SameLine();
        }

        ImGui::TextDisabled("worst %.2f ms", worstFrameTime);

        // The graph keeps the 60 FPS budget on screen even when the frame is nowhere near it, so
        // that the height of a spike means the same thing from one look to the next. It is drawn
        // without an overlay label, which would otherwise sit right on top of the line.
        constexpr float FRAME_BUDGET_MS = 16.7f;

        const float graphScale = std::max(worstFrameTime * 1.2f, FRAME_BUDGET_MS);

        ImGui::PlotLines("##FrameTimes", m_FrameTimes.data(), FRAME_HISTORY_SIZE, m_FrameTimeCursor,
            nullptr, 0.f, graphScale, ImVec2(-1.f, 70.f));
    }

    // Both contexts are listed because they are separate frame costs: the editor renders the level
    // viewport and the game viewport every frame, each with its own culling. They are columns
    // rather than rows so that the two are read against each other, and so the table stays narrow.
    void RenderViewportStatistics()
    {
        struct ViewportColumn
        {
            const char* Name;
            const Pine::RenderingContext* Context;
        };

        const std::array<ViewportColumn, 2> viewports = {{
            { "Level", Editor::RenderHandler::GetLevelRenderingContext() },
            { "Game", Editor::RenderHandler::GetGameRenderingContext() }
        }};

        constexpr auto tableFlags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingStretchProp;

        if (!ImGui::BeginTable("##ViewportStatistics", 1 + static_cast<int>(viewports.size()), tableFlags))
        {
            return;
        }

        ImGui::TableSetupColumn("Statistic");

        for (const auto& viewport : viewports)
        {
            ImGui::TableSetupColumn(viewport.Name);
        }

        ImGui::TableHeadersRow();

        const auto renderStatisticRow = [&viewports](const char* label, const auto& formatValue)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);

            for (std::size_t i = 0; i < viewports.size(); i++)
            {
                ImGui::TableSetColumnIndex(static_cast<int>(i) + 1);

                const auto* context = viewports[i].Context;

                if (context == nullptr || !context->Active)
                {
                    ImGui::TextDisabled("-");
                    continue;
                }

                ImGui::TextUnformatted(formatValue(context->Statistics).c_str());
            }
        };

        renderStatisticRow("Draw calls", [](const Pine::RenderingStatistics& statistics)
        {
            return fmt::format("{}", statistics.DrawCalls);
        });

        renderStatisticRow("Vertices", [](const Pine::RenderingStatistics& statistics)
        {
            return FormatCount(statistics.VertexCount);
        });

        renderStatisticRow("Objects", [](const Pine::RenderingStatistics& statistics)
        {
            return fmt::format("{} / {}", statistics.VisibleObjectCount,
                statistics.VisibleObjectCount + statistics.CulledObjectCount);
        });

        renderStatisticRow("Terrain chunks", [](const Pine::RenderingStatistics& statistics)
        {
            return fmt::format("{} / {}", statistics.VisibleTerrainChunkCount,
                statistics.VisibleTerrainChunkCount + statistics.CulledTerrainChunkCount);
        });

        renderStatisticRow("Terrain detail", [](const Pine::RenderingStatistics& statistics)
        {
            return FormatCount(statistics.TerrainDetailInstanceCount);
        });

        renderStatisticRow("Lights", [](const Pine::RenderingStatistics& statistics)
        {
            return fmt::format("{}", statistics.LightCount);
        });

        renderStatisticRow("Render time", [](const Pine::RenderingStatistics& statistics)
        {
            return fmt::format("{:.2f} ms", ToMilliseconds(statistics.RenderTime));
        });

        ImGui::EndTable();

        ImGui::TextDisabled("Visible / total objects and terrain chunks.");
    }

    bool MatchesFilter(const TrackedScope* scope)
    {
        if (m_ScopeFilter[0] == '\0')
        {
            return true;
        }

        return ContainsIgnoreCase(scope->ShortName, m_ScopeFilter);
    }

    bool IsScopeOrderedBefore(const TrackedScope* first, const TrackedScope* second, const ImGuiTableSortSpecs* sortSpecs)
    {
        if (sortSpecs == nullptr || sortSpecs->SpecsCount == 0)
        {
            return first->SmoothedTime > second->SmoothedTime;
        }

        const auto& sortSpec = sortSpecs->Specs[0];
        const bool ascending = sortSpec.SortDirection == ImGuiSortDirection_Ascending;

        switch (sortSpec.ColumnIndex)
        {
        case ScopeColumn_Name:
            return ascending ? first->ShortName < second->ShortName : second->ShortName < first->ShortName;
        case ScopeColumn_CallCount:
            return ascending ? first->CallCount < second->CallCount : second->CallCount < first->CallCount;
        default:
            // Time and frame share are the same number, one of them divided by the frame.
            return ascending ? first->SmoothedTime < second->SmoothedTime : second->SmoothedTime < first->SmoothedTime;
        }
    }

    void BuildScopeTree(const ImGuiTableSortSpecs* sortSpecs)
    {
        m_RootScopes.clear();
        m_ChildScopes.clear();

        const bool isFiltering = m_ScopeFilter[0] != '\0';

        std::unordered_set<const TrackedScope*> visibleScopes;

        for (const auto scope : Pine::Performance::GetTrackedScopes())
        {
            if (!m_ShowIdleScopes && scope->CallCount == 0)
            {
                continue;
            }

            if (!MatchesFilter(scope))
            {
                continue;
            }

            visibleScopes.insert(scope);
        }

        for (const auto scope : Pine::Performance::GetTrackedScopes())
        {
            if (visibleScopes.count(scope) == 0)
            {
                continue;
            }

            // A filtered list is deliberately flat: a search for "shadow" should show every scope
            // that matched, not hide half of them inside a parent that did not.
            const bool hasVisibleParent = !isFiltering &&
                scope->Parent != nullptr &&
                visibleScopes.count(scope->Parent) > 0;

            if (hasVisibleParent)
            {
                m_ChildScopes[scope->Parent].push_back(scope);
            }
            else
            {
                m_RootScopes.push_back(scope);
            }
        }

        const auto compare = [sortSpecs](const TrackedScope* first, const TrackedScope* second)
        {
            return IsScopeOrderedBefore(first, second, sortSpecs);
        };

        std::sort(m_RootScopes.begin(), m_RootScopes.end(), compare);

        for (auto& [parent, children] : m_ChildScopes)
        {
            std::sort(children.begin(), children.end(), compare);
        }
    }

    void RenderScopeTooltip(const TrackedScope* scope)
    {
        ImGui::BeginTooltip();

        ImGui::TextUnformatted(scope->Name);
        ImGui::Separator();
        ImGui::Text("Last frame: %.3f ms over %d call(s)", ToMilliseconds(scope->Time), scope->CallCount);
        ImGui::Text("Smoothed: %.3f ms", ToMilliseconds(scope->SmoothedTime));

        ImGui::EndTooltip();
    }

    void RenderScopeRow(TrackedScope* scope, const double frameTime)
    {
        const auto childEntry = m_ChildScopes.find(scope);
        const bool hasChildren = childEntry != m_ChildScopes.end();

        const bool isIdle = scope->CallCount == 0;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(ScopeColumn_Name);

        ImGuiTreeNodeFlags treeFlags =
            ImGuiTreeNodeFlags_SpanFullWidth |
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_DefaultOpen;

        if (!hasChildren)
        {
            treeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        }

        if (isIdle)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        }

        const bool isOpen = ImGui::TreeNodeEx(scope, treeFlags, "%s", scope->ShortName.c_str());

        if (isIdle)
        {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemHovered())
        {
            RenderScopeTooltip(scope);
        }

        ImGui::TableSetColumnIndex(ScopeColumn_Time);
        TextRightAligned(fmt::format("{:.3f}", ToMilliseconds(scope->SmoothedTime)));

        ImGui::TableSetColumnIndex(ScopeColumn_FrameShare);

        // The bar is what makes the table readable at a glance: the handful of scopes the frame is
        // actually spent in stand out by length, without anyone reading four digits per row.
        const double frameShare = frameTime > 0.0 ? scope->SmoothedTime / frameTime : 0.0;
        const auto frameShareText = fmt::format("{:.1f}%", frameShare * 100.0);

        ImGui::ProgressBar(static_cast<float>(std::min(frameShare, 1.0)),
            ImVec2(-1.f, ImGui::GetTextLineHeight()), frameShareText.c_str());

        ImGui::TableSetColumnIndex(ScopeColumn_CallCount);
        TextRightAligned(fmt::format("{}", scope->CallCount));

        if (isOpen)
        {
            if (hasChildren)
            {
                for (const auto child : childEntry->second)
                {
                    RenderScopeRow(child, frameTime);
                }
            }

            ImGui::TreePop();
        }
    }

    void RenderScopeControls()
    {
        ImGui::SetNextItemWidth(220.f);
        ImGui::InputTextWithHint("##ScopeFilter", ICON_MD_SEARCH "  Filter", m_ScopeFilter, sizeof(m_ScopeFilter));

        ImGui::SameLine();
        ImGui::Checkbox("Show idle scopes", &m_ShowIdleScopes);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Also list scopes that did not run during the last frame, such as\n"
                              "asset importing or terrain generation.");
        }
    }

    void RenderScopeTable()
    {
        constexpr auto tableFlags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Sortable;

        if (!ImGui::BeginTable("##ProfilerScopes", 4, tableFlags, ImVec2(-1.f, -1.f)))
        {
            return;
        }

        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed |
            ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 80.f);
        ImGui::TableSetupColumn("% of frame", ImGuiTableColumnFlags_WidthStretch |
            ImGuiTableColumnFlags_PreferSortDescending, 0.3f);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed |
            ImGuiTableColumnFlags_PreferSortDescending, 64.f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        BuildScopeTree(ImGui::TableGetSortSpecs());

        // Every scope's share is measured against the same frame, so that a child's percentage can
        // be read directly against its parent's.
        const double frameTime = Pine::RenderManager::GetGlobalDeltaTime();

        for (const auto scope : m_RootScopes)
        {
            RenderScopeRow(scope, frameTime);
        }

        ImGui::EndTable();
    }

    void RenderScopePane()
    {
        ImGui::SeparatorText("Scopes");

        RenderScopeControls();
        RenderScopeTable();
    }

    void RenderSummaryPane()
    {
        RenderFrameSummary();

        if (ImGui::CollapsingHeader("Viewports", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderViewportStatistics();
        }
    }

    void RenderPanes()
    {
        // The panel is normally docked wide and short, where stacking the two would leave the scope
        // tree a few rows tall with a graph stretched across two thousand pixels above it. Side by
        // side, the tree gets the height and the width it wants and the summary keeps the column it
        // needs. Docked narrow there is no width to split, so the two stack instead.
        constexpr float SIDE_BY_SIDE_MINIMUM_WIDTH = 900.f;

        // Wide enough for the viewport table's three columns, and no wider: every pixel past that
        // is worth more to the scope tree.
        constexpr float SUMMARY_PANE_MINIMUM_WIDTH = 340.f;
        constexpr float SUMMARY_PANE_MAXIMUM_WIDTH = 460.f;

        const float availableWidth = ImGui::GetContentRegionAvail().x;

        if (availableWidth < SIDE_BY_SIDE_MINIMUM_WIDTH)
        {
            RenderSummaryPane();
            RenderScopePane();

            return;
        }

        const float summaryPaneWidth = std::clamp(availableWidth * 0.3f,
            SUMMARY_PANE_MINIMUM_WIDTH, SUMMARY_PANE_MAXIMUM_WIDTH);

        const float scopePaneWidth = availableWidth - summaryPaneWidth - ImGui::GetStyle().ItemSpacing.x;

        ImGui::BeginChild("##ScopePane", ImVec2(scopePaneWidth, 0.f));
        RenderScopePane();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##SummaryPane", ImVec2(0.f, 0.f));
        RenderSummaryPane();
        ImGui::EndChild();
    }

}

void Panels::Profiler::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Profiler::GetActive()
{
    return m_Active;
}

void Panels::Profiler::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_SPEED "  Profiler", &m_Active))
    {
        if (!m_HasLookedUpScopes)
        {
            m_HasLookedUpScopes = true;
            m_RenderManagerScope = Pine::Performance::FindTrackedScopeByName("Pine::RenderManager::Run");
        }

        RenderPanes();
    }
    ImGui::End();
}

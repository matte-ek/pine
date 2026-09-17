#include "DebugPanel.hpp"

#include <cstdint>
#include <imgui.h>

#include "IconsMaterialDesign.h"

#include "Gui/Shared/Selection/Selection.hpp"
#include "Rendering/RenderHandler.hpp"

#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Features/Shadows/Shadows.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/GraphicsSettings/GraphicsSettings.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

namespace
{
    bool m_Active = true;

    bool m_AmbientOcclusionTexture = false;
    bool m_DepthPositionTexture = false;
    bool m_ShadowAtlasTexture = false;
}

void Panels::Debug::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Debug::GetActive()
{
    return m_Active;
}

void Panels::Debug::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_TROUBLESHOOT "  Debug", &m_Active))
    {
        if (ImGui::CollapsingHeader("Culling"))
        {
            // Both contexts are listed on purpose: visibility is per frustum, so a level viewport
            // and a game camera pointed different ways should disagree here. If the two rows move
            // in lockstep regardless of where each camera looks, culling has regressed to being
            // shared again.
            const auto renderContextStats = [](const char* name, const Pine::RenderingContext* context)
            {
                if (context == nullptr)
                {
                    return;
                }

                const auto& statistics = context->Statistics;
                const int total = statistics.VisibleObjectCount + statistics.CulledObjectCount;

                ImGui::Text("%s: %d visible, %d culled (of %d)",
                    name,
                    statistics.VisibleObjectCount,
                    statistics.CulledObjectCount,
                    total);
            };

            renderContextStats("Level viewport", Editor::RenderHandler::GetLevelRenderingContext());
            renderContextStats("Game viewport", Editor::RenderHandler::GetGameRenderingContext());
        }

        if (ImGui::CollapsingHeader("Lightning"))
        {

            const auto& selectedEntities = Selection::GetSelectedEntities();
            if (!selectedEntities.empty())
            {
                auto selectedEntity = selectedEntities.front();

                if (auto modelRenderer = selectedEntity->GetComponent<Pine::ModelRenderer>())
                {
                    auto& lightData = modelRenderer->GetRenderingHintData().Lights;

                    ImGui::Text("Has Computed Light Data: %d", lightData.HasComputedData);

                    namespace Slots = Pine::Renderer3D::Specifications::ObjectLightSlots;

                    // Point Lights
                    for (int i = 0; i < Slots::POINT_LIGHT_COUNT;i++)
                    {
                        if (auto light = lightData.Index[Slots::POINT_LIGHT_OFFSET + i].Get())
                        {
                            ImGui::Text("Point Light #%d: %s", i, light->GetParent()->GetName().c_str());
                        }
                        else
                        {
                            ImGui::Text("Point Light #%d: N/A", i);
                        }
                    }

                    // Spotlights
                    for (int i = 0; i < Slots::SPOT_LIGHT_COUNT;i++)
                    {
                        if (auto light = lightData.Index[Slots::SPOT_LIGHT_OFFSET + i].Get())
                        {
                            ImGui::Text("Spot Light #%d: %s", i, light->GetParent()->GetName().c_str());
                        }
                        else
                        {
                            ImGui::Text("Spot Light #%d: N/A", i);
                        }
                    }

                    if (ImGui::Button("Invalidate object lightning data"))
                    {
                        lightData.HasComputedData = false;
                    }
                }

                if (auto light = selectedEntity->GetComponent<Pine::Light>())
                {

                }
            }

            ImGui::Separator();

            ImGui::Text("Renderer3D");

            ImGui::Checkbox("View Ambient Occlusion", &m_AmbientOcclusionTexture);
            ImGui::Checkbox("View Position Texture", &m_DepthPositionTexture);
        }

        if (ImGui::CollapsingHeader("Shadows"))
        {
            const auto& statistics = Pine::Rendering::Shadows::GetStatistics();

            // Rendered and cached are both local and both sum to the view count, so the pair reads
            // as the cache hit rate it is meant to be. They used to be counted over different sets -
            // cached local-only, rendered whole-frame - which made them silently fail to add up as
            // soon as a second viewport opened.
            ImGui::Text("Local views: %d (%d rendered, %d cached), casters drawn: %d",
                statistics.LocalViewCount,
                statistics.TilesRendered,
                statistics.TilesCached,
                statistics.CastersDrawn);

            ImGui::Text("Cascade views: %d, casters drawn: %d",
                statistics.CascadeViewCount,
                statistics.CascadeCastersDrawn);

            // The whole frame's atlas cost across both kinds. Cascades always render, so their view
            // count is their tile count. Counted per rendering context, so two live viewports draw
            // two sets of cascades and this says so.
            ImGui::Text("Tiles rendered: %d", statistics.TilesRendered + statistics.CascadeViewCount);

            ImGui::Text("Atlas: %dx%d", Pine::Rendering::ShadowAtlas::GetResolution(),
                                        Pine::Rendering::ShadowAtlas::GetResolution());

            // Tile ownership, eviction churn, cube seams and stale caches are all invisible in the
            // final image and obvious here. Worth having before the things it diagnoses exist.
            const auto& slots = Pine::Rendering::ShadowAtlas::GetSlots();

            int occupied = 0;
            for (const auto& slot : slots)
            {
                if (slot.Owner != nullptr)
                {
                    occupied++;
                }
            }

            ImGui::Text("Slots: %d of %d in use", occupied, static_cast<int>(slots.size()));

            if (ImGui::TreeNode("Tiles"))
            {
                for (std::size_t i = 0; i < slots.size(); i++)
                {
                    const auto& slot = slots[i];

                    if (slot.Owner == nullptr)
                    {
                        continue;
                    }

                    static const char* sizeNames[] = { "1/2", "1/4", "1/8" };

                    const auto tile = Pine::Rendering::Shadows::GetTileDebugInfo(static_cast<int>(i));

                    // Slot::Owner is only a Light for tiles a light claimed. The cascades' tiles are
                    // pinned to a token, and casting that to a Light would dereference a char.
                    const char* ownerName = tile.Reserved
                        ? "directional cascade"
                        : static_cast<const Pine::Light*>(slot.Owner)->GetParent()->GetName().c_str();

                    ImGui::Text("#%d %s %dx%d @ (%d,%d) - %s%s",
                        static_cast<int>(i),
                        sizeNames[static_cast<int>(slot.Size)],
                        slot.Rect.z, slot.Rect.w, slot.Rect.x, slot.Rect.y,
                        ownerName,
                        slot.RenderedThisFrame ? " [rendered]" : " [cached]");

                    if (tile.Reserved)
                    {
                        continue;
                    }

                    // Importance drives who wins a tile, fade covers the hand-over, residency is
                    // what stops two lights trading the same tile every frame. A thrashing scene
                    // shows up as residency never climbing past MIN_RESIDENCY_SECONDS.
                    ImGui::SameLine();
                    ImGui::TextDisabled("  importance %.2f, fade %.2f, held %.1fs, %d tile%s",
                        tile.Importance, tile.Fade, tile.ResidencyTime,
                        tile.TileCount, tile.TileCount == 1 ? "" : "s");
                }

                ImGui::TreePop();
            }

            // An allocator that never runs out is an allocator whose exhaustion path has never run.
            // -1 is off; 0 forces every light to lose its tile, which is the setting that shows at a
            // glance that the control is actually connected.
            auto settings = Pine::Rendering::GraphicsSettings::Get();

            if (ImGui::SliderInt("Tile budget", &settings.LocalShadowTileBudget, 0, 16))
            {
                Pine::Rendering::GraphicsSettings::Set(settings);
            }

            int debugSlotLimit = Pine::Rendering::ShadowAtlas::GetDebugSlotLimit();
            if (ImGui::SliderInt("Force tile limit", &debugSlotLimit, -1, 16,
                                 debugSlotLimit < 0 ? "off" : "%d tiles"))
            {
                Pine::Rendering::ShadowAtlas::SetDebugSlotLimit(debugSlotLimit);
            }

            ImGui::Checkbox("View Shadow Atlas", &m_ShadowAtlasTexture);
        }

        if (ImGui::CollapsingHeader("Physics"))
        {
            if (ImGui::Button("Connect to PhysX debugger"))
            {
                Pine::Physics3D::ConnectVisualDebugger();
            }
        }

    }
    ImGui::End();

    if (m_AmbientOcclusionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Ambient Occlusion", &m_AmbientOcclusionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Rendering::AmbientOcclusion::GetOutputTexture()->GetGraphicsIdentifier());
            ImGui::Image(id, ImVec2(640, 360));
        }
        ImGui::End();
    }

    if (m_ShadowAtlasTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Shadow Atlas", &m_ShadowAtlasTexture))
        {
            if (auto* texture = Pine::Rendering::ShadowAtlas::GetTexture())
            {
                const std::uint64_t id = *static_cast<std::uint32_t*>(texture->GetGraphicsIdentifier());

                const auto origin = ImGui::GetCursorScreenPos();
                constexpr float displaySize = 512.f;

                ImGui::Image(id, ImVec2(displaySize, displaySize));

                // Tile borders drawn over the depth image: a depth atlas is near-featureless to
                // look at, and which tile is which is the whole question being asked here.
                auto* drawList = ImGui::GetWindowDrawList();

                const float scale = displaySize / static_cast<float>(Pine::Rendering::ShadowAtlas::GetResolution());

                for (const auto& slot : Pine::Rendering::ShadowAtlas::GetSlots())
                {
                    const auto min = ImVec2(origin.x + slot.Rect.x * scale, origin.y + slot.Rect.y * scale);
                    const auto max = ImVec2(min.x + slot.Rect.z * scale, min.y + slot.Rect.w * scale);

                    const ImU32 color = slot.Owner == nullptr
                        ? IM_COL32(80, 80, 80, 120)
                        : (slot.RenderedThisFrame ? IM_COL32(80, 220, 80, 255) : IM_COL32(220, 180, 60, 255));

                    drawList->AddRect(min, max, color);
                }
            }
        }
        ImGui::End();
    }

    if (m_DepthPositionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Position Texture", &m_DepthPositionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Pipeline3D::GetPositionTexture()->GetGraphicsIdentifier());
            ImGui::Image(id, ImVec2(640, 360));
        }
        ImGui::End();
    }
}

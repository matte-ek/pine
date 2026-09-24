#include "PropertiesPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

#include "AssetPropertiesRenderer/AssetPropertiesRenderer.hpp"
#include "EntityPropertiesRenderer/EntityPropertiesRenderer.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Audio/Audio.hpp"

namespace
{
    bool m_Active = true;

    // A clip preview is started from this panel, and is only stopped from it too. It ends once the
    // panel stops showing its clip, so a long one cannot be left playing with no way to stop it.
    void StopPreviewUnlessShown(const Pine::Asset* shownAsset)
    {
        const auto previewClip = Pine::Audio::GetPreviewClip();

        if (previewClip != nullptr && previewClip != shownAsset)
        {
            Pine::Audio::StopPreview();
        }
    }
}

void Panels::Properties::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Properties::GetActive()
{
    return m_Active;
}

void Panels::Properties::Render()
{
    if (!ImGui::Begin(ICON_MD_BUILD "  Properties", &m_Active))
    {
        StopPreviewUnlessShown(nullptr);

        ImGui::End();

        return;
    }

    if (!Selection::GetSelectedEntities().empty())
        EntityPropertiesPanel::Render(Selection::GetSelectedEntities().front());

    Pine::Asset* shownAsset = nullptr;

    if (!Selection::GetSelectedAssets().empty())
    {
        shownAsset = Selection::GetSelectedAssets().front();

        AssetPropertiesPanel::Render(shownAsset);
    }

    StopPreviewUnlessShown(shownAsset);

    ImGui::End();
}

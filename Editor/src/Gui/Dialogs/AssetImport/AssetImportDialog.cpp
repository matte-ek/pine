#include "AssetImportDialog.hpp"

#include <algorithm>
#include <chrono>
#include <optional>

#include <fmt/format.h>

#include "IconsMaterialDesign.h"
#include "imgui.h"

#include "Gui/Shared/AssetImportSettings/AssetImportSettings.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"

namespace
{
    using namespace Editor::Gui;

    constexpr auto DialogId = "Import Assets###AssetImportDialog";

    // How long one frame may spend importing before handing control back so the editor can paint.
    // A step is never cut short: a large BC7 texture takes longer than this on its own, and
    // stalling on one is better than never starting it.
    constexpr double FrameBudgetMilliseconds = 8.0;

    enum class DialogState
    {
        // Nothing dropped, nothing running.
        Idle,

        // Showing the plan, waiting for the user to accept or drop it.
        Review,

        // Working through the plan, a few entries per frame.
        Importing
    };

    struct Row
    {
        Pine::Importer::AssetImport* Import = nullptr;

        bool Selected = false;

        // What the asset's import settings were before the dialog let them be edited, for
        // whichever type this row turned out to be. Only Action::Update rows carry one: their
        // asset is a live, loaded one, so an edit here changes the real thing straight away and
        // Cancel has to be able to put it back. Action::Create rows need nothing -
        // DeleteContext() throws their assets away whole.
        std::optional<Pine::TextureImportConfiguration> TextureConfigurationRestore;
        std::optional<Pine::AudioImportConfiguration> AudioConfigurationRestore;
    };

    DialogState m_State = DialogState::Idle;

    std::vector<std::string> m_QueuedPaths;

    Pine::Importer::ImportContext* m_Context = nullptr;

    // The plan as it was shown to the user, and the only honest denominator for the progress
    // counter. Not context->Imports: importing a model appends the textures it discovers, so that
    // list grows as the import runs - a counter reading "3/58" would climb to "3/2400" partway
    // through a folder of models. The files the user dropped are what the progress is about; the
    // dependencies just make individual steps take longer.
    std::vector<Row> m_Rows;

    // Where a shift-click measures its range from.
    int m_SelectionAnchor = -1;

    const char* AssetTypeIcon(const Pine::AssetType type)
    {
        switch (type)
        {
            case Pine::AssetType::Texture2D:
            case Pine::AssetType::Texture3D:
                return ICON_MD_IMAGE;
            case Pine::AssetType::Model:
            case Pine::AssetType::Mesh:
                return ICON_MD_VIEW_IN_AR;
            case Pine::AssetType::Material:
                return ICON_MD_PALETTE;
            case Pine::AssetType::Shader:
                return ICON_MD_GRADIENT;
            case Pine::AssetType::Audio:
                return ICON_MD_VOLUME_UP;
            case Pine::AssetType::CSharpScript:
                return ICON_MD_CODE;
            case Pine::AssetType::Level:
                return ICON_MD_PUBLIC;
            case Pine::AssetType::Blueprint:
                return ICON_MD_WIDGETS;
            default:
                return ICON_MD_INSERT_DRIVE_FILE;
        }
    }

    // Plain language, because this is the payoff: an unsupported file or a collision used to be a
    // PError scrolling past in the console while the import carried on without it.
    const char* ActionText(const Pine::AssetImportAction action)
    {
        switch (action)
        {
            case Pine::AssetImportAction::Create:
                return "New";
            case Pine::AssetImportAction::Update:
                return "Replaces existing (keeps UId)";
            case Pine::AssetImportAction::Unsupported:
                return "Unsupported type";
            case Pine::AssetImportAction::Conflict:
                return "Blocked: something else is already there";
            default:
                return "Undetermined";
        }
    }

    ImVec4 ActionColor(const Pine::AssetImportAction action)
    {
        switch (action)
        {
            case Pine::AssetImportAction::Create:
                return ImVec4(0.45f, 0.85f, 0.55f, 1.f);
            case Pine::AssetImportAction::Update:
                return ImVec4(0.95f, 0.80f, 0.35f, 1.f);
            default:
                return ImVec4(0.95f, 0.45f, 0.40f, 1.f);
        }
    }

    bool WillImport(const Pine::Importer::AssetImport& import)
    {
        return import.Action == Pine::AssetImportAction::Create ||
               import.Action == Pine::AssetImportAction::Update;
    }

    Pine::Texture2D* RowTexture(const Row& row)
    {
        if (!row.Import || !WillImport(*row.Import))
        {
            return nullptr;
        }

        return dynamic_cast<Pine::Texture2D*>(row.Import->AssetPtr);
    }

    Pine::AudioFile* RowAudioFile(const Row& row)
    {
        if (!row.Import || !WillImport(*row.Import))
        {
            return nullptr;
        }

        return dynamic_cast<Pine::AudioFile*>(row.Import->AssetPtr);
    }

    void Dispose()
    {
        if (m_Context)
        {
            Pine::Importer::DeleteContext(m_Context);
            m_Context = nullptr;
        }

        m_Rows.clear();
        m_SelectionAnchor = -1;
        m_State = DialogState::Idle;
    }

    // Turns the queued paths into a resolved plan and opens the dialog on it.
    void BuildPlan()
    {
        m_Context = Editor::Utilities::Asset::CreateImportContext(m_QueuedPaths);

        m_QueuedPaths.clear();

        Pine::Importer::Resolve(m_Context);

        // Snapshot before anything proposes settings, not after: the proposal is itself a change
        // to a live asset for every row that re-imports one.
        for (const auto& import : m_Context->Imports)
        {
            Row row;

            row.Import = import.get();

            if (import->Action == Pine::AssetImportAction::Update)
            {
                if (const auto texture = dynamic_cast<Pine::Texture2D*>(import->AssetPtr))
                {
                    row.TextureConfigurationRestore = texture->GetImportConfiguration();
                }
                else if (const auto audioFile = dynamic_cast<Pine::AudioFile*>(import->AssetPtr))
                {
                    row.AudioConfigurationRestore = audioFile->GetImportConfiguration();
                }
            }

            m_Rows.push_back(row);
        }

        Pine::Importer::ProposeImportSettings(m_Context);

        m_State = DialogState::Review;

        ImGui::OpenPopup(DialogId);
    }

    // Drops the plan and puts back everything it changed. The Create rows need nothing - their
    // assets exist only inside the context, and DeleteContext() throws them away - but the Update
    // rows have been editing live assets, and none of that was asked for.
    void Abandon()
    {
        for (const auto& row : m_Rows)
        {
            if (row.TextureConfigurationRestore)
            {
                if (const auto texture = dynamic_cast<Pine::Texture2D*>(row.Import->AssetPtr))
                {
                    texture->GetImportConfiguration() = *row.TextureConfigurationRestore;
                }
            }

            if (row.AudioConfigurationRestore)
            {
                if (const auto audioFile = dynamic_cast<Pine::AudioFile*>(row.Import->AssetPtr))
                {
                    audioFile->GetImportConfiguration() = *row.AudioConfigurationRestore;
                }
            }
        }

        Dispose();
    }

    void Finish()
    {
        int imported = 0;
        int failed = 0;
        int created = 0;
        int updated = 0;

        for (const auto& import : m_Context->Imports)
        {
            if (import->ImportStatus == Pine::AssetImportStatus::Imported)
            {
                imported++;

                if (import->Action == Pine::AssetImportAction::Create)
                    created++;
                else if (import->Action == Pine::AssetImportAction::Update)
                    updated++;
            }
            else if (import->ImportStatus == Pine::AssetImportStatus::Failed)
            {
                failed++;
            }
        }

        PInfo(fmt::format("Imported {} assets ({} new, {} re-imported), {} failed",
            imported, created, updated, failed));

        Dispose();

        Editor::Utilities::Asset::RefreshAll();

        ImGui::CloseCurrentPopup();
    }

    void SelectRow(const int index)
    {
        const auto& io = ImGui::GetIO();

        if (io.KeyShift && m_SelectionAnchor >= 0 && m_SelectionAnchor < static_cast<int>(m_Rows.size()))
        {
            const auto first = std::min(m_SelectionAnchor, index);
            const auto last = std::max(m_SelectionAnchor, index);

            if (!io.KeyCtrl)
            {
                for (auto& row : m_Rows)
                {
                    row.Selected = false;
                }
            }

            for (auto i = first; i <= last; i++)
            {
                m_Rows[i].Selected = true;
            }

            return;
        }

        if (io.KeyCtrl)
        {
            m_Rows[index].Selected = !m_Rows[index].Selected;
            m_SelectionAnchor = index;

            return;
        }

        for (auto& row : m_Rows)
        {
            row.Selected = false;
        }

        m_Rows[index].Selected = true;
        m_SelectionAnchor = index;
    }

    void RenderPlanTable()
    {
        constexpr auto tableFlags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Resizable;

        if (!ImGui::BeginTable("##ImportPlan", 4, tableFlags, ImVec2(-1.f, -1.f)))
        {
            return;
        }

        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.f);
        ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(m_Rows.size()); i++)
        {
            auto& row = m_Rows[i];
            const auto& import = *row.Import;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            const auto fileName = import.SourcePaths.empty() ?
                std::string("(no source file)") : import.SourcePaths.front().filename().string();

            if (ImGui::Selectable(fmt::format("{}##ImportRow{}", fileName, i).c_str(),
                    row.Selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                SelectRow(i);
            }

            if (ImGui::IsItemHovered() && !import.SourcePaths.empty())
            {
                ImGui::SetTooltip("%s", import.SourcePaths.front().string().c_str());
            }

            ImGui::TableSetColumnIndex(1);

            if (import.Type == Pine::AssetType::Invalid)
            {
                ImGui::TextDisabled("-");
            }
            else
            {
                ImGui::Text("%s  %s", AssetTypeIcon(import.Type), AssetTypeToHumanString(import.Type));
            }

            ImGui::TableSetColumnIndex(2);

            if (WillImport(import))
            {
                ImGui::TextUnformatted(import.ResolvedEnginePath.c_str());
            }
            else
            {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(ActionColor(import.Action), "%s", ActionText(import.Action));
        }

        ImGui::EndTable();
    }

    // Both of the settings blocks below edit the first of the selection and push whatever changed
    // onto the rest, rather than writing every field to every asset - otherwise selecting a mixed
    // set and touching one control would flatten the settings they didn't ask about.
    void RenderTextureSettings(const std::vector<Pine::Texture2D*>& textures)
    {
        if (textures.empty())
        {
            return;
        }

        ImGui::TextDisabled("%zu texture(s) selected", textures.size());
        ImGui::Spacing();

        auto& leadConfiguration = textures.front()->GetImportConfiguration();

        const auto changes = AssetImportSettings::RenderTexture(leadConfiguration);

        if (changes.Any())
        {
            for (size_t i = 1; i < textures.size(); i++)
            {
                AssetImportSettings::ApplyTextureChanges(
                    leadConfiguration, textures[i]->GetImportConfiguration(), changes);
            }
        }
    }

    void RenderAudioSettings(const std::vector<Pine::AudioFile*>& audioFiles)
    {
        if (audioFiles.empty())
        {
            return;
        }

        ImGui::TextDisabled("%zu audio clip(s) selected", audioFiles.size());
        ImGui::Spacing();

        auto& leadConfiguration = audioFiles.front()->GetImportConfiguration();

        const auto changes = AssetImportSettings::RenderAudio(leadConfiguration);

        if (changes.Any())
        {
            for (size_t i = 1; i < audioFiles.size(); i++)
            {
                AssetImportSettings::ApplyAudioChanges(
                    leadConfiguration, audioFiles[i]->GetImportConfiguration(), changes);
            }
        }
    }

    void RenderSettings()
    {
        std::vector<Pine::Texture2D*> selectedTextures;
        std::vector<Pine::AudioFile*> selectedAudioFiles;

        for (const auto& row : m_Rows)
        {
            if (!row.Selected)
            {
                continue;
            }

            if (const auto texture = RowTexture(row))
            {
                selectedTextures.push_back(texture);
            }
            else if (const auto audioFile = RowAudioFile(row))
            {
                selectedAudioFiles.push_back(audioFile);
            }
        }

        ImGui::TextDisabled("Import Settings");
        ImGui::Spacing();

        if (selectedTextures.empty() && selectedAudioFiles.empty())
        {
            ImGui::TextWrapped(
                "Select one or more textures or audio clips to change how they are imported. "
                "Other asset types have no import settings yet.");

            return;
        }

        RenderTextureSettings(selectedTextures);

        if (!selectedTextures.empty() && !selectedAudioFiles.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        RenderAudioSettings(selectedAudioFiles);
    }

    void RenderReview()
    {
        int importable = 0;
        int blocked = 0;

        for (const auto& row : m_Rows)
        {
            if (WillImport(*row.Import))
                importable++;
            else
                blocked++;
        }

        const auto footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.f;

        ImGui::BeginChild("##ImportPlanArea", ImVec2(-360.f, -footerHeight), false);
        {
            RenderPlanTable();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ImportSettingsArea", ImVec2(-1.f, -footerHeight), true);
        {
            RenderSettings();
        }
        ImGui::EndChild();

        ImGui::Separator();

        const auto footerRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

        if (blocked > 0)
        {
            ImGui::TextColored(ActionColor(Pine::AssetImportAction::Conflict),
                "%d of %d files will be skipped.", blocked, static_cast<int>(m_Rows.size()));
        }
        else
        {
            ImGui::TextDisabled("%d files ready to import.", importable);
        }

        ImGui::SameLine(footerRight - 212.f);

        if (importable == 0)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Import", ImVec2(100.f, 0.f)))
        {
            m_State = DialogState::Importing;
        }

        if (importable == 0)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100.f, 0.f)))
        {
            Abandon();

            ImGui::CloseCurrentPopup();
        }
    }

    void RenderImporting()
    {
        const auto start = std::chrono::steady_clock::now();

        bool remaining;

        do
        {
            remaining = Pine::Importer::ExecuteNext(m_Context);
        }
        while (remaining &&
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()
                   < FrameBudgetMilliseconds);

        const auto total = m_Rows.size();
        const auto done = std::min(m_Context->ExecuteCursor, total);

        ImGui::Spacing();
        ImGui::Text("Importing... %zu/%zu assets", done, total);
        ImGui::Spacing();

        ImGui::ProgressBar(total == 0 ? 1.f : static_cast<float>(done) / static_cast<float>(total),
            ImVec2(-1.f, 0.f));

        if (!remaining)
        {
            Finish();
        }
    }
}

bool Dialog::AssetImport::IsPending()
{
    return m_State != DialogState::Idle || !m_QueuedPaths.empty();
}

void Dialog::AssetImport::Queue(const std::vector<std::string>& paths)
{
    m_QueuedPaths.insert(m_QueuedPaths.end(), paths.begin(), paths.end());
}

void Dialog::AssetImport::Render()
{
    if (m_State == DialogState::Idle)
    {
        if (m_QueuedPaths.empty())
        {
            return;
        }

        BuildPlan();
    }

    ImGui::SetNextWindowSize(ImVec2(1080.f, 580.f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal(DialogId, nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        // ImGui closes a modal when Escape is pressed. While reviewing, that means the same thing
        // as Cancel. Once the import is running it can't mean anything: every step taken so far
        // has already written its '.passet'. Put the dialog back up and carry on.
        if (m_State == DialogState::Importing)
        {
            ImGui::OpenPopup(DialogId);

            return;
        }

        Abandon();

        return;
    }

    if (m_State == DialogState::Review)
    {
        RenderReview();
    }
    else
    {
        RenderImporting();
    }

    ImGui::EndPopup();
}

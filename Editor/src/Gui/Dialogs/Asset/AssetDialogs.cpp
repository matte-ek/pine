#include "AssetDialogs.hpp"

#include <imgui.h>

#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Other/OsNative/OsNative.hpp"
#include "Projects/Projects.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"

namespace
{
    using namespace Editor::Gui;

    auto m_CurrentCreateDirectory = false;
    auto m_CurrentCreateAssetType = Pine::AssetType::Invalid;

    void DisplayAssetContextMenu()
    {
        bool openCreateItemDialog = false;
        bool openRenameItemDialog = false;

        if (ImGui::BeginPopup("AssetContextMenu", 0))
        {
            auto selectedNode = Panels::AssetBrowser::GetSelectedNode();

            const bool isTargetingAsset = !Selection::GetSelectedAssets().empty();
            const bool isTargetingDirectory = selectedNode && selectedNode->Type == AssetHierarchy::NodeType::Directory;

            if (ImGui::BeginMenu("Create"))
            {
                m_CurrentCreateDirectory = false;
                m_CurrentCreateAssetType = Pine::AssetType::Invalid;

                if (ImGui::MenuItem("Directory", nullptr))
                {
                    m_CurrentCreateDirectory = true;
                }

                if (ImGui::MenuItem("Material", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::Material;
                }

                if (ImGui::MenuItem("Level", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::Level;
                }

                if (ImGui::MenuItem("Texture3D", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::Texture3D;
                }

                if (ImGui::MenuItem("Tile set", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::Tileset;
                }

                if (ImGui::MenuItem("Tile map", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::Tilemap;
                }

                if (ImGui::MenuItem("Script", nullptr))
                {
                    m_CurrentCreateAssetType = Pine::AssetType::CSharpScript;
                }

                if (m_CurrentCreateAssetType != Pine::AssetType::Invalid || m_CurrentCreateDirectory)
                {
                    openCreateItemDialog = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Open in File Browser"))
            {
                Editor::OsNative::OpenFileExplorer(std::filesystem::absolute(Panels::AssetBrowser::GetOpenDirectoryNode()->Path));
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Rename", "F2", false, isTargetingAsset || isTargetingDirectory))
            {
                openRenameItemDialog = true;
            }

            if (ImGui::MenuItem("Remove", "DEL", false, isTargetingAsset || isTargetingDirectory))
            {
                /*
                 MOVE TO ASSET UTIL!!!!

                if (isTargetingAsset && m_SelectedNode->Asset->GetType() == Pine::AssetType::CSharpScript)
                {
                    Editor::Utilities::Script::DeleteScript(m_SelectedNode->Asset->GetFilePath().string());
                }

                std::filesystem::remove(m_SelectedNode->Path.string());
                */

                Selection::Clear();

                Panels::AssetBrowser::BuildAssetHierarchy();
            }

            ImGui::EndPopup();
        }

        if (openRenameItemDialog)
            ImGui::OpenPopup("RenameItemDialog");
        if (openCreateItemDialog)
            ImGui::OpenPopup("CreateItemDialog");
    }

    void DisplayCreateDialog()
    {
        ImGui::SetNextWindowSize(ImVec2(320.f, 135.f));

        if (ImGui::BeginPopupModal("CreateItemDialog", nullptr, ImGuiWindowFlags_NoResize))
        {
            static char buffer[64];

            ImGui::Text("Item Name:");

            if (ImGui::IsWindowAppearing())
            {
                strcpy(buffer, "");
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::SetNextItemWidth(300.f);
            ImGui::InputText("##ItemName", buffer, IM_ARRAYSIZE(buffer));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            if ((ImGui::Button("Create", ImVec2(100.f, 30.f)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) && strlen(buffer) > 0)
            {
                auto openDirectory = Panels::AssetBrowser::GetOpenDirectoryNode();
                auto targetPath = openDirectory->Path.string() + "/" + buffer;

                if (m_CurrentCreateDirectory)
                {
                    std::filesystem::create_directory(targetPath);
                }
                else
                {
                    Editor::Utilities::Asset::CreateEmptyAsset(targetPath, m_CurrentCreateAssetType);
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(100.f, 30.f)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void DisplayRenameDialog()
    {
        ImGui::SetNextWindowSize(ImVec2(300.f, 120.f));

        if (ImGui::BeginPopupModal("RenameItemDialog", nullptr, ImGuiWindowFlags_NoResize))
        {
            static char buffer[128];

            auto selectedNode = Panels::AssetBrowser::GetSelectedNode();

            if (ImGui::IsWindowAppearing())
            {
                if (selectedNode->DisplayText.size() < 127)
                {
                    strcpy(buffer, selectedNode->DisplayText.c_str());
                }
            }

            ImGui::Text("Name:");
            ImGui::InputText("##Rename", buffer, IM_ARRAYSIZE(buffer));

            if ((ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter)) && strlen(buffer) > 0)
            {
                const auto asset = Selection::GetSelectedAssets()[0];
                const auto newPath = selectedNode->Path.parent_path().string() + "/" + buffer;

                // Create a new hard link at the new location.
                std::filesystem::remove(selectedNode->Path.c_str());
                std::filesystem::create_hard_link(newPath.c_str(), asset->GetFilePath());

                // Update mapped path
                asset->SetPath(Editor::Utilities::Asset::EstimateMappedPath(newPath, Editor::Projects::GetProjectPath() + "/assets/"));

                Panels::AssetBrowser::BuildAssetHierarchy();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

void Dialog::Asset::Display()
{
    DisplayAssetContextMenu();
    DisplayCreateDialog();
    DisplayRenameDialog();
}

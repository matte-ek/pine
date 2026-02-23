#include "AssetBrowserPanel.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "IconsMaterialDesign.h"
#include "imgui.h"
#include "AssetHierarchy/AssetHierarchy.hpp"
#include "Gui/Dialogs/Asset/AssetDialogs.hpp"
#include "Gui/Shared/IconStorage/IconStorage.hpp"

#include "Gui/Shared/Selection/Selection.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"

#include "Other/OsNative/OsNative.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Projects/Projects.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"
#include "Utilities/Scripts/ScriptUtilities.hpp"

namespace
{
    using namespace Editor::Gui;

    bool m_Active = true;
    char m_SearchBuffer[64];

    int m_IconSize = 64;

    AssetHierarchy::Node* m_Root = nullptr;
    AssetHierarchy::Node* m_CurrentDirectoryNode = nullptr;
    AssetHierarchy::Node* m_SelectedNode = nullptr;

    bool DisplayAssetNode(AssetHierarchy::Node* node)
    {
        static auto fileIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file");

        const auto isSelected = Selection::IsSelected(node->Asset.Get());
        const auto icon = node->DisplayIcon == nullptr ? fileIcon->GetGraphicsTexture() : node->DisplayIcon;

        bool isClicked = false;

        if (Widgets::Icon(node->DisplayText, icon, isSelected, m_IconSize))
        {
            isClicked = true;
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            isClicked = true;
            ImGui::OpenPopup("AssetContextMenu");
        }

        // Allow drag dropping of this asset node
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(icon->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));
            ImGui::SameLine();

            auto asset = node->Asset.Get();

            ImGui::BeginChild("##DragDropInfo", ImVec2(200.f, 64.f));
            {
                ImGui::Text("%s", node->DisplayText.c_str());
                ImGui::Text("%s", AssetTypeToString(asset->GetType()));
            }
            ImGui::EndChild();

            ImGui::SetDragDropPayload("Asset", &asset, sizeof(Pine::Asset*));

            ImGui::EndDragDropSource();
        }

        return isClicked;
    }

    AssetHierarchy::Node* DisplayDirectoryNode(AssetHierarchy::Node* node)
    {
        static auto folderIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/folder");

        AssetHierarchy::Node* selectedNode = nullptr;

        if (Widgets::Icon(node->DisplayText, folderIcon, m_SelectedNode == node, m_IconSize))
        {
            // Somewhat special hack for the "..." backwards directory.
            if (node->IsBackwardsDirectory)
                selectedNode = node->Parent;
            else
                selectedNode = node;
        }

        // Allow drag dropping of the directory.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(folderIcon->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));
            ImGui::SameLine();
            ImGui::Text("%s", node->DisplayText.c_str());

            ImGui::SetDragDropPayload("Directory", &node, sizeof(AssetHierarchy::Node*));

            ImGui::EndDragDropSource();
        }

        // Allow drag dropping into the directory.
        if (ImGui::BeginDragDropTarget())
        {
            // TODO: Allow moving of assets.

            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            Selection::Clear();

            m_SelectedNode = node;

            ImGui::OpenPopup("AssetContextMenu");
        }

        return selectedNode;
    }

    AssetHierarchy::Node* DisplayNode(const AssetHierarchy::Node* node)
    {
        AssetHierarchy::Node* clickedNode = nullptr;

        // Render directory nodes
        for (auto& directory : node->Children)
        {
            if (directory->Type == AssetHierarchy::NodeType::File)
            {
                continue;
            }

            if (const auto newNode = DisplayDirectoryNode(directory))
            {
                clickedNode = newNode;
            }

            ImGui::NextColumn();
        }

        // Then, render all the asset nodes
        for (auto& file : node->Children)
        {
            if (file->Type == AssetHierarchy::NodeType::Directory)
            {
                continue;
            }

            if (DisplayAssetNode(file))
            {
                clickedNode = file;
            }

            ImGui::NextColumn();
        }

        return clickedNode;
    }

    AssetHierarchy::Node* DisplayAssetNodeSearch(const AssetHierarchy::Node* root, const std::string& searchQuery)
    {
        AssetHierarchy::Node* clickedNode = nullptr;

        for (auto& file : root->Children)
        {
            if (file->Type == AssetHierarchy::NodeType::Directory)
            {
                continue;
            }

            if (Pine::String::ToLower(file->DisplayText).find(searchQuery) == std::string::npos)
            {
                continue;
            }

            if (DisplayAssetNode(file))
            {
                clickedNode = file;
            }
        }

        for (auto& directory : root->Children)
        {
            if (directory->Type == AssetHierarchy::NodeType::File)
            {
                continue;
            }

            if (const auto clickedAsset = DisplayAssetNodeSearch(directory, searchQuery))
            {
                clickedNode = clickedAsset;
            }
        }

        return clickedNode;
    }

    void DisplayNodeTree(AssetHierarchy::Node* node)
    {
        static std::string renderText(128, ' ');

        renderText.clear();

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding;
        bool hasDirectoryChildren = false;

        for (const auto child : node->Children)
        {
            if (child->Type == AssetHierarchy::NodeType::File)
                continue;
            if (child->IsBackwardsDirectory)
                continue;

            hasDirectoryChildren = true;

            break;
        }

        if (node == m_Root)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

            renderText = node->DisplayText + "##" + node->Path.string();
        }
        else
        {
            renderText = ICON_MD_FOLDER " " + node->DisplayText + "##" + node->Path.string();
        }

        if (!hasDirectoryChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;

            // Oh yes, this is stupid as fuck.
            // Just hacky workaround since I do not
            // know how to deal with imgui properly.
            renderText = "         " + renderText;
        }

        bool restoreFrameColor = false;

        if (m_CurrentDirectoryNode != node)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));

            restoreFrameColor = true;
        }

        if (ImGui::TreeNodeEx(renderText.c_str(), flags))
        {
            if (restoreFrameColor)
            {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemClicked())
            {
                m_CurrentDirectoryNode = node;
            }

            for (const auto child : node->Children)
            {
                if (child->Type == AssetHierarchy::NodeType::File)
                    continue;
                if (child->IsBackwardsDirectory)
                    continue;

                DisplayNodeTree(child);
            }

            ImGui::TreePop();
        }
        else
        {
            if (ImGui::IsItemClicked())
            {
                m_CurrentDirectoryNode = node;
            }

            if (restoreFrameColor)
            {
                ImGui::PopStyleColor();
            }
        }
    }

    void RenderAssetTreeView()
    {
        if (m_Root == nullptr)
        {
            return;
        }

        DisplayNodeTree(m_Root);
    }
}

void Panels::AssetBrowser::SetActive(const bool value)
{
    m_Active = value;
}

bool Panels::AssetBrowser::GetActive()
{
    return m_Active;
}

AssetHierarchy::Node* Panels::AssetBrowser::GetRootNode()
{
    return m_Root;
}

AssetHierarchy::Node* Panels::AssetBrowser::GetOpenDirectoryNode()
{
    return m_CurrentDirectoryNode;
}

AssetHierarchy::Node* Panels::AssetBrowser::GetSelectedNode()
{
    return m_SelectedNode;
}

void Panels::AssetBrowser::BuildAssetHierarchy()
{
    const auto projectAssetsDirectory = Editor::Projects::GetProjectPath() + "/assets";

    m_SelectedNode = nullptr;

    // If we're currently in a directory, we would want to save the path so we can restore to it
    // after rebuilding the asset hierarchy.
    std::string restoreDirectory;
    if (m_CurrentDirectoryNode != nullptr)
    {
        restoreDirectory = m_CurrentDirectoryNode->Path.string();
        m_CurrentDirectoryNode = nullptr;
    }

    // Make sure to dispose of the previous nodes.
    if (m_Root != nullptr)
    {
        m_Root->Dispose();
        delete m_Root;
        m_Root = nullptr;
    }

    // First update the icon cache, as we grab the icons from the IconStorage
    // as we fill out the nodes.
    IconStorage::Update();

    // Create the new root node and fill it out.
    m_Root = AssetHierarchy::CreateRootNode();

    AssetHierarchy::PopulateMappedNode(m_Root, "engine", "engine");
    AssetHierarchy::PopulateNode(m_Root, projectAssetsDirectory, projectAssetsDirectory);

    // Make sure we always have a current directory.
    m_CurrentDirectoryNode = m_Root;

    // If we've stored any previous user directory earlier, restore to it.
    if (!restoreDirectory.empty())
    {
        m_CurrentDirectoryNode = AssetHierarchy::FindDirectoryNode(m_Root, restoreDirectory);
    }
}

void Panels::AssetBrowser::Render()
{
    if (!ImGui::Begin(ICON_MD_FOLDER_OPEN " Asset Browser", &m_Active))
    {
        ImGui::End();

        return;
    }

    ImGui::BeginChild("##AssetsTreeView", ImVec2(300.f, -1), ImGuiChildFlags_Border, 0);
    {
        RenderAssetTreeView();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##Assets", ImVec2(-1, -1), 0);
    {
        if (ImGui::Button(ICON_MD_FOLDER_OPEN " Import"))
        {
            Pine::Script::Manager::ReloadGameAssembly();
        }

        ImGui::SameLine();

        if (ImGui::Button(ICON_MD_REFRESH " Refresh") || m_Root == nullptr)
        {
            BuildAssetHierarchy();
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(220.f);

        ImGui::InputTextWithHint("##AssetSearch", ICON_MD_SEARCH " Search...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));

        ImGui::BeginChild("##AssetsBrowser", ImVec2(-1.f, -1.f), ImGuiChildFlags_Border, 0);
        {
            const bool isSearching = strlen(m_SearchBuffer) > 0;
            const int iconSizePadding = m_IconSize + 16;
            const float spaceAvailable = ImGui::GetContentRegionAvail().x - static_cast<float>(iconSizePadding * 2);
            const int nrColumns = static_cast<int>(spaceAvailable) / iconSizePadding;

            if (nrColumns <= 0)
            {
                ImGui::EndChild();
                ImGui::EndChild();
                ImGui::End();

                return;
            }

            ImGui::Columns(nrColumns, nullptr, false);

            // Use special node display functions when searching
            if (isSearching)
            {
                const std::string searchQuery = Pine::String::ToLower(m_SearchBuffer);

                if (const auto selectedEntry = DisplayAssetNodeSearch(m_CurrentDirectoryNode, searchQuery))
                {
                    if (selectedEntry->Type == AssetHierarchy::NodeType::File)
                    {
                        if (selectedEntry->Asset.Get() != nullptr)
                        {
                            Selection::Add<Pine::Asset>(selectedEntry->Asset.Get(), true);
                        }
                    }
                }
            }
            else
            {
                if (const auto newEntrySelection = DisplayNode(m_CurrentDirectoryNode))
                {
                    if (newEntrySelection->Type == AssetHierarchy::NodeType::Directory)
                    {
                        m_CurrentDirectoryNode = newEntrySelection;
                        m_SelectedNode = newEntrySelection;
                    }
                    else
                    {
                        if (newEntrySelection->Asset.Get() != nullptr)
                        {
                            Selection::Add<Pine::Asset>(newEntrySelection->Asset.Get(), true);
                        }

                        m_SelectedNode = newEntrySelection;
                    }
                }
            }

            ImGui::Columns(1);

            // Allow the user to clear out any selections by clicking the empty area.
            if (ImGui::IsWindowHovered())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    Selection::Clear();

                    m_SelectedNode = nullptr;
                }

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsPopupOpen("AssetContextMenu"))
                {
                    ImGui::OpenPopup("AssetContextMenu");

                    m_SelectedNode = nullptr;
                }
            }

            // Handle all context menus and dialogs.
            Dialog::Asset::Display();
        }
        ImGui::EndChild();

        // Allow some special drag dropping into the general asset browser area, for example, we might want
        // the user to drag drop an entity to create a blueprint.
        if (ImGui::BeginDragDropTarget())
        {
            if (ImGui::AcceptDragDropPayload("Entity", ImGuiDragDropFlags_SourceAllowNullID))
            {
                const auto entity = *static_cast<Pine::Entity**>(ImGui::GetDragDropPayload()->Data);
                // TODO: Fix me.
            }

            ImGui::EndDragDropTarget();
        }

        if (!Selection::GetSelectedAssets().empty())
        {
            for (auto& asset : Selection::GetSelectedAssets())
            {
                IconStorage::MarkIconDirty(asset->GetUId());
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
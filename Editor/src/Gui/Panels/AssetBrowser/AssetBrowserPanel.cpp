#include "AssetBrowserPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/String/String.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "Gui/Shared/IconStorage/IconStorage.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/Material/Material.hpp"

namespace
{

    // Items with the file extensions specified below will not be shown in the user interface.
    // Can be used for example metadata files.
    std::vector<std::string> m_FileExtensionIgnoreList = { ".asset" };

    int m_IconSize = 64;

    enum class EntryType
    {
        Directory,
        File
    };

    struct PathEntry
    {
        std::filesystem::path Path;
        EntryType Type = EntryType::File;

        std::string DisplayText;

        Pine::IAsset* Asset = nullptr;

        Pine::Graphics::ITexture* Icon = nullptr;

        PathEntry* Parent = nullptr;
        std::vector<PathEntry*> Children;

        void Dispose()
        {
            for (auto child : Children)
            {
                child->Dispose();

                delete child;
            }
        } 
    };

    bool m_Active = true;
    char m_SearchBuffer[64];

    PathEntry* m_Root = nullptr;
    PathEntry* m_SelectedEntry = nullptr;
    PathEntry* m_SelectedItem = nullptr;

    void ProcessDirectoryTree(const std::filesystem::path& path, PathEntry* entry)
    {
        // Add a "go back" directory for the user first, displayed as "..."
        if (entry->Parent != nullptr)
        {
            auto backEntry = new PathEntry;

            backEntry->Type = EntryType::Directory;
            backEntry->Path = entry->Parent->Path;
            backEntry->Parent = entry->Parent;
            backEntry->DisplayText = "...";

            entry->Children.push_back(backEntry);
        }

        for (const auto& directoryEntry : std::filesystem::directory_iterator(path))
        {
            auto newEntry = new PathEntry;

            newEntry->Type = directoryEntry.is_directory() ? EntryType::Directory : EntryType::File;
            newEntry->Path = directoryEntry.path();
            newEntry->Parent = entry;
            newEntry->DisplayText = directoryEntry.path().filename().string();

            if (newEntry->Type == EntryType::File)
            {
                bool ignoreEntry = false;

                for (const auto& fileExtensionIgnoreEntry : m_FileExtensionIgnoreList)
                {
                    if (newEntry->Path.extension().string() == fileExtensionIgnoreEntry)
                    {
                        ignoreEntry = true;
                        break;
                    }
                }

                if (ignoreEntry)
                {
                    delete newEntry;
                    continue;
                }

                newEntry->Asset = Pine::Assets::Get(newEntry->Path.string(), true);

            	if (newEntry->Asset)
				{
                    newEntry->Icon = IconStorage::GetIconTexture(newEntry->Asset->GetPath());
                }
            }
            else 
            {
                ProcessDirectoryTree(directoryEntry, newEntry);
            }

            entry->Children.push_back(newEntry);
        }
    }

    PathEntry* FindDirectoryEntry(PathEntry* root, const std::filesystem::path& path)
    {
        if (root->Path == path)
            return root;
        if (root->Type != EntryType::Directory || root->DisplayText == "...")
            return nullptr;

        for (const auto& entry : root->Children)
        {
            if (const auto ret = FindDirectoryEntry(entry, path))
                return ret;
        }

        return nullptr;
    }

    void MapDirectoryEntry(const std::filesystem::path& path, const std::string& displayName, PathEntry* entry)
    {
        auto mappedEntry = new PathEntry;

        mappedEntry->Type = EntryType::Directory;
        mappedEntry->Path = path;
        mappedEntry->DisplayText = displayName;
        mappedEntry->Parent = entry;

        ProcessDirectoryTree(path, mappedEntry);

        entry->Children.push_back(mappedEntry);
    }

    PathEntry* RenderAssetEntry(PathEntry* file)
    {
        static auto fileIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

        PathEntry* clickedItem = nullptr;

        const auto isSelected = Selection::IsSelected(file->Asset);
        const auto icon = file->Icon == nullptr ? fileIcon->GetGraphicsTexture() : file->Icon;

        if (Widgets::Icon(file->DisplayText, icon, isSelected, m_IconSize))
        {
            clickedItem = file;
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            clickedItem = file;

            ImGui::OpenPopup("AssetContextMenu");
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(icon->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));
            ImGui::SameLine();

            ImGui::BeginChild("##DragDropInfo", ImVec2(200.f, 64.f));
            {
                ImGui::Text("%s", file->DisplayText.c_str());
                ImGui::Text("%s", AssetTypeToString(file->Asset->GetType()));
            }
            ImGui::EndChild();

            ImGui::SetDragDropPayload("Asset", &file->Asset, sizeof(Pine::IAsset*));

            ImGui::EndDragDropSource();
        }

        ImGui::NextColumn();

        return clickedItem;
    }

    PathEntry* RenderEntry(const PathEntry* entry, bool& rebuildTree)
    {
        static auto folderIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/folder.png");
        static auto fileIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

        PathEntry* clickedItem = nullptr;

        // Display directories first
        for (auto& directory : entry->Children)
        {
            if (directory->Type == EntryType::File)
                continue;

            if (Widgets::Icon(directory->DisplayText, folderIcon, m_SelectedItem == directory, m_IconSize))
            {
                if (directory->DisplayText == "...")
                    clickedItem = directory->Parent;
                else
                    clickedItem = directory;
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(folderIcon->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));
                ImGui::SameLine();
                ImGui::Text("%s", directory->DisplayText.c_str());

                ImGui::SetDragDropPayload("Directory", &directory, sizeof(PathEntry*));

                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_SourceAllowNullID))
                {
                    const auto asset = *static_cast<Pine::IAsset**>(ImGui::GetDragDropPayload()->Data);
                    const auto newPath = directory->Path.string() + "/" + asset->GetFileName();

                    std::filesystem::rename(asset->GetFilePath(), newPath);

                    Pine::Assets::MoveAsset(asset, newPath);

                    rebuildTree = true;
                }

                if (ImGui::AcceptDragDropPayload("Directory", ImGuiDragDropFlags_SourceAllowNullID))
                {
                    const auto newDirectory = *static_cast<PathEntry**>(ImGui::GetDragDropPayload()->Data);
                    const auto newPath = newDirectory->Path.string() + "/" + directory->Parent->DisplayText;

                    std::filesystem::rename(directory->Path, newPath);

                    rebuildTree = true;

                    // This is kind of messy.
                    /*
                    std::function<void(PathEntry*, PathEntry*)> moveChildren;
                    moveChildren = [moveChildren](PathEntry* entry, PathEntry* newDirectory)
                    {
                        for (auto& child : entry->Children)
                        {
                            if (child->Type == EntryType::Directory)
                            {
                                moveChildren(child, newDirectory);
                            }
                            else
                            {
                                const auto newPath = newDirectory->Path.string() + "/" + child->DisplayText;

                                std::filesystem::rename(child->Path, newPath);

                                Pine::Assets::MoveAsset(child->Asset, newPath);
                            }
                        }
                    };

                    moveChildren(directory, newDirectory);
                    */
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                Selection::Clear();
                m_SelectedItem = directory;

                ImGui::OpenPopup("AssetContextMenu");
            }

            ImGui::NextColumn();
        }

        for (auto& file : entry->Children)
        {
            if (file->Type == EntryType::Directory)
                continue;

            if (auto clickedAsset = RenderAssetEntry(file))
            {
                clickedItem = clickedAsset;
            }
        }
        
        return clickedItem;
    }

    PathEntry* RenderEntrySearch(const PathEntry* root, const std::string& searchQuery)
    {
        PathEntry* clickedItem = nullptr;

        for (auto& file : root->Children)
        {
            if (file->Type == EntryType::Directory)
                continue;

            if (Pine::String::ToLower(file->DisplayText).find(Pine::String::ToLower(searchQuery)) == std::string::npos)
            {
                continue;
            }

            if (auto clickedAsset = RenderAssetEntry(file))
            {
                clickedItem = clickedAsset;
            }
        }

        for (auto& directory : root->Children)
        {
            if (directory->Type == EntryType::File)
                continue;

            if (auto clickedAsset = RenderEntrySearch(directory, searchQuery))
            {
                clickedItem = clickedAsset;
            }
        }

        return clickedItem;
    }

    void RenderContextMenu()
    {
        static int itemCreationType = -1;

        static auto renderRenameDialog = [&](bool refresh)
        {
            ImGui::SetNextWindowSize(ImVec2(300.f, 120.f));

            if (ImGui::BeginPopupModal("RenameItemDialog", nullptr, ImGuiWindowFlags_NoResize))
            {
                static char buffer[128];

                if (refresh)
                {
                    if (m_SelectedItem->DisplayText.size() < 127)
                        strcpy(buffer, m_SelectedItem->DisplayText.c_str());
                }

                ImGui::Text("Item Name:");
                ImGui::InputText("##Rename", buffer, IM_ARRAYSIZE(buffer));

                if ((ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter)) && strlen(buffer) > 0)
                {
                    const auto asset = Selection::GetSelectedAssets()[0];
                    const auto newPath = asset->GetFilePath().parent_path().string() + "/" + buffer;

                    std::filesystem::rename(m_SelectedItem->Path, newPath);

                    Pine::Assets::MoveAsset(asset, newPath);

                    m_SelectedItem = nullptr;

                    Panels::AssetBrowser::RebuildAssetTree();

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        };

        static auto createItemDialog = [&](bool refresh)
        {
            ImGui::SetNextWindowSize(ImVec2(300.f, 140.f));

            if (ImGui::BeginPopupModal("CreateItemDialog", nullptr, ImGuiWindowFlags_NoResize))
            {
                static char buffer[64];

                if (refresh)
                {
                    strcpy(buffer, "");
                }

                ImGui::Text("Item Name:");
                ImGui::InputText("##ItemName", buffer, IM_ARRAYSIZE(buffer));

                if ((ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) && strlen(buffer) > 0)
                {
                    Pine::IAsset* asset = nullptr;

                    switch (itemCreationType)
                    {
                        case 0:
                            // not going to error check the buffer here, going to blame user error if something goes wrong :-)
                            std::filesystem::create_directory(m_SelectedEntry->Path.string() + "/" + buffer);
                            break;
                        case 1:
                            asset = new Pine::Material();
                            break;
                        case 2:
                            asset = new Pine::Level();
                            break;
                        case 3:
                            asset = new Pine::Texture3D();
                            break;
                        default:
                            break;
                    }

                    if (asset)
                    {
                        // TODO: Append file extension if required.
                        asset->SetFilePath(m_SelectedEntry->Path.string() + "/" + buffer);
                        asset->SaveToFile();
                        asset->Dispose();

                        delete asset;
                    }

                    itemCreationType = -1;

                    Panels::AssetBrowser::RebuildAssetTree();

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    itemCreationType = -1;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        };

        bool openRenameItemDialog = false;
        bool openCreateItemDialog = false;

        if (ImGui::BeginPopup("AssetContextMenu", 0))
        {
            const bool isTargetingAsset = !Selection::GetSelectedAssets().empty();
            const bool isTargetingDirectory = m_SelectedItem && m_SelectedItem->Type == EntryType::Directory;

            if (ImGui::BeginMenu("Create"))
            {
                itemCreationType = -1;

                if (ImGui::MenuItem("Directory", nullptr))
                {
                    itemCreationType = 0;
                }

                if (ImGui::MenuItem("Material", nullptr))
                {
                    itemCreationType = 1;
                }

                if (ImGui::MenuItem("Level", nullptr))
                {
                    itemCreationType = 2;
                }

                if (ImGui::MenuItem("Texture3D", nullptr))
                {
                    itemCreationType = 3;
                }

                if (itemCreationType != -1)
                {
                    openCreateItemDialog = true;
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Rename", "F2", false, isTargetingAsset || isTargetingDirectory))
            {
                openRenameItemDialog = true;
            }

            if (ImGui::MenuItem("Remove", "DEL", false, isTargetingAsset || isTargetingDirectory))
            {
                std::filesystem::remove(m_SelectedItem->Path.string());

                Selection::Clear();
                m_SelectedItem = nullptr;

                Panels::AssetBrowser::RebuildAssetTree();
            }

            ImGui::EndPopup();
        }

        if (openRenameItemDialog)
            ImGui::OpenPopup("RenameItemDialog");
        if (openCreateItemDialog)
            ImGui::OpenPopup("CreateItemDialog");

        renderRenameDialog(openRenameItemDialog);
        createItemDialog(openCreateItemDialog);
    }
}

void Panels::AssetBrowser::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::AssetBrowser::GetActive()
{
    return m_Active;
}

void Panels::AssetBrowser::RebuildAssetTree()
{
    std::string restoreDirectory;

    m_SelectedItem = nullptr;

    if (m_SelectedEntry != nullptr)
    {
        restoreDirectory = m_SelectedEntry->Path.string();
        m_SelectedEntry = nullptr;
    }

    if (m_Root != nullptr)
    {
        m_Root->Dispose();

        delete m_Root;

        m_Root = nullptr;
    }

    Pine::Assets::RefreshAll();
    Pine::Assets::LoadDirectory("game/assets");

    // First update the icon cache, as we grab the icons from the IconStorage
    // as we fill out this tree.
    IconStorage::Update();

    m_Root = new PathEntry;

    m_Root->Type = EntryType::Directory;
    m_Root->Path = "game";

    MapDirectoryEntry("engine", "engine", m_Root);
    ProcessDirectoryTree("game/assets", m_Root);

    m_SelectedEntry = m_Root;

    if (!restoreDirectory.empty())
    {
        m_SelectedEntry = FindDirectoryEntry(m_Root, restoreDirectory);
    }
}

void Panels::AssetBrowser::Render()
{
    if (!ImGui::Begin(ICON_MD_FOLDER_OPEN " Asset Browser", &m_Active))
    {
        ImGui::End();

        return;
    } 

    if (ImGui::Button(ICON_MD_FOLDER_OPEN " Import"))
    {
        IconStorage::Update();
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_REFRESH " Refresh") || m_Root == nullptr)
    {
        RebuildAssetTree();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(220.f);

    ImGui::InputTextWithHint("##AssetSearch", ICON_MD_SEARCH " Search...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));

    ImGui::BeginChild("##Assets", ImVec2(-1.f, -1.f), true, 0);

    const bool isSearching = strlen(m_SearchBuffer) > 0;
    const int iconSizePadding = m_IconSize + 16;
    const float spaceAvailable = ImGui::GetContentRegionAvail().x - static_cast<float>(iconSizePadding * 2);
    const int nrColumns = static_cast<int>(spaceAvailable) / iconSizePadding;

    if (nrColumns == 0)
    {
        ImGui::EndChild();
        ImGui::End();
        
        return;
    }

    ImGui::Columns(nrColumns, nullptr, false);

    if (isSearching)
    {
        const std::string searchQuery = Pine::String::ToLower(m_SearchBuffer);

        if (const auto selectedEntry = RenderEntrySearch(m_SelectedEntry, searchQuery))
        {
            if (selectedEntry->Type == EntryType::File)
            {
                if (selectedEntry->Asset != nullptr)
                    Selection::Add<Pine::IAsset>(selectedEntry->Asset, true);
            }
        }
    }
    else 
    {
        bool rebuildTree = false;

        if (const auto newEntrySelection = RenderEntry(m_SelectedEntry, rebuildTree))
        {
            if (newEntrySelection->Type == EntryType::Directory)
            {
                m_SelectedEntry = newEntrySelection;
                m_SelectedItem = newEntrySelection;
            }
            else
            {
                if (newEntrySelection->Asset != nullptr)
                    Selection::Add<Pine::IAsset>(newEntrySelection->Asset, true);

                m_SelectedItem = newEntrySelection;
            }
        }

        if (rebuildTree)
            Panels::AssetBrowser::RebuildAssetTree();
    }

    ImGui::Columns(1);

    if (ImGui::IsWindowHovered())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Left))
        {
            Selection::Clear();

            m_SelectedItem = nullptr;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Right) && !ImGui::IsPopupOpen("AssetContextMenu"))
        {
            ImGui::OpenPopup("AssetContextMenu");

            m_SelectedItem = nullptr;
        }
    }

    RenderContextMenu();

    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget())
    {
        if (ImGui::AcceptDragDropPayload("Entity", ImGuiDragDropFlags_SourceAllowNullID))
        {
            const auto entity = *static_cast<Pine::Entity**>(ImGui::GetDragDropPayload()->Data);

            Pine::Blueprint blueprint;

            blueprint.CreateFromEntity(entity);
            blueprint.SetFilePath(m_SelectedEntry->Path.string() + "/" + entity->GetName() + ".bpt"); // surely the entity name is a valid file name :-)
            blueprint.SaveToFile();
            blueprint.Dispose();

            RebuildAssetTree();
        }

        ImGui::EndDragDropTarget();
    }

    if (!Selection::GetSelectedAssets().empty())
    {
        for (auto& asset : Selection::GetSelectedAssets())
            IconStorage::MarkIconDirty(asset->GetPath());
    }

    ImGui::End();
}
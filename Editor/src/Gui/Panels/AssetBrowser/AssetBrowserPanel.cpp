#include "AssetBrowserPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "Gui/Shared/IconStorage/IconStorage.hpp"

namespace
{

    // Items with the file extensions specifed below will not be shown in the user interface.
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

        Pine::Texture2D* Icon = nullptr;

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
					newEntry->Icon = IconStorage::GetIconTexture(newEntry->Asset->GetPath());
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

    PathEntry* RenderEntry(const PathEntry* entry)
    {
        static auto folderIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/folder.png");
        static auto fileIcon = Pine::Assets::Get<Pine::Texture2D>("editor/icons/file.png");

        PathEntry* clickedItem = nullptr;

        // Display directories first
        for (auto& directory : entry->Children)
        {
            if (directory->Type == EntryType::File)
                continue;

            if (Widgets::Icon(directory->DisplayText, folderIcon, false, m_IconSize))
            {
                if (directory->DisplayText == "...")
                    clickedItem = directory->Parent;
                else
                    clickedItem = directory;
            }

            ImGui::NextColumn();
        }

        for (auto& file : entry->Children)
        {
            if (file->Type == EntryType::Directory)
                continue;

            const auto isSelected = Selection::IsSelected(file->Asset);
            const auto icon = file->Icon == nullptr ? fileIcon : file->Icon;

            if (Widgets::Icon(file->DisplayText, icon, isSelected, m_IconSize))
            {
            	clickedItem = file;
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(*static_cast<std::uint64_t*>(icon->GetGraphicsTexture()->GetGraphicsIdentifier())), ImVec2(64.f, 64.f));
                ImGui::SameLine();
                ImGui::Text("%s", file->DisplayText.c_str());

                ImGui::SetDragDropPayload("Asset", &file->Asset, sizeof(Pine::IAsset*));

                ImGui::EndDragDropSource();
            }

            ImGui::NextColumn();
        }
        
        return clickedItem;
    }

    bool m_Active = true;
    char m_SearchBuffer[64];

    PathEntry* m_Root = nullptr;
    PathEntry* m_SelectedEntry = nullptr;
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

    // First update the icon cache, as we grab the icons from the IconStorage
    // as we fill out this tree.
    IconStorage::Update();

    m_Root = new PathEntry;

    m_Root->Type = EntryType::Directory;
    m_Root->Path = "game";

    MapDirectoryEntry("engine", "Engine", m_Root);
    ProcessDirectoryTree("game", m_Root);

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

    const int iconSizePadding = m_IconSize + 16;
    const float spaceAvailable = ImGui::GetContentRegionAvail().x - (iconSizePadding * 2);
    const int nrColumns = static_cast<int>(spaceAvailable) / iconSizePadding;

    if (nrColumns > 0 && m_SelectedEntry != nullptr)
    {
        ImGui::Columns(nrColumns, nullptr, false);

        if (const auto newEntrySelection = RenderEntry(m_SelectedEntry))
        {
            if (newEntrySelection->Type == EntryType::Directory)
            {
                m_SelectedEntry = newEntrySelection;
            }
            else 
            {
                Selection::Add<Pine::IAsset>(newEntrySelection->Asset, true);
            }
        }

        ImGui::Columns(1);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_::ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
        {
            Selection::Clear();
        }
    }


    ImGui::EndChild();

    ImGui::End();
}

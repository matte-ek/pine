#include "AssetHierarchy.hpp"

#include "Gui/Shared/IconStorage/IconStorage.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Projects/Projects.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"

namespace
{
    using namespace Editor::Gui::AssetHierarchy;

    void ProcessDirectory(Node* node, const std::filesystem::path& path, const std::string& relativeDir)
    {
        // Add a "go back" directory for the user first, displayed as "..."
        if (node->Parent != nullptr)
        {
            const auto backNode = new Node;

            backNode->Type = NodeType::Directory;
            backNode->Path = node->Parent->Path;
            backNode->Parent = node->Parent;
            backNode->DisplayText = "...";
            backNode->IsBackwardsDirectory = true;

            node->Children.push_back(backNode);
        }

        for (const auto& directoryEntry : std::filesystem::directory_iterator(path))
        {
            auto newNode = new Node;

            newNode->Type = directoryEntry.is_directory() ? NodeType::Directory : NodeType::File;
            newNode->Path = directoryEntry.path();
            newNode->Parent = node;
            newNode->DisplayText = directoryEntry.path().stem().filename().string();

            if (newNode->Type == NodeType::File)
            {
                if (newNode->Path.extension().string() != ".passet")
                {
                    delete newNode;
                    continue;
                }

                newNode->AssetPath = Editor::Utilities::Asset::EstimateMappedPath(newNode->Path, relativeDir);
                newNode->Asset = Pine::Assets::GetAssetByPath(newNode->AssetPath);

                if (newNode->Asset.Get())
                {
                    newNode->DisplayIcon = Editor::Gui::IconStorage::GetIconTexture(newNode->Asset->GetUId());
                }
            }
            else
            {
                ProcessDirectory(newNode, directoryEntry, relativeDir);
            }

            node->Children.push_back(newNode);
        }
    }
}

Node* Editor::Gui::AssetHierarchy::CreateRootNode()
{
    auto node = new Node{};

    node->Type = NodeType::Directory;
    node->Path = Projects::GetProjectPath() + "/assets";
    node->DisplayText = "root";

    return node;
}

Node* Editor::Gui::AssetHierarchy::FindDirectoryNode(Node* node, const std::filesystem::path& path)
{
    if (node->Path == path)
    {
        return node;
    }
    
    if (node->Type != NodeType::Directory || node->IsBackwardsDirectory)
    {
        return nullptr;
    }

    for (const auto& entry : node->Children)
    {
        if (const auto ret = FindDirectoryNode(entry, path))
        {
            return ret;
        }
    }

    return nullptr;
}

void Editor::Gui::AssetHierarchy::PopulateNode(Node* root, const std::filesystem::path& path, std::string_view relativeDir)
{
    ProcessDirectory(root, path, relativeDir.data());
}

void Editor::Gui::AssetHierarchy::PopulateMappedNode(Node* root, const std::filesystem::path& path, std::string_view name)
{
    auto node = new Node{};

    node->Type = NodeType::Directory;
    node->Path = path;
    node->DisplayText = name.data();
    node->Parent = root;

    ProcessDirectory(node, path, "");

    root->Children.push_back(node);
}

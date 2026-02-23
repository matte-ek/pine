#pragma once
#include <filesystem>

#include "Pine/Assets/Asset/Asset.hpp"

namespace Pine
{
    namespace Graphics
    {
        class ITexture;
    }

    class Asset;
}

namespace Editor::Gui::AssetHierarchy
{
    enum class NodeType
    {
        Directory,
        File
    };

    struct Node
    {
        // The underlying full path of the pine asset file or directory.
        std::filesystem::path Path;

        // The mapped path of the pine asset file
        std::string AssetPath;

        // The type of the node
        NodeType Type = NodeType::File;

        // Underlying asset handle, if any.
        Pine::AssetHandle<Pine::Asset> Asset;

        // How the node will be displayed in the asset browser.
        std::string DisplayText;
        Pine::Graphics::ITexture* DisplayIcon = nullptr;

        // Special case, this node does not really exist on disk but is used to provide the
        //  "..." folder functionally in the asset browser, to go back to the parent directory.
        bool IsBackwardsDirectory = false;

        // Relation to other nodes
        Node* Parent = nullptr;
        std::vector<Node*> Children;

        void Dispose() const
        {
            for (const auto child : Children)
            {
                child->Dispose();
                delete child;
            }
        }
    };

    Node* CreateRootNode();

    Node* FindDirectoryNode(Node* node, const std::filesystem::path& path);

    void PopulateNode(Node* root, const std::filesystem::path& path, std::string_view relativeDir = "");
    void PopulateMappedNode(Node* root, const std::filesystem::path& path, std::string_view name);
}

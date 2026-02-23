#pragma once

namespace Editor::Gui::AssetHierarchy
{
    struct Node;
}

namespace Panels::AssetBrowser
{
    void SetActive(bool value);
    bool GetActive();

    Editor::Gui::AssetHierarchy::Node* GetRootNode();
    Editor::Gui::AssetHierarchy::Node* GetOpenDirectoryNode();
    Editor::Gui::AssetHierarchy::Node* GetSelectedNode();

    void BuildAssetHierarchy();

    void Render();
}

#pragma once
#include "Pine/Assets/Texture2D/Texture2D.hpp"

namespace Editor::Gui::AssetHierarchy
{
    struct AssetIcon;
}

namespace Editor::Gui::IconStorage
{
    void Setup();
    void Dispose();
	void Update();

    void MarkIconDirty(Pine::UId id);

	AssetHierarchy::AssetIcon GetIconTexture(Pine::UId id);

    Pine::Graphics::ITexture* GetPreviewTexture();
    void HandlePreviewDragging();
}

#pragma once

namespace Panels::AssetBrowser
{
    void SetActive(bool value);
    bool GetActive();

    void RebuildAssetTree();

    void Render();
}
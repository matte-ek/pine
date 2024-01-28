#pragma once

namespace Commands
{
    void Setup();
    void Dispose();
    void Update();

    void Copy();
    void Paste();
    void Duplicate();
    void Delete();

    void Undo();
    void Redo();

    void Refresh(bool engineAssets = false);
    void Save();
}
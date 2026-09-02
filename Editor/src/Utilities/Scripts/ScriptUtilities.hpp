#pragma once
#include <string>

namespace Editor::Utilities::Script
{
    void Setup();

    // Writes a new C# script source file (from the built-in template) at csFilePath, with the
    // given managed class name substituted in. The file is picked up by the project's
    // Game.csproj (which globs assets/**/*.cs) and compiled into the game assembly.
    void CreateScriptSource(const std::string& csFilePath, const std::string& className);

    // Removes the sibling '.cs' source for a script asset, given its '.passet' path.
    void DeleteScript(const std::string& passetFilePath);
}

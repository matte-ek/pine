#pragma once

#include <string>
#include <vector>

namespace Editor::Gui::Dialog::AssetImport
{
    // Takes the files and directories the user dropped onto the editor window. Nothing is resolved
    // or imported here: the drop callback runs inside glfwPollEvents(), which is no place to be
    // compiling textures. Render() picks the paths up on the next frame.
    void Queue(const std::vector<std::string>& paths);

    // Drives the dialog: builds the plan for anything queued, shows it, and runs the import the
    // user agreed to. Call once per frame from the editor's ImGui pass, outside any window.
    void Render();
}

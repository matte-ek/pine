#pragma once
#include <string>

namespace Editor::Projects
{
    void SetProject(const std::string& name);

    const std::string& GetProjectName();
    std::string GetProjectPath();

    void LoadProjectAssets();
}
#include "Projects.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Performance/Performance.hpp"

namespace
{
    std::string m_ProjectName;
}

void Editor::Projects::SetProject(const std::string& name)
{
    m_ProjectName = name;

    Pine::Assets::SetWorkingDirectory(GetProjectPath() + "/assets");
}

const std::string& Editor::Projects::GetProjectName()
{
    return m_ProjectName;
}

std::string Editor::Projects::GetProjectPath()
{
    return "projects/" + m_ProjectName;
}

void Editor::Projects::LoadProjectAssets()
{
    {
        PINE_PF_SCOPE_MANUAL("Editor::LoadProjectAssets");
        Pine::Assets::LoadAssetsFromDirectory("");
    }
}



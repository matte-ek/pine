#include "Projects.hpp"

namespace
{
    std::string m_ProjectName;
}

void Editor::Projects::SetProject(const std::string& name)
{
    m_ProjectName = name;
}

const std::string& Editor::Projects::GetProjectName()
{
    return m_ProjectName;
}

std::string Editor::Projects::GetProjectPath()
{
    return "projects/" + m_ProjectName;
}



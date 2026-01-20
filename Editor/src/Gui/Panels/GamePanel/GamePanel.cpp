#include "GamePanel.hpp"

#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Game/Game.hpp"

namespace
{
    bool m_Active = true;

    Pine::Game::GameProperties m_GameProperties;

    char m_NameBuffer[128];
    char m_VersionBuffer[128];
    char m_AuthorBuffer[128];

    Pine::Level* m_StartupLevel = nullptr;
}

void Panels::Game::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Game::GetActive()
{
    return m_Active;
}

void Panels::Game::Setup()
{
    m_GameProperties = Pine::Game::GetGameProperties();

    strcpy_s(m_NameBuffer, sizeof(m_NameBuffer), m_GameProperties.Name.c_str());
    strcpy_s(m_VersionBuffer, sizeof(m_VersionBuffer), m_GameProperties.Version.c_str());
    strcpy_s(m_AuthorBuffer, sizeof(m_AuthorBuffer), m_GameProperties.Author.c_str());

    m_StartupLevel = Pine::Assets::Get<Pine::Level>(m_GameProperties.StartupLevel);
}

void Panels::Game::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_SPORTS_ESPORTS " Game Properties", &m_Active))
    {
        if (Widgets::InputText("Name", m_NameBuffer, sizeof(m_NameBuffer)))
        {
            m_GameProperties.Name = m_NameBuffer;
        }

        if (Widgets::InputText("Author", m_AuthorBuffer, sizeof(m_AuthorBuffer)))
        {
            m_GameProperties.Author = m_AuthorBuffer;
        }

        if (Widgets::InputText("Version", m_VersionBuffer, sizeof(m_VersionBuffer)))
        {
            m_GameProperties.Version = m_VersionBuffer;
        }

        auto [hasResult, newStartupLevel] = Widgets::AssetPicker("Startup Level", m_StartupLevel, Pine::AssetType::Level);
        if (hasResult)
        {
            m_StartupLevel = dynamic_cast<Pine::Level*>(newStartupLevel);
            m_GameProperties.StartupLevel = newStartupLevel->GetPath();
        }

        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(150, 40)))
        {
            nlohmann::json j;

            j["name"] = m_GameProperties.Name;
            j["author"] = m_GameProperties.Author;
            j["version"] = m_GameProperties.Version;
            j["startupLevel"] = m_GameProperties.StartupLevel;

            Pine::Serialization::SaveToFile("game.json", j);
        }

        ImGui::End();
    }
}

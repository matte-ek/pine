#include "GamePanel.hpp"

#include <imgui.h>
#include <fmt/format.h>

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

    char m_TagsBuffer[128][64];
    char m_LayersBuffer[128][32];

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

    for (int i = 0; i < 64;i++)
    {
        strcpy_s(m_TagsBuffer[i], 128, m_GameProperties.EntityTags[i].c_str());
    }

    for (int i = 0; i < 31;i++)
    {
        strcpy_s(m_LayersBuffer[i], 128, m_GameProperties.ColliderLayers[i].c_str());
    }

    strcpy_s(m_LayersBuffer[31], 128, "Default");

    m_StartupLevel = Pine::Assets::Get<Pine::Level>(m_GameProperties.StartupLevel);
}

void Panels::Game::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_SPORTS_ESPORTS " Game Properties", &m_Active))
    {
        if (ImGui::CollapsingHeader("Meta"))
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
        }

        if (ImGui::CollapsingHeader("Tags"))
        {
            for (int i = 0; i < 64;i++)
            {
                if (Widgets::InputText(fmt::format("Tag #{}", i + 1), m_TagsBuffer[i], sizeof(m_TagsBuffer[i])))
                {
                    m_GameProperties.EntityTags[i] = m_TagsBuffer[i];
                }
            }
        }

        if (ImGui::CollapsingHeader("Layers"))
        {
            Widgets::PushDisabled();
            Widgets::InputText("Layer #1", m_LayersBuffer[31], sizeof(m_LayersBuffer[31]));
            Widgets::PopDisabled();

            for (int i = 0; i < 31;i++)
            {
                if (Widgets::InputText(fmt::format("Layer #{}", i + 2), m_LayersBuffer[i], sizeof(m_LayersBuffer[i])))
                {
                    m_GameProperties.ColliderLayers[i] = m_LayersBuffer[i];
                }
            }
        }

        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(150, 40)))
        {
            nlohmann::json j;

            j["name"] = m_GameProperties.Name;
            j["author"] = m_GameProperties.Author;
            j["version"] = m_GameProperties.Version;

            for (int i = 0; i < 64;i++)
            {
                j[fmt::format("tag{}", i)] = m_GameProperties.EntityTags[i];
            }

            for (int i = 0; i < 31;i++)
            {
                j[fmt::format("layer{}", i)] = m_GameProperties.ColliderLayers[i];
            }

            j["startupLevel"] = m_GameProperties.StartupLevel;

            Pine::Serialization::SaveToFile("game.json", j);
            Pine::Game::SetGameProperties(m_GameProperties);
        }
    }
    ImGui::End();
}

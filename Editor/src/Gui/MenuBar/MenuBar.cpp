#include "MenuBar.hpp"
#include "imgui.h"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Panels/Console/ConsolePanel.hpp"
#include "Gui/Panels/DebugPanel/DebugPanel.hpp"
#include "Gui/Panels/EntityList/EntityListPanel.hpp"
#include "Gui/Panels/GameViewport/GameViewportPanel.hpp"
#include "Gui/Panels/LevelViewport/LevelViewportPanel.hpp"
#include "Gui/Panels/Profiler/ProfilerPanel.hpp"
#include "Gui/Panels/Properties/PropertiesPanel.hpp"
#include "Gui/Shared/Commands/Commands.hpp"

void MenuBar::Setup()
{

}

void MenuBar::Render()
{

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {

            }

            if (ImGui::MenuItem("Save", "CTRL+S"))
            {
                Commands::Save();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {

            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Copy", "CTRL+C"))
            {
                Commands::Copy();
            }

            if (ImGui::MenuItem("Paste", "CTRL+V"))
            {
                Commands::Paste();
            }

            if (ImGui::MenuItem("Cut", "CTRL+X"))
            {
                Commands::Copy();
                Commands::Delete();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "DEL"))
            {
                Commands::Delete();
            }

            if (ImGui::MenuItem("Duplicate", "CTRL+D"))
            {
                Commands::Duplicate();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Undo", "CTRL+Z"))
            {
                Commands::Undo();
            }

            if (ImGui::MenuItem("Redo", "CTRL+Y"))
            {
                Commands::Redo();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Refresh", "F5"))
            {
                Commands::Refresh();
            }

            if (ImGui::MenuItem("Refresh Engine Assets", "CTRL+F5"))
            {
                Commands::Refresh(true);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows"))
        {
            if (ImGui::MenuItem("Asset Browser", nullptr, Panels::AssetBrowser::GetActive()))
            {
                Panels::AssetBrowser::SetActive(!Panels::AssetBrowser::GetActive());
            }

            if (ImGui::MenuItem("Entity list", nullptr, Panels::EntityList::GetActive()))
            {
                Panels::EntityList::SetActive(!Panels::EntityList::GetActive());
            }

            if (ImGui::MenuItem("Properties", nullptr, Panels::Properties::GetActive()))
            {
                Panels::Properties::SetActive(!Panels::Properties::GetActive());
            }

            if (ImGui::MenuItem("Game View", nullptr, Panels::GameViewport::GetActive()))
            {
                Panels::GameViewport::SetActive(!Panels::GameViewport::GetActive());
            }

            if (ImGui::MenuItem("Level View", nullptr, Panels::LevelViewport::GetActive()))
            {
                Panels::LevelViewport::SetActive(!Panels::LevelViewport::GetActive());
            }

            if (ImGui::MenuItem("Console", nullptr, Panels::Console::GetActive()))
            {
                Panels::Console::SetActive(!Panels::Console::GetActive());
            }

            if (ImGui::MenuItem("Profiler", nullptr, Panels::Profiler::GetActive()))
            {
                Panels::Profiler::SetActive(!Panels::Profiler::GetActive());
            }

            if (ImGui::MenuItem("Debug", nullptr, Panels::Debug::GetActive()))
            {
                Panels::Debug::SetActive(!Panels::Debug::GetActive());
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

}

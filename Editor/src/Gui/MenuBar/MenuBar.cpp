#include "MenuBar.hpp"
#include "imgui.h"
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

        ImGui::EndMainMenuBar();
    }

}

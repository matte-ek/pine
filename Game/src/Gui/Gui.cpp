#include "Gui.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui.h"
#include "LevelEditor/LevelEditor.hpp"

#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    bool m_IsInEditor = false;

    void InitializeImGui()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsClassic();

        ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer()), true);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    void ShutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void RenderDebugMenu()
    {
        static int selectedGameLevel = 0;

        ImGui::Begin("Game Debug Menu");

        ImGui::Text("Level:");

        ImGui::SetNextItemWidth(100.f);
    	ImGui::Combo("##GameLevel", &selectedGameLevel, "Level 1\0Level 2\0Level 3\0");

        if (ImGui::Button("Load Level"))
        {

        }

        if (ImGui::Button("Start Editor"))
        {
            LevelEditor::SetLevel(selectedGameLevel);

        	m_IsInEditor = true;
        }

        ImGui::End();
    }

    void OnPineRender(Pine::RenderStage stage)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (m_IsInEditor)
        {
            LevelEditor::Render();
        }
        else
        {
            RenderDebugMenu();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void Gui::Initialize()
{
    InitializeImGui();

    Pine::RenderManager::AddRenderCallback(OnPineRender);
}

void Gui::Shutdown()
{
    ShutdownImGui();
}

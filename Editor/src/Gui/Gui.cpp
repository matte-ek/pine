#include "Gui.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui.h"

#include "Panels/Panels.hpp"

namespace
{
    void InitializeImGui()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(Pine::WindowManager::GetWindowPointer(), true);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    void ShutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void OnPineRender(Pine::RenderStage stage)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        Panels::ShowViewports();
        Panels::ShowEntityList();
        Panels::ShowViewports();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void Gui::Setup()
{
    InitializeImGui();

    Pine::RenderManager::AddRenderCallback(OnPineRender);
}

void Gui::Shutdown()
{
    ShutdownImGui();
}

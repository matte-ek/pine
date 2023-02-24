#include "Gui.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui.h"
#include "IconsMaterialDesign.h"

#include "Panels/Properties/PropertiesPanel.hpp"
#include "Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Panels/EntityList/EntityListPanel.hpp"
#include "Panels/GameViewport/GameViewportPanel.hpp"
#include "Panels/LevelViewport/LevelViewportPanel.hpp"

namespace
{
    void SetTheme()
    {
        // TODO: Do a nice green dark theme
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(4, 3);
        style.CellPadding = ImVec2(4, 2);
        style.ItemSpacing = ImVec2(4, 5);
        style.GrabMinSize = 8.f;
        style.FrameRounding = 4.f;

        ImGui::StyleColorsDark();
    }

    void SetFonts()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Add normal font
        io.Fonts->AddFontFromFileTTF("editor/fonts/NotoSans-Regular.ttf", 17.f);

        // Merge in Material Icons
        static const ImWchar icons_ranges[] = {ICON_MIN_MD, ICON_MAX_16_MD, 0};

        ImFontConfig icons_config;

        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphOffset = ImVec2(0, 3.f);

        io.Fonts->AddFontFromFileTTF("editor/fonts/MaterialIcons-Regular.ttf", 16.0f, &icons_config, icons_ranges);
    }

    void InitializeImGui()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        SetFonts();
        SetTheme();

        ImGui_ImplGlfw_InitForOpenGL(reinterpret_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer()), true);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    void ShutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void CreateDockSpace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        // Force the window to be fullscreen
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));
        ImGui::SetNextWindowViewport(viewport->ID);

        // Make sure to remove any other visual stuff
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("PineDockSpaceWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        ImGui::PopStyleVar(3);

        const ImGuiID dockSpaceID = ImGui::GetID("DockSpace");

        ImGui::DockSpace(dockSpaceID, ImVec2(0.0f, 0.f), 0);

        ImGui::End();
    }

    void OnPineRender(Pine::RenderStage stage)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        CreateDockSpace();

        ImGui::ShowDemoWindow();

        Panels::GameViewport::Render();
        Panels::LevelViewport::Render();
        Panels::EntityList::Render();
        Panels::AssetBrowser::Render();
        Panels::Properties::Render();

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

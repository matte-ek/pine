#include "Gui.hpp"
#include "IconsMaterialDesign.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

#include "Gui/MenuBar/MenuBar.hpp"
#include "Gui/Shared/Commands/Commands.hpp"
#include "Gui/Shared/Gizmo/Gizmo2D/Gizmo2D.hpp"
#include "Gui/Shared/Gizmo/Gizmo3D/Gizmo3D.hpp"
#include "Gui/Shared/IconStorage/IconStorage.hpp"
#include "Other/EntitySelection/EntitySelection.hpp"
#include "Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Panels/Console/ConsolePanel.hpp"
#include "Panels/DebugPanel/DebugPanel.hpp"
#include "Panels/Engine/EngineAssetsPanel.hpp"
#include "Panels/EntityList/EntityListPanel.hpp"
#include "Panels/GamePanel/GamePanel.hpp"
#include "Panels/GameViewport/GameViewportPanel.hpp"
#include "Panels/LevelPanel/LevelPanel.hpp"
#include "Panels/LevelViewport/LevelViewportPanel.hpp"
#include "Panels/Profiler/ProfilerPanel.hpp"
#include "Panels/Properties/PropertiesPanel.hpp"
#include "Pine/Performance/Performance.hpp"

namespace
{
    void OnWindowFocus()
    {
        if (!Pine::WindowManager::GetWindowPointer())
            return;

        ImGui_ImplGlfw_WindowFocusCallback(static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer()), true);
    }

    void SetTheme()
    {
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(8, 4);
        style.CellPadding = ImVec2(5, 5);
        style.ItemSpacing = ImVec2(4, 4);
        style.GrabMinSize = 8.f;
        style.FrameRounding = 4.f;
        style.GrabRounding = 4.f;
        style.ScrollbarSize = 10.f;
        style.PopupRounding = 4.f;
        style.TabRounding = 4.f;

        ImGui::StyleColorsDark();

        ImVec4* colors = ImGui::GetStyle().Colors;

        colors[ImGuiCol_Text] = ImVec4(0.87f, 0.98f, 0.92f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.07f, 0.08f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.05f, 0.08f, 0.07f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.13f, 0.12f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.11f, 0.15f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.07f, 0.10f, 0.09f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.04f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.13f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.07f, 0.07f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.22f, 0.21f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.16f, 0.22f, 0.21f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.75f, 0.45f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.75f, 0.45f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.75f, 0.45f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.11f, 0.27f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.19f, 0.17f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.11f, 0.27f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.05f, 0.08f, 0.07f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.58f, 0.98f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.11f, 0.10f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.09f, 0.21f, 0.19f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.10f, 0.14f, 0.13f, 1.00f);
    }

    void SetFonts()
    {
        const ImGuiIO& io = ImGui::GetIO();

        // Add normal font
        io.Fonts->AddFontFromFileTTF("editor/fonts/NotoSans-Regular.ttf", 18.f);

        // Merge in Material Icons
        static constexpr ImWchar icons_ranges[] = {ICON_MIN_MD, ICON_MAX_16_MD, 0};

        ImFontConfig icons_config;

        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphOffset = ImVec2(0, 3.f);

        io.Fonts->AddFontFromFileTTF("editor/fonts/MaterialIcons-Regular.ttf", 17.0f, &icons_config, icons_ranges);
    }

    void InitializeImGui()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        SetFonts();
        SetTheme();

        ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(Pine::WindowManager::GetWindowPointer()), true);
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

    void OnPineRender(Pine::RenderingContext*, Pine::RenderStage stage, float deltaTime)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        PINE_PF_SCOPE_MANUAL("Editor::OnPineRender()");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        CreateDockSpace();

        MenuBar::Render();

        ImGui::ShowDemoWindow();

        Panels::GameViewport::Render();
        Panels::LevelViewport::Render();
        Panels::EntityList::Render();
        Panels::AssetBrowser::Render();
        Panels::Properties::Render();
        Panels::LevelPanel::Render();
        Panels::Console::Render();
        Panels::Profiler::Render();
        Panels::EngineAssetsPanel::Render();
        Panels::Debug::Render();
        Panels::Game::Render();

        Editor::Commands::Update();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void Editor::Gui::Setup()
{
    InitializeImGui();

    Pine::WindowManager::InstallWindowCallbacks();

    Pine::WindowManager::AddWindowFocusCallback(OnWindowFocus);

    Commands::Setup();
    Gizmo::Gizmo2D::Setup();
    Gizmo::Gizmo3D::Setup();
    EntitySelection::Setup();
    IconStorage::Setup();

    Panels::Game::Setup();

    Pine::RenderManager::AddRenderCallback(OnPineRender);
}

void Editor::Gui::Shutdown()
{
    Commands::Dispose();
    EntitySelection::Dispose();
    IconStorage::Dispose();

    ShutdownImGui();
}

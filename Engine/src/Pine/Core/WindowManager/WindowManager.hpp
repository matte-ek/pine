#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../Math/Math.hpp"

// Handles OS window creation, modification etc.
// Pine only supports one window at the time, don't see any reason
// for there to be more windows at the moment.

namespace Pine::WindowManager
{
    struct Monitor
    {
        // Underlying pointer within GLFW, usually
        // shouldn't have to be used by the user.
        void* m_MonitorPointer = nullptr;

        // Position and size, position has an offset depending on the monitor location
        Vector2i m_Position;
        Vector2i m_Size;

        // Monitor string, example "Monitor #0"
        std::string m_Text;

        // If the monitor is set as the "Primary" monitor within the OS
        bool m_IsPrimaryMonitor = false;

        // If our windows is on this monitor, this is currently only available
        // if the screen type is set to FullscreenWindowed or Fullscreen
        bool m_IsPineWindowMonitor = false;
    };

    enum class ScreenType
    {
        Default,
        FullscreenWindowed,
        Fullscreen
    };

    // Functions that should be called by the engine itself only
    namespace Internal
    {
        bool CreateWindow(Vector2i position, Vector2i size, const std::string& title, ScreenType type = ScreenType::Default);
        void DestroyWindow();
    }

    bool IsWindowCreated();
    bool IsWindowOpen();

    void SetWindowPosition(Vector2i position);
    void SetWindowSize(Vector2i size);
    void SetWindowTitle(const std::string& title);
    void SetWindowVisible(bool visible);
    void SetWindowScreenType(ScreenType screenType);

    Vector2i GetWindowPosition();
    Vector2i GetWindowSize();
    bool GetWindowVisible();
    const std::string& GetWindowTitle();
    ScreenType GetWindowScreenType();

    // Callbacks
    void InstallWindowCallbacks(); // This is normally called by the engine itself, but ImGui might steal some of them, so we need to call this again after ImGui has been initialized.
    void AddWindowResizeCallback(const std::function<void(int, int)>& callback);
    void AddWindowFocusCallback(const std::function<void()>& callback);
    void AddWindowDropCallback(const std::function<void(std::vector<std::string>)>& callback);

    // Pointer to the underlying OS "handle", for example, on Windows
    // systems this will be the HWND.
    void* GetWindowHandlePointer();

    // Pointer to the underlying GLFW window
    void* GetWindowPointer();

}
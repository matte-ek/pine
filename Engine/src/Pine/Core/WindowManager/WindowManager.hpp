#pragma once
#include <string>
#include <vector>
#include "../Math/Math.hpp"

// Handles OS window creation, modification etc.
// Pine only supports one window at the time, don't see any reason
// for there to be more windows at the moment.

struct GLFWwindow;

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

    // Managing the window obviously requires IsWindowCreated() to return true.

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

    // Pointer to the underlying OS "handle", for example, on Windows
    // systems this will be the HWND.
    void* GetWindowHandlePointer();

    // Pointer to the underlying GLFW window
    GLFWwindow* GetWindowPointer();

}
#pragma once
#include <string>
#include <vector>
#include "../Math/Vector2/Vector2.hpp"

// Handles OS window creation, modification etc.
// Pine only supports one window at the time, don't see any reason
// for there to be more windows at the moment.

class GLFWwindow;

namespace Pine::WindowManager
{
    struct Monitor
    {
        // Underlying pointer within GLFW, usually
        // shouldn't have to be used by the user.
        void* m_MonitorPointer = nullptr;

        // Position and size, position has an offset depending on the monitor location
        Math::Vector2i m_Position;
        Math::Vector2i m_Size;

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
        bool CreateWindow(Math::Vector2i position, Math::Vector2i size, const std::string& title, ScreenType type = ScreenType::Default);
        void DestroyWindow();
    }

    bool IsWindowCreated();
    bool IsWindowOpen();

    // Managing the window obviously requires IsWindowCreated() to return true.

    void SetWindowPosition(Math::Vector2i position);
    void SetWindowSize(Math::Vector2i size);
    void SetWindowTitle(const std::string& title);
    void SetWindowVisible(bool visible);
    void SetWindowScreenType(ScreenType screenType);

    Math::Vector2i GetWindowPosition();
    Math::Vector2i GetWindowSize();
    bool GetWindowVisible();
    const std::string& GetWindowTitle();
    ScreenType GetWindowScreenType();

    // Pointer to the underlying OS "handle", for example, on Windows
    // systems this will be the HWND.
    void* GetWindowHandlePointer();

    // Pointer to the underlying GLFW window
    GLFWwindow* GetWindowPointer();

}